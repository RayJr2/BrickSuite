#include "BrickSuitePairingService.h"

#include "BrickSuiteAuthentication.h"
#include "../services/CredentialStore.h"

#include <QUuid>
#include <QDebug>

BrickSuitePairingService::BrickSuitePairingService(
    PairedDeviceRegistry& registry, Clock clock, WriteCredential write, RemoveCredential remove)
    : m_registry(registry)
    , m_clock(clock ? std::move(clock) : [] { return QDateTime::currentDateTimeUtc(); })
    , m_write(write ? std::move(write) : [](const QString& name, const QString& value, QString* error) {
        return CredentialStore::write(name, value, error);
    })
    , m_remove(remove ? std::move(remove) : [](const QString& name, QString* error) {
        return CredentialStore::remove(name, error);
    })
{
}

QDateTime BrickSuitePairingService::now() const { return m_clock().toUTC(); }

void BrickSuitePairingService::expireIfNeeded()
{
    if (m_attempt.active && now() >= m_attempt.expiresUtc) cancel();
}

BrickSuitePairingService::Attempt BrickSuitePairingService::start(QString* error)
{
    QString randomError;
    const QByteArray bytes = BrickSuiteAuthentication::secureRandom(8, &randomError);
    if (bytes.size() != 8) {
        if (error) *error = randomError;
        return {};
    }
    const QString hex = QString::fromLatin1(bytes.toHex().toUpper());
    m_attempt = {true, hex.mid(0, 4) + QLatin1Char('-') + hex.mid(4, 4)
        + QLatin1Char('-') + hex.mid(8, 4) + QLatin1Char('-') + hex.mid(12, 4),
        now().addSecs(ExpirationSeconds), 0, {}};
    return m_attempt;
}

QString BrickSuitePairingService::normalizedCode(const QString& code, bool* valid)
{
    QString compact = code.trimmed();
    bool formatValid = compact.size() == 16;
    if (compact.size() == 19) {
        formatValid = compact.at(4) == QLatin1Char('-')
            && compact.at(9) == QLatin1Char('-')
            && compact.at(14) == QLatin1Char('-');
        if (formatValid) {
            compact.remove(14, 1);
            compact.remove(9, 1);
            compact.remove(4, 1);
        }
    }
    if (formatValid) {
        for (const QChar character : compact) {
            const QChar upper = character.toUpper();
            if ((character < QLatin1Char('0') || character > QLatin1Char('9'))
                && (upper < QLatin1Char('A') || upper > QLatin1Char('F'))) {
                formatValid = false;
                break;
            }
        }
    }
    if (valid) *valid = formatValid;
    if (!formatValid) return {};
    compact = compact.toUpper();
    return compact.mid(0, 4) + QLatin1Char('-') + compact.mid(4, 4)
        + QLatin1Char('-') + compact.mid(8, 4) + QLatin1Char('-') + compact.mid(12, 4);
}

void BrickSuitePairingService::cancel() { m_attempt = {}; }

BrickSuitePairingService::Attempt BrickSuitePairingService::attempt() const
{
    auto* self = const_cast<BrickSuitePairingService*>(this);
    self->expireIfNeeded();
    return m_attempt;
}

QString BrickSuitePairingService::credentialReference(const QString& deviceId)
{
    return QStringLiteral("BrickSuitePairedDevice/%1").arg(deviceId.toLower());
}

BrickSuitePairingService::Result BrickSuitePairingService::pair(
    const QString& code, const QString& friendlyName, const QString& clientVersion,
    const QString& platform)
{
    expireIfNeeded();
    const QDateTime current = now();
    if (!m_attempt.active)
        return {false, {}, {}, QStringLiteral("PAIRING_DISABLED"),
                QStringLiteral("Host pairing is disabled or expired.")};
    if (m_attempt.retryUtc.isValid() && current < m_attempt.retryUtc)
        return {false, {}, {}, QStringLiteral("PAIRING_RATE_LIMITED"),
                QStringLiteral("Wait before trying the pairing code again.")};
    const QByteArray expected = m_attempt.code.toUtf8();
    const QByteArray supplied = normalizedCode(code).toUtf8();
    if (!BrickSuiteAuthentication::constantTimeEquals(expected, supplied)) {
        ++m_attempt.failures;
        if (m_attempt.failures >= MaximumFailures) {
            cancel();
            return {false, {}, {}, QStringLiteral("PAIRING_ATTEMPTS_EXCEEDED"),
                    QStringLiteral("Pairing was disabled after too many failed attempts.")};
        }
        m_attempt.retryUtc = current.addSecs(m_attempt.failures);
        return {false, {}, {}, QStringLiteral("PAIRING_CODE_INVALID"),
                QStringLiteral("The pairing code is invalid.")};
    }
    const QString name = friendlyName.trimmed();
    if (name.isEmpty() || name.size() > 80)
        return {false, {}, {}, QStringLiteral("INVALID_REQUEST"),
                QStringLiteral("Enter a device name of 1 to 80 characters.")};

    // Consume before writing durable state so two calls cannot both enroll.
    cancel();
    const QString deviceId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QString credentialError;
    const QString credential = BrickSuiteAuthentication::generateAccessToken(&credentialError);
    const QString reference = credentialReference(deviceId);
    if (credential.isEmpty() || !m_write(reference, credential, &credentialError))
        return {false, {}, {}, QStringLiteral("CREDENTIAL_STORE_FAILED"),
                credentialError.isEmpty() ? QStringLiteral("Unable to store the paired-device credential securely.")
                                          : credentialError};
    const QString timestamp = current.toString(Qt::ISODateWithMs);
    PairedDeviceRecord record{deviceId, name, timestamp, timestamp,
                              clientVersion.left(40), platform.left(40), reference, true};
    QString registryError;
    if (!m_registry.add(record, &registryError)) {
        m_remove(reference, nullptr);
        return {false, {}, {}, QStringLiteral("PAIRING_PERSISTENCE_FAILED"), registryError};
    }
    qInfo().noquote() << "BrickSuite paired device created" << deviceId << name;
    return {true, deviceId, credential, {}, {}};
}
