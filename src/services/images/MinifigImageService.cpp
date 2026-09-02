#include "MinifigImageService.h"

#include <QDebug>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUrl>

MinifigImageService::MinifigImageService(QObject* parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
{
}

QString MinifigImageService::cacheDirectory() const
{
    const QString applicationDataPath = QStandardPaths::writableLocation(
        QStandardPaths::AppLocalDataLocation);
    const QString directoryPath = QDir(applicationDataPath).filePath("cache/minifigs");
    QDir().mkpath(directoryPath);
    return directoryPath;
}

QString MinifigImageService::cacheKey(const QString& minifigNumber) const
{
    // The full SHA-256 digest is deterministic, portable as a filename, and
    // preserves distinctions that lossy filename sanitization would collapse.
    const QByteArray identity = QByteArrayLiteral("Rebrickable\0")
                                + minifigNumber.trimmed().toUtf8();
    return QString::fromLatin1(
        QCryptographicHash::hash(identity, QCryptographicHash::Sha256).toHex());
}

QString MinifigImageService::requestKey(const QString& minifigNumber) const
{
    return cacheKey(minifigNumber);
}

QString MinifigImageService::cacheFilePath(const QString& minifigNumber,
                                           const QString& imageUrl) const
{
    QString extension;
    if (!imageUrl.isEmpty())
        extension = QFileInfo(QUrl(imageUrl).path()).suffix().toLower();
    static const QStringList supportedExtensions = {
        QStringLiteral("bmp"), QStringLiteral("gif"), QStringLiteral("jpeg"),
        QStringLiteral("jpg"), QStringLiteral("png"), QStringLiteral("webp")};
    if (!supportedExtensions.contains(extension))
        extension = QStringLiteral("jpg");
    return QDir(cacheDirectory()).filePath(
        QString("%1.%2").arg(cacheKey(minifigNumber), extension));
}

QString MinifigImageService::cachedImagePath(const QString& minifigNumber) const
{
    const QString key = requestKey(minifigNumber);
    if (m_cachedPaths.contains(key)) {
        const QString rememberedPath = m_cachedPaths.value(key);
        QImage rememberedImage;
        if (rememberedImage.load(rememberedPath))
            return rememberedPath;
        if (!QFile::remove(rememberedPath))
            qWarning() << "Unable to remove invalid cached Minifig image:"
                       << rememberedPath;
        const_cast<MinifigImageService*>(this)->m_cachedPaths.remove(key);
    }

    QDir directory(cacheDirectory());
    const QStringList matches = directory.entryList(
        QStringList() << QString("%1.*").arg(cacheKey(minifigNumber)),
        QDir::Files,
        QDir::Name);
    for (const QString& match : matches) {
        const QString path = directory.filePath(match);
        QImage cachedImage;
        if (cachedImage.load(path)) {
            const_cast<MinifigImageService*>(this)->m_cachedPaths.insert(key, path);
            return path;
        }
        if (!QFile::remove(path))
            qWarning() << "Unable to remove invalid cached Minifig image:" << path;
    }
    return QString();
}

void MinifigImageService::requestMinifigImage(const QString& minifigNumber,
                                              const QString& imageUrl)
{
    const QString number = minifigNumber.trimmed();
    if (number.isEmpty()) {
        emit imageFailed(minifigNumber, "Minifig number is empty.");
        return;
    }

    const QString cachedPath = cachedImagePath(number);
    if (!cachedPath.isEmpty()) {
        emit imageReady(number, cachedPath);
        return;
    }

    const QUrl url(imageUrl.trimmed());
    if (!url.isValid() || (url.scheme() != "http" && url.scheme() != "https")) {
        emit imageFailed(number, "Minifig image URL is invalid.");
        return;
    }

    const QString key = requestKey(number);
    if (m_pendingKeys.contains(key))
        return;

    m_pendingKeys.insert(key);
    m_queue.enqueue({number, imageUrl.trimmed()});
    startQueuedRequests();
}

void MinifigImageService::clearQueuedRequests()
{
    while (!m_queue.isEmpty())
        m_pendingKeys.remove(requestKey(m_queue.dequeue().minifigNumber));
}

void MinifigImageService::startQueuedRequests()
{
    while (m_activeDownloads < MaximumConcurrentDownloads && !m_queue.isEmpty()) {
        const Request request = m_queue.dequeue();
        ++m_activeDownloads;
        downloadImage(request);
    }
}

void MinifigImageService::downloadImage(const Request& request)
{
    QNetworkRequest networkRequest(QUrl(request.imageUrl));
    networkRequest.setHeader(QNetworkRequest::UserAgentHeader, "BrickSuite/1.0");
    QNetworkReply* reply = m_networkManager->get(networkRequest);

    connect(reply, &QNetworkReply::finished, this, [this, reply, request]() {
        const QString key = requestKey(request.minifigNumber);
        m_pendingKeys.remove(key);
        --m_activeDownloads;

        if (reply->error() != QNetworkReply::NoError) {
            const QString error = reply->errorString();
            reply->deleteLater();
            qWarning() << "Minifig image download failed."
                       << "Minifig:" << request.minifigNumber
                       << "Error:" << error;
            emit imageFailed(request.minifigNumber,
                             QString("Unable to download Minifig image: %1").arg(error));
            startQueuedRequests();
            return;
        }

        const QByteArray imageData = reply->readAll();
        reply->deleteLater();
        QImage downloadedImage;
        if (!downloadedImage.loadFromData(imageData)) {
            emit imageFailed(request.minifigNumber,
                             "The downloaded Minifig image is invalid.");
            startQueuedRequests();
            return;
        }
        const QString path = cacheFilePath(request.minifigNumber, request.imageUrl);
        QSaveFile file(path);

        if (imageData.isEmpty() || !file.open(QIODevice::WriteOnly)
            || file.write(imageData) != imageData.size() || !file.commit()) {
            file.cancelWriting();
            emit imageFailed(request.minifigNumber, "Unable to cache Minifig image.");
            startQueuedRequests();
            return;
        }

        m_cachedPaths.insert(key, path);
        emit imageReady(request.minifigNumber, path);
        startQueuedRequests();
    });
}
