#include "../src/network/BrickSuitePairingService.h"
#include "../src/network/PairedDeviceRegistry.h"

#include <QCoreApplication>
#include <QFile>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include <iostream>

namespace {
bool check(bool condition, const char* message)
{
    if (!condition) std::cerr << "FAIL: " << message << '\n';
    return condition;
}
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    bool ok = true;
    QTemporaryDir temporary;
    ok &= check(temporary.isValid(), "temporary directory created");
    const QString registryPath = temporary.filePath(QStringLiteral("server/paired-devices.json"));
    PairedDeviceRegistry registry(registryPath);
    QString error;
    ok &= check(registry.load(&error), "missing registry loads as empty");

    const QString canonical = QStringLiteral("6001-D8C3-1EBB-E3E1");
    const QStringList equivalentCodes{
        canonical,
        QStringLiteral("6001D8C31EBBE3E1"),
        QStringLiteral("6001-d8c3-1ebb-e3e1"),
        QStringLiteral("6001d8c31ebbe3e1"),
        QStringLiteral("6001-d8C3-1eBb-E3e1"),
        QStringLiteral("  6001-D8C3-1EBB-E3E1  ")};
    for (const QString& candidate : equivalentCodes) {
        bool valid = false;
        ok &= check(BrickSuitePairingService::normalizedCode(candidate, &valid) == canonical
                        && valid,
                    "equivalent pairing code normalizes canonically");
    }
    const QStringList malformedCodes{
        QString(), QStringLiteral("6001-D8C3-1EBB"),
        QStringLiteral("6001-D8C3-1EBB-E3E11"),
        QStringLiteral("6001-D8C3-1EBB-E3G1"),
        QStringLiteral("6001--D8C3-1EBB-E3E1"),
        QStringLiteral("6001-D8C31-EBB-E3E1"),
        QStringLiteral("6001 D8C3 1EBB E3E1"),
        QStringLiteral("6001.D8C3.1EBB.E3E1"),
        QStringLiteral("6001-D8C3-1EBB-")};
    for (const QString& candidate : malformedCodes) {
        bool valid = true;
        ok &= check(BrickSuitePairingService::normalizedCode(candidate, &valid).isEmpty()
                        && !valid,
                    "malformed pairing code rejected");
    }

    QDateTime now = QDateTime::fromString(QStringLiteral("2026-09-12T12:00:00.000Z"),
                                          Qt::ISODateWithMs);
    QHash<QString, QString> credentials;
    BrickSuitePairingService service(registry, [&] { return now; },
        [&](const QString& key, const QString& value, QString*) {
            credentials.insert(key, value);
            return true;
        },
        [&](const QString& key, QString*) { credentials.remove(key); return true; });

    ok &= check(service.pair(QStringLiteral("AAAA"), QStringLiteral("Device"), {}, {}).errorCode
                    == QStringLiteral("PAIRING_DISABLED"),
                "disabled pairing rejects enrollment");
    const auto first = service.start(&error);
    ok &= check(first.active && first.code.size() == 19 && first.code.count(QLatin1Char('-')) == 3,
                "pairing creates a formatted 64-bit code");
    const QString firstCode = first.code;
    auto wrong = service.pair(QStringLiteral("0000-0000-0000-0000"),
                              QStringLiteral("Device"), QStringLiteral("0.4.0"),
                              QStringLiteral("Windows"));
    ok &= check(!wrong.success && wrong.errorCode == QStringLiteral("PAIRING_CODE_INVALID"),
                "wrong code rejected");
    ok &= check(service.pair(firstCode, QStringLiteral("Device"), {}, {}).errorCode
                    == QStringLiteral("PAIRING_RATE_LIMITED"),
                "retry backoff enforced");
    now = now.addSecs(1);
    const QString unformattedCorrectCode = QString(firstCode).remove(QLatin1Char('-')).toLower();
    const auto paired = service.pair(unformattedCorrectCode, QStringLiteral("Living Room PC"),
                                     QStringLiteral("0.4.0"), QStringLiteral("Windows"));
    ok &= check(paired.success && !paired.deviceId.isEmpty() && paired.credential.size() >= 43,
                "valid code creates a device and 256-bit credential");
    ok &= check(!service.attempt().active, "successful code consumed");
    ok &= check(service.pair(firstCode, QStringLiteral("Second"), {}, {}).errorCode
                    == QStringLiteral("PAIRING_DISABLED"),
                "consumed code cannot be reused through alternate formatting or case");
    const auto record = registry.find(paired.deviceId);
    ok &= check(record && record->friendlyName == QStringLiteral("Living Room PC")
                    && credentials.value(record->credentialReference) == paired.credential,
                "registry metadata and separate credential reference persisted");

