#include "SetImageService.h"

#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUrl>

SetImageService::SetImageService(QObject* parent)
    : QObject(parent)
{
    m_networkManager = new QNetworkAccessManager(this);
}

QString SetImageService::cacheDirectory() const
{
    const QString applicationDataPath = QStandardPaths::writableLocation(
        QStandardPaths::AppLocalDataLocation);

    const QString directoryPath = QDir(applicationDataPath).filePath("cache/sets");

    QDir directory;

    if (!directory.exists(directoryPath)) {
        directory.mkpath(directoryPath);
    }

    return directoryPath;
}

QString SetImageService::safeSetNumber(const QString& setNumber) const
{
    QString safe = setNumber.trimmed();

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

QString SetImageService::cacheFilePath(const QString& setNumber, const QString& imageUrl) const
{
    QString extension;

    if (!imageUrl.isEmpty()) {
        const QUrl url(imageUrl);

        extension = QFileInfo(url.path()).suffix().toLower();
    }

    if (extension.isEmpty()) {
        extension = "jpg";
    }

    const QString fileName = QString("%1.%2").arg(safeSetNumber(setNumber), extension);

    return QDir(cacheDirectory()).filePath(fileName);
}

QString SetImageService::cachedImagePath(const QString& setNumber) const
{
    const QString safe = safeSetNumber(setNumber);

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

    const_cast<SetImageService*>(this)->m_cachedPaths.insert(safe, path);

    return path;
}

void SetImageService::requestSetImage(const QString& setNumber, const QString& imageUrl)
{
    const QString trimmedSetNumber = setNumber.trimmed();

    if (trimmedSetNumber.isEmpty()) {
        emit imageFailed(setNumber, "Set number is empty.");

        return;
    }

    const QString cachedPath = cachedImagePath(trimmedSetNumber);

    if (!cachedPath.isEmpty()) {
        emit imageReady(trimmedSetNumber, cachedPath);

        return;
    }

    if (imageUrl.trimmed().isEmpty()) {
        emit imageFailed(trimmedSetNumber, "Set image URL is empty.");

        return;
    }

    const QString safe = safeSetNumber(trimmedSetNumber);

    if (m_pendingSetNumbers.contains(safe))
        return;

    m_pendingSetNumbers.insert(safe);

    downloadImage(trimmedSetNumber, imageUrl.trimmed());
}

void SetImageService::downloadImage(const QString& setNumber, const QString& imageUrl)
{
    const QUrl url(imageUrl);

    if (!url.isValid()) {
        qWarning() << "Set image request rejected: invalid URL."
                   << "SetNumber:" << setNumber
                   << "Url:" << imageUrl;
        emit imageFailed(setNumber, "Invalid set image URL.");

        return;
    }

    QNetworkRequest request(url);

    request.setHeader(QNetworkRequest::UserAgentHeader, "BrickSuite/1.0");

    QNetworkReply* reply = m_networkManager->get(request);

    connect(reply, &QNetworkReply::finished, this, [this, reply, setNumber, imageUrl]() {
        const QString safe = safeSetNumber(setNumber);

        m_pendingSetNumbers.remove(safe);

        if (reply->error() != QNetworkReply::NoError) {
            const QString error = reply->errorString();

            qWarning() << "Set image download failed."
                       << "SetNumber:" << setNumber
                       << "Error:" << error;

            reply->deleteLater();

            emit imageFailed(setNumber, QString("Unable to download set image: %1").arg(error));

            return;
        }

        const QByteArray imageData = reply->readAll();

        reply->deleteLater();

        if (imageData.isEmpty()) {
            qWarning() << "Set image download returned no data."
                       << "SetNumber:" << setNumber;
            emit imageFailed(setNumber, "The image download returned no data.");

            return;
        }

        const QString path = cacheFilePath(setNumber, imageUrl);

        QSaveFile file(path);

        if (!file.open(QIODevice::WriteOnly)) {
            qWarning() << "Unable to create cached Set image."
                       << "SetNumber:" << setNumber
                       << "Path:" << path
                       << "Error:" << file.errorString();
            emit imageFailed(setNumber, QString("Unable to create cached image: %1").arg(path));

            return;
        }

        if (file.write(imageData) != imageData.size()) {
            qWarning() << "Unable to write cached Set image."
                       << "SetNumber:" << setNumber
                       << "Path:" << path;
            file.cancelWriting();

            emit imageFailed(setNumber, "Unable to write cached set image.");

            return;
        }

        if (!file.commit()) {
            qWarning() << "Unable to commit cached Set image."
                       << "SetNumber:" << setNumber
                       << "Path:" << path
                       << "Error:" << file.errorString();
            emit imageFailed(setNumber, "Unable to commit cached set image.");

            return;
        }

        m_cachedPaths.insert(safe, path);

        emit imageReady(setNumber, path);
    });
}