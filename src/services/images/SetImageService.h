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
#include <QSet>
#include <QString>

class QNetworkAccessManager;

class SetImageService : public QObject
{
    Q_OBJECT

public:
    explicit SetImageService(QObject* parent = nullptr);

    void requestSetImage(const QString& setNumber, const QString& imageUrl);

    QString cachedImagePath(const QString& setNumber) const;

signals:
    void imageReady(const QString& setNumber, const QString& imagePath);

    void imageFailed(const QString& setNumber, const QString& message);

private:
    QString cacheDirectory() const;

    QString cacheFilePath(const QString& setNumber, const QString& imageUrl = QString()) const;

    QString safeSetNumber(const QString& setNumber) const;

    void downloadImage(const QString& setNumber, const QString& imageUrl);

    QNetworkAccessManager* m_networkManager = nullptr;

    QSet<QString> m_pendingSetNumbers;

    QHash<QString, QString> m_cachedPaths;
};