    QFile registryFile(registryPath);
    ok &= check(registryFile.open(QIODevice::ReadOnly), "registry readable");
    const QByteArray registryBytes = registryFile.readAll();
    registryFile.close();
    ok &= check(!registryBytes.contains(paired.credential.toUtf8()),
                "raw credential absent from registry");
    PairedDeviceRegistry reloaded(registryPath);
    ok &= check(reloaded.load(&error) && reloaded.find(paired.deviceId).has_value(),
                "registry reload preserves stable device identity");

    const auto lowercaseAttempt = service.start(&error);
    const auto lowercasePaired = service.pair(lowercaseAttempt.code.toLower(),
                                              QStringLiteral("Living Room PC"), {}, {});
    ok &= check(lowercasePaired.success,
                "lowercase formatted correct code pairs successfully");
    const auto canonicalAttempt = service.start(&error);
    const auto canonicalPaired = service.pair(QStringLiteral("  ") + canonicalAttempt.code
                                                  + QStringLiteral("  "),
                                              QStringLiteral("Canonical device"), {}, {});
    ok &= check(canonicalPaired.success,
                "canonical correct code with surrounding whitespace pairs successfully");

    const auto secondAttempt = service.start(&error);
    ok &= check(secondAttempt.active && secondAttempt.code != firstCode,
                "new attempt receives a different code");
    now = secondAttempt.expiresUtc;
    ok &= check(service.pair(secondAttempt.code, QStringLiteral("Expired"), {}, {}).errorCode
                    == QStringLiteral("PAIRING_DISABLED"),
                "expired code rejected and disabled");

    const auto malformedAttempt = service.start(&error);
    const auto malformedSubmission = service.pair(QStringLiteral("not-a-code"),
                                                   QStringLiteral("Device"), {}, {});
    ok &= check(malformedSubmission.errorCode == QStringLiteral("PAIRING_CODE_INVALID")
                    && service.attempt().failures == 1,
                "malformed network submission still counts against Host attempt protection");

    const auto limited = service.start(&error);
    for (int failure = 1; failure <= BrickSuitePairingService::MaximumFailures; ++failure) {
        const auto result = service.pair(QStringLiteral("FFFF-FFFF-FFFF-FFFF"),
                                         QStringLiteral("Device"), {}, {});
        if (failure < BrickSuitePairingService::MaximumFailures)
            ok &= check(result.errorCode == QStringLiteral("PAIRING_CODE_INVALID"),
                        "failed attempt rejected");
        else
            ok &= check(result.errorCode == QStringLiteral("PAIRING_ATTEMPTS_EXCEEDED")
                            && !service.attempt().active,
                        "attempt limit disables pairing");
        now = now.addSecs(failure);
    }

    PairedDeviceRegistry failedCredentialRegistry(temporary.filePath(QStringLiteral("cred-fail.json")));
    BrickSuitePairingService credentialFailure(failedCredentialRegistry, [&] { return now; },
        [](const QString&, const QString&, QString* failure) {
            if (failure) *failure = QStringLiteral("injected credential failure");
            return false;
        });
    const auto credentialAttempt = credentialFailure.start(&error);
    ok &= check(credentialFailure.pair(credentialAttempt.code, QStringLiteral("Device"), {}, {}).errorCode
                    == QStringLiteral("CREDENTIAL_STORE_FAILED")
                    && failedCredentialRegistry.devices().isEmpty(),
                "credential failure creates no registry record");

    const QString blockingParent = temporary.filePath(QStringLiteral("not-a-directory"));
    QFile blocker(blockingParent);
    ok &= check(blocker.open(QIODevice::WriteOnly) && blocker.write("x") == 1,
                "blocking parent fixture created");
    blocker.close();
    PairedDeviceRegistry failedRegistry(blockingParent + QStringLiteral("/paired.json"));
    QHash<QString, QString> cleanupCredentials;
    BrickSuitePairingService registryFailure(failedRegistry, [&] { return now; },
        [&](const QString& key, const QString& value, QString*) {
            cleanupCredentials.insert(key, value); return true;
        }, [&](const QString& key, QString*) { cleanupCredentials.remove(key); return true; });
    const auto registryAttempt = registryFailure.start(&error);
    ok &= check(registryFailure.pair(registryAttempt.code, QStringLiteral("Device"), {}, {}).errorCode
                    == QStringLiteral("PAIRING_PERSISTENCE_FAILED")
                    && failedRegistry.devices().isEmpty() && cleanupCredentials.isEmpty(),
                "registry failure rolls back the new secret and record");

    QFile corrupt(registryPath);
    ok &= check(corrupt.open(QIODevice::WriteOnly | QIODevice::Truncate)
                    && corrupt.write("{broken") == 7,
                "corrupt registry fixture created");
    corrupt.close();
    PairedDeviceRegistry corruptRegistry(registryPath);
    ok &= check(!corruptRegistry.load(&error) && corruptRegistry.devices().isEmpty(),
                "corrupt registry fails closed");

    return ok ? 0 : 1;
}
