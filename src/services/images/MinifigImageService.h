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

#pragma once

#include <QHash>
#include <QObject>
#include <QQueue>
#include <QSet>
#include <QString>

class QNetworkAccessManager;

class MinifigImageService : public QObject
{
    Q_OBJECT

public:
    explicit MinifigImageService(QObject* parent = nullptr);

    void requestMinifigImage(const QString& minifigNumber, const QString& imageUrl);
    void clearQueuedRequests();
    QString cachedImagePath(const QString& minifigNumber) const;
    bool isImageKnownUnavailable(const QString& minifigNumber,
                                 const QString& imageUrl) const;

signals:
    void imageReady(const QString& minifigNumber, const QString& imagePath);
    void imageFailed(const QString& minifigNumber, const QString& message);

private:
    struct Request
    {
        QString minifigNumber;
        QString imageUrl;
    };

    QString cacheDirectory() const;
    QString cacheFilePath(const QString& minifigNumber,
                          const QString& imageUrl = QString()) const;
    QString cacheKey(const QString& minifigNumber) const;
    QString requestKey(const QString& minifigNumber) const;
    void startQueuedRequests();
    void downloadImage(const Request& request);

    static constexpr int MaximumConcurrentDownloads = 4;

    QNetworkAccessManager* m_networkManager = nullptr;
    QQueue<Request> m_queue;
    QSet<QString> m_pendingKeys;
    QHash<QString, QString> m_cachedPaths;
    int m_activeDownloads = 0;
};
