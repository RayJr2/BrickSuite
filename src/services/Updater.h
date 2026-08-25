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

signals:
    void updateAvailable(const QString& newVersion,
                         const QString& downloadUrl,
                         const QString& releaseNotes);

    void updateCheckFailed(const QString& reason);
    void noUpdateAvailable();

private:
    QString detectPlatformKey() const;
    QString pickDownloadUrl(const QJsonObject& root,
                            const QString& platformKey) const;

    void processReply(QNetworkReply* reply);

    QNetworkAccessManager* m_networkManager = nullptr;
    QString m_currentAppVersion;
};
