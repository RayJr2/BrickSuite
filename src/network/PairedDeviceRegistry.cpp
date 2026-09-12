#include "PairedDeviceRegistry.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <QUuid>
#include <algorithm>

namespace {
constexpr int RegistryVersion = 1;

bool validUuid(const QString& value)
{
    return !QUuid(value).isNull()
        && QUuid(value).toString(QUuid::WithoutBraces).compare(value, Qt::CaseInsensitive) == 0;
}
}

PairedDeviceRegistry::PairedDeviceRegistry(QString path)
    : m_path(path.isEmpty() ? defaultPath() : std::move(path))
{
}

QString PairedDeviceRegistry::defaultPath()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation))
        .filePath(QStringLiteral("server/paired-devices.json"));
}

bool PairedDeviceRegistry::load(QString* error)
{
    m_devices.clear();
    m_available = false;
    m_lastError.clear();
    QFile input(m_path);
    if (!input.exists()) {
        m_available = true;
        return true;
    }
    if (!input.open(QIODevice::ReadOnly)) {
        m_lastError = QStringLiteral("Unable to open the paired-device registry.");
        if (error) *error = m_lastError;
        return false;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(input.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        m_lastError = QStringLiteral("The paired-device registry is invalid.");
        if (error) *error = m_lastError;
        return false;
    }
    const QJsonObject root = document.object();
    if (root.value(QStringLiteral("version")).toInt(-1) != RegistryVersion
        || !root.value(QStringLiteral("devices")).isArray()) {
        m_lastError = QStringLiteral("The paired-device registry version is unsupported.");
        if (error) *error = m_lastError;
        return false;
    }
    QSet<QString> ids;
    for (const QJsonValue& value : root.value(QStringLiteral("devices")).toArray()) {
        if (!value.isObject()) {
            m_lastError = QStringLiteral("The paired-device registry contains an invalid record.");
            if (error) *error = m_lastError;
            m_devices.clear();
            return false;
        }
        const QJsonObject object = value.toObject();
        PairedDeviceRecord record{object.value(QStringLiteral("deviceId")).toString(),
            object.value(QStringLiteral("friendlyName")).toString(),
            object.value(QStringLiteral("firstPairedUtc")).toString(),
            object.value(QStringLiteral("lastSeenUtc")).toString(),
            object.value(QStringLiteral("clientVersion")).toString(),
            object.value(QStringLiteral("platform")).toString(),
            object.value(QStringLiteral("credentialReference")).toString(),
            object.value(QStringLiteral("active")).toBool(true)};
        if (!validUuid(record.deviceId) || record.friendlyName.isEmpty()
            || record.friendlyName.size() > 80 || record.credentialReference.isEmpty()
            || ids.contains(record.deviceId)
            || !QDateTime::fromString(record.firstPairedUtc, Qt::ISODateWithMs).isValid()) {
            m_lastError = QStringLiteral("The paired-device registry contains an invalid record.");
            if (error) *error = m_lastError;
            m_devices.clear();
            return false;
        }
        ids.insert(record.deviceId);
        m_devices.append(record);
    }
    m_available = true;
    return true;
}

bool PairedDeviceRegistry::save(QString* error)
{
    if (!QDir().mkpath(QFileInfo(m_path).absolutePath())) {
        if (error) *error = QStringLiteral("Unable to create the paired-device registry directory.");
        return false;
    }
    QJsonArray devices;
    for (const auto& record : m_devices) {
        devices.append(QJsonObject{{QStringLiteral("deviceId"), record.deviceId},
            {QStringLiteral("friendlyName"), record.friendlyName},
            {QStringLiteral("firstPairedUtc"), record.firstPairedUtc},
            {QStringLiteral("lastSeenUtc"), record.lastSeenUtc},
            {QStringLiteral("clientVersion"), record.clientVersion},
            {QStringLiteral("platform"), record.platform},
            {QStringLiteral("credentialReference"), record.credentialReference},
            {QStringLiteral("active"), record.active}});
    }
    QSaveFile output(m_path);
    const QByteArray bytes = QJsonDocument(QJsonObject{
        {QStringLiteral("version"), RegistryVersion},
        {QStringLiteral("devices"), devices}}).toJson(QJsonDocument::Compact);
    if (!output.open(QIODevice::WriteOnly) || output.write(bytes) != bytes.size()
        || !output.commit()) {
        if (error) *error = QStringLiteral("Unable to save the paired-device registry atomically.");
        return false;
    }
    return true;
}

bool PairedDeviceRegistry::add(const PairedDeviceRecord& record, QString* error)
{
    if (!m_available) {
        if (error) *error = m_lastError;
        return false;
    }
    if (find(record.deviceId)) {
        if (error) *error = QStringLiteral("The paired-device identity already exists.");
        return false;
    }
    m_devices.append(record);
    if (save(error)) return true;
    m_devices.removeLast();
    return false;
}

bool PairedDeviceRegistry::rename(const QString& deviceId, const QString& friendlyName,
                                  QString* error)
{
    const QString name = friendlyName.trimmed();
    if (!m_available || name.isEmpty() || name.size() > 80) {
        if (error) *error = !m_available ? m_lastError
            : QStringLiteral("Enter a device name of 1 to 80 characters.");
        return false;
    }
    for (auto& record : m_devices) {
        if (record.deviceId.compare(deviceId, Qt::CaseInsensitive) != 0) continue;
        const QString previous = record.friendlyName;
        record.friendlyName = name;
        if (save(error)) return true;
        record.friendlyName = previous;
        return false;
    }
    if (error) *error = QStringLiteral("The paired device was not found.");
    return false;
}

bool PairedDeviceRegistry::deactivate(const QString& deviceId, QString* error)
{
    if (!m_available) { if (error) *error = m_lastError; return false; }
    for (auto& record : m_devices) {
        if (record.deviceId.compare(deviceId, Qt::CaseInsensitive) != 0) continue;
        if (!record.active) return true;
        record.active = false;
        if (save(error)) return true;
        record.active = true;
        return false;
    }
    if (error) *error = QStringLiteral("The paired device was not found.");
    return false;
}

bool PairedDeviceRegistry::deactivateAll(QString* error)
{
    if (!m_available) { if (error) *error = m_lastError; return false; }
    const auto previous = m_devices;
    for (auto& record : m_devices) record.active = false;
    if (save(error)) return true;
    m_devices = previous;
    return false;
}

bool PairedDeviceRegistry::remove(const QString& deviceId, QString* error)
{
    if (!m_available) { if (error) *error = m_lastError; return false; }
    for (qsizetype index = 0; index < m_devices.size(); ++index) {
        if (m_devices.at(index).deviceId.compare(deviceId, Qt::CaseInsensitive) != 0) continue;
        const auto previous = m_devices;
        m_devices.removeAt(index);
        if (save(error)) return true;
        m_devices = previous;
        return false;
    }
    return true;
}

bool PairedDeviceRegistry::removeAllInactive(QString* error)
{
    if (!m_available) { if (error) *error = m_lastError; return false; }
    const auto previous = m_devices;
    m_devices.erase(std::remove_if(m_devices.begin(), m_devices.end(),
        [](const PairedDeviceRecord& record) { return !record.active; }), m_devices.end());
    if (save(error)) return true;
    m_devices = previous;
    return false;
}

bool PairedDeviceRegistry::updateLastSeen(const QString& deviceId, const QDateTime& utc,
                                           QString* error)
{
    if (!m_available) { if (error) *error = m_lastError; return false; }
    for (auto& record : m_devices) {
        if (record.deviceId != deviceId) continue;
        const QString previous = record.lastSeenUtc;
        record.lastSeenUtc = utc.toUTC().toString(Qt::ISODateWithMs);
        if (save(error)) return true;
        record.lastSeenUtc = previous;
        return false;
    }
    if (error) *error = QStringLiteral("The paired device was not found.");
    return false;
}

std::optional<PairedDeviceRecord> PairedDeviceRegistry::find(const QString& deviceId) const
{
    for (const auto& record : m_devices)
        if (record.deviceId.compare(deviceId, Qt::CaseInsensitive) == 0) return record;
    return std::nullopt;
}
