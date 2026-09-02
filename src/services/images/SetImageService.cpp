/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 *
 * This file is part of BrickSuite.
 *
 * BrickSuite is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, version 3 of the License.
 *
 * BrickSuite is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with BrickSuite. If not, see <https://www.gnu.org/licenses/>.
 */

#include "SetImageService.h"
#include "ImageUnavailableCache.h"

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

bool SetImageService::isImageKnownUnavailable(const QString& setNumber,
                                              const QString& imageUrl) const
{
    return ImageUnavailableCache::isKnownUnavailable(
        cacheDirectory(), QStringLiteral("Rebrickable Set:%1").arg(setNumber.trimmed()),
        imageUrl.trimmed());
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

    if (isImageKnownUnavailable(trimmedSetNumber, imageUrl)) {
        emit imageFailed(trimmedSetNumber, "The Set image is known to be unavailable.");
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

        const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (httpStatus == 404) {
            const auto result = ImageUnavailableCache::markUnavailable(
                cacheDirectory(), QStringLiteral("Rebrickable Set:%1").arg(setNumber), imageUrl);
            reply->deleteLater();
            if (result == ImageUnavailableCache::MarkResult::Created) {
                qWarning() << "Set image returned HTTP 404 and is now known unavailable."
                           << "SetNumber:" << setNumber;
            } else if (result == ImageUnavailableCache::MarkResult::Failed) {
                qWarning() << "Set image returned HTTP 404, but its unavailable marker could not be saved."
                           << "SetNumber:" << setNumber;
            }
            emit imageFailed(setNumber, "The Set image is known to be unavailable.");
            return;
        }

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
