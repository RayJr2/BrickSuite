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

#include <QObject>
#include <QVersionNumber>

class QJsonObject;
class QNetworkAccessManager;
class QNetworkReply;

class Updater : public QObject
{
    Q_OBJECT

public:
    explicit Updater(QObject* parent = nullptr);

    void checkForUpdates(const QString& manifestUrl,
                         const QString& currentVersion);

    // Pure selection policy, parameterized for host-independent tests.
    static QString platformKey(const QString& operatingSystem,
                               const QString& buildArchitecture);
    static QString pickDownloadUrl(const QJsonObject& root,
                                   const QString& platformKey);

signals:
    void updateAvailable(const QString& newVersion,
                         const QString& downloadUrl,
                         const QString& releaseNotes);

    void updateCheckFailed(const QString& reason);
    void noUpdateAvailable();

private:
    QString detectPlatformKey() const;

    void processReply(QNetworkReply* reply);

    QNetworkAccessManager* m_networkManager = nullptr;
    QString m_currentAppVersion;
};
