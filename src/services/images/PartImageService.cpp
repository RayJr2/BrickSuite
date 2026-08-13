#include "PartImageService.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUrl>

PartImageService::PartImageService(
    QObject* parent)
    : QObject(parent)
{
    m_networkManager =
        new QNetworkAccessManager(this);
}

QString PartImageService::cacheDirectory() const
{
    const QString applicationDataPath =
        QStandardPaths::writableLocation(
            QStandardPaths::AppLocalDataLocation);

    const QString directoryPath =
        QDir(applicationDataPath)
            .filePath("cache/parts");

    QDir directory;

    if (!directory.exists(directoryPath))
    {
        directory.mkpath(
            directoryPath);
    }

    return directoryPath;
}

QString PartImageService::safePartNumber(
    const QString& partNumber) const
{
    QString safe =
        partNumber.trimmed();

    safe.replace("/", "_");
    safe.replace("\\", "_");
    safe.replace(":", "_");
    safe.replace("*", "_");
    safe.replace("?", "_");
    safe.replace("\"", "_");
    safe.replace("<", "_");
    safe.replace(">", "_");
    safe.replace("|", "_");

    return safe;
}

QString PartImageService::cacheFilePath(
    const QString& partNumber,
    const QString& imageUrl) const
{
    QString extension;

    if (!imageUrl.isEmpty())
    {
        const QUrl url(imageUrl);

        extension =
            QFileInfo(
                url.path())
                .suffix()
                .toLower();
    }

    if (extension.isEmpty())
    {
        //
        // Existing generic lookup.
        // Try our normal preferred extension.
        //
        extension = "jpg";
    }

    const QString fileName =
        QString("%1.%2")
            .arg(
                safePartNumber(
                    partNumber),
                extension);

    return QDir(
               cacheDirectory())
        .filePath(
            fileName);
}

bool PartImageService::hasCachedImage(
    const QString& partNumber) const
{
    const QString safe =
        safePartNumber(
            partNumber);

    QDir directory(
        cacheDirectory());

    const QStringList matches =
        directory.entryList(
            QStringList()
                << QString("%1.*").arg(safe),
            QDir::Files);

    return !matches.isEmpty();
}

QString PartImageService::cachedImagePath(const QString& partNumber) const
{
    const QString safe = safePartNumber(partNumber);

    if (m_cachedPaths.contains(safe)) {
        return m_cachedPaths.value(safe);
    }

    QDir directory(cacheDirectory());

    const QStringList matches = directory.entryList(QStringList() << QString("%1.*").arg(safe),
                                                    QDir::Files,
                                                    QDir::Name);

    if (matches.isEmpty())
        return QString();

    const QString path = directory.filePath(matches.first());

    const_cast<PartImageService*>(this)->m_cachedPaths.insert(safe, path);

    return path;
}

void PartImageService::requestPartImage(const QString& partNumber, const QString& imageUrl)
{
    const QString trimmedPartNumber = partNumber.trimmed();

    if (trimmedPartNumber.isEmpty()) {
        emit imageFailed(partNumber, "Part number is empty.");

        return;
    }

    const QString cachedPath = cachedImagePath(trimmedPartNumber);

    if (!cachedPath.isEmpty()) {
        emit imageReady(trimmedPartNumber, cachedPath);

        return;
    }

    if (imageUrl.trimmed().isEmpty()) {
        emit imageFailed(trimmedPartNumber, "Part image URL is empty.");

        return;
    }

    const QString safe = safePartNumber(trimmedPartNumber);

    if (m_pendingPartNumbers.contains(safe)) {
        return;
    }

    m_pendingPartNumbers.insert(safe);

    downloadImage(trimmedPartNumber, imageUrl.trimmed());
}

void PartImageService::downloadImage(const QString& partNumber, const QString& imageUrl)
{
    const QUrl url(imageUrl);

    if (!url.isValid()) {
        emit imageFailed(partNumber, "Invalid part image URL.");

        return;
    }

    QNetworkRequest request(url);

    request.setHeader(QNetworkRequest::UserAgentHeader, "BrickSuite/1.0");

    QNetworkReply* reply = m_networkManager->get(request);

    connect(reply, &QNetworkReply::finished, this, [this, reply, partNumber, imageUrl]() {
        //
        // This request is no longer pending.
        //
        // We do this once, right at the beginning of the
        // finished handler. That covers ALL success and
        // failure paths below.
        //
        const QString safe = safePartNumber(partNumber);

        m_pendingPartNumbers.remove(safe);

        //
        // Network error
        //
        if (reply->error() != QNetworkReply::NoError) {
            const QString error = reply->errorString();

            reply->deleteLater();

            emit imageFailed(partNumber, QString("Unable to download part image: %1").arg(error));

            return;
        }

        //
        // Read downloaded image
        //
        const QByteArray imageData = reply->readAll();

        reply->deleteLater();

        if (imageData.isEmpty()) {
            emit imageFailed(partNumber, "Rebrickable returned an empty image.");

            return;
        }

        //
        // Determine cache filename
        //
        const QString path = cacheFilePath(partNumber, imageUrl);

        //
        // Save image safely to disk
        //
        QSaveFile file(path);

        if (!file.open(QIODevice::WriteOnly)) {
            emit imageFailed(partNumber, QString("Unable to create cached image: %1").arg(path));

            return;
        }

        if (file.write(imageData) != imageData.size()) {
            file.cancelWriting();

            emit imageFailed(partNumber, "Unable to write cached part image.");

            return;
        }

        if (!file.commit()) {
            emit imageFailed(partNumber, "Unable to commit cached part image.");

            return;
        }

        //
        // Remember the cached path in memory.
        //
        m_cachedPaths.insert(safe, path);

        //
        // Notify interested UI controls.
        //
        emit imageReady(partNumber, path);
    });
}