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
    QFile input(m_path);
    if (!input.exists()) return true;
    if (!input.open(QIODevice::ReadOnly)) {
        if (error) *error = QStringLiteral("Unable to open the paired-device registry.");
        return false;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(input.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (error) *error = QStringLiteral("The paired-device registry is invalid.");
        return false;
    }
    const QJsonObject root = document.object();
    if (root.value(QStringLiteral("version")).toInt(-1) != RegistryVersion
        || !root.value(QStringLiteral("devices")).isArray()) {
        if (error) *error = QStringLiteral("The paired-device registry version is unsupported.");
        return false;
    }
    QSet<QString> ids;
    for (const QJsonValue& value : root.value(QStringLiteral("devices")).toArray()) {
        if (!value.isObject()) {
            if (error) *error = QStringLiteral("The paired-device registry contains an invalid record.");
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
            if (error) *error = QStringLiteral("The paired-device registry contains an invalid record.");
            m_devices.clear();
            return false;
        }
        ids.insert(record.deviceId);
        m_devices.append(record);
    }
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
    if (find(record.deviceId)) {
        if (error) *error = QStringLiteral("The paired-device identity already exists.");
        return false;
    }
    m_devices.append(record);
    if (save(error)) return true;
    m_devices.removeLast();
    return false;
}

bool PairedDeviceRegistry::updateLastSeen(const QString& deviceId, const QDateTime& utc,
                                           QString* error)
{
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
