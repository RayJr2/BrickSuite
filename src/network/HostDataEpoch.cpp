#include "HostDataEpoch.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUuid>

namespace {
constexpr auto EpochFile = "host-data-epoch.json";
constexpr auto InitializedFile = "host-data-epoch.initialized";

QString effectiveDirectory(const QString& directory)
{
    return directory.isEmpty() ? HostDataEpoch::storageDirectory() : directory;
}

bool writeMarker(const QString& path, QString* error)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write("1\n") != 2 || !file.commit()) {
        if (error) *error = QStringLiteral("Unable to persist the Host data-epoch initialization marker.");
        return false;
    }
    return true;
}
}

QString HostDataEpoch::storageDirectory()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation))
        .filePath(QStringLiteral("host-state"));
}

QString HostDataEpoch::create()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces).toLower();
}

bool HostDataEpoch::isValid(const QString& epoch)
{
    if (epoch.isEmpty()) return false;
    const QUuid value(epoch);
    return !value.isNull()
        && value.toString(QUuid::WithoutBraces).compare(epoch, Qt::CaseInsensitive) == 0;
}

bool HostDataEpoch::persist(const QString& epoch, QString* error, const QString& directory)
{
    if (!isValid(epoch)) {
        if (error) *error = QStringLiteral("The Host data epoch is not a valid UUID.");
        return false;
    }
    const QString root = effectiveDirectory(directory);
    if (!QDir().mkpath(root)) {
        if (error) *error = QStringLiteral("Unable to create the Host state directory.");
        return false;
    }
    QSaveFile file(QDir(root).filePath(QString::fromLatin1(EpochFile)));
    const QByteArray document = QJsonDocument(QJsonObject{
        {QStringLiteral("version"), 1},
        {QStringLiteral("epoch"), epoch.toLower()}}).toJson(QJsonDocument::Compact);
    if (!file.open(QIODevice::WriteOnly) || file.write(document) != document.size()
        || !file.commit()) {
        if (error) *error = QStringLiteral("Unable to atomically persist the Host data epoch.");
        return false;
    }
    return true;
}

HostDataEpoch::LoadResult HostDataEpoch::load(const QString& directory, bool permitBootstrap)
{
    LoadResult result;
    const QString root = effectiveDirectory(directory);
    const QString epochPath = QDir(root).filePath(QString::fromLatin1(EpochFile));
    const QString markerPath = QDir(root).filePath(QString::fromLatin1(InitializedFile));
    const bool epochExists = QFile::exists(epochPath);
    const bool markerExists = QFile::exists(markerPath);

    if (!epochExists) {
        if (markerExists || !permitBootstrap) {
            result.error = QStringLiteral("The initialized Host data epoch is missing. Host startup was stopped to protect database identity.");
            return result;
        }
        result.epoch = create();
        if (!persist(result.epoch, &result.error, root)
            || !writeMarker(markerPath, &result.error)) {
            QFile::remove(epochPath);
            result.epoch.clear();
            return result;
        }
        result.success = true;
        result.bootstrapped = true;
        return result;
    }

    QFile file(epochPath);
    if (!file.open(QIODevice::ReadOnly)) {
        result.error = QStringLiteral("The Host data epoch could not be read.");
        return result;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    const QJsonObject object = document.object();
    if (parseError.error != QJsonParseError::NoError || !document.isObject()
        || object.value(QStringLiteral("version")).toInt(-1) != 1
        || !object.value(QStringLiteral("epoch")).isString()
        || !isValid(object.value(QStringLiteral("epoch")).toString())) {
        result.error = QStringLiteral("The persisted Host data epoch is malformed. Host startup was stopped to protect database identity.");
        return result;
    }
    result.epoch = object.value(QStringLiteral("epoch")).toString().toLower();
    if (!markerExists && !writeMarker(markerPath, &result.error)) {
        result.epoch.clear();
        return result;
    }
    result.success = true;
    return result;
}

HostDataEpoch::LoadResult HostDataEpoch::loadOrBootstrap(const QString& directory)
{
    return load(directory, true);
}

HostDataEpoch::LoadResult HostDataEpoch::loadExisting(const QString& directory)
{
    return load(directory, false);
}

QString HostDataEpoch::current()
{
    const LoadResult result = loadOrBootstrap();
    return result.success ? result.epoch : QString();
}
