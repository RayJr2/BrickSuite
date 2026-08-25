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

#include "Updater.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>

Updater::Updater(QObject* parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
{
}

void Updater::checkForUpdates(const QString& manifestUrl,
                              const QString& currentVersion)
{
    m_currentAppVersion = currentVersion;

    QUrl url(manifestUrl);

    // Bust intermediary/browser caches so a newly-published manifest is
    // picked up immediately.
    QUrlQuery query(url);
    query.addQueryItem(QStringLiteral("_ts"),
                       QString::number(QDateTime::currentMSecsSinceEpoch()));
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("BrickSuite Updater"));

    QNetworkReply* reply = m_networkManager->get(request);

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        processReply(reply);
        reply->deleteLater();
    });
}

static QVersionNumber parseNumericVersion(const QString& value)
{
    // QVersionNumber parses the leading numeric portion. This keeps
    // comparisons predictable if a future manifest uses a suffix such
    // as "-beta".
    return QVersionNumber::fromString(value);
}

QString Updater::pickDownloadUrl(const QJsonObject& root,
                                 const QString& platformKey) const
{
    // Preferred schema:
    // {
    //   "downloads": {
    //     "windows": { "url": "..." },
    //     "macos":   { "url": "..." },
    //     "linux64": { "url": "..." },
    //     "linuxarm":{ "url": "..." }
    //   }
    // }
    if (root.contains(QStringLiteral("downloads"))
        && root.value(QStringLiteral("downloads")).isObject()) {
        const QJsonObject downloads =
            root.value(QStringLiteral("downloads")).toObject();

        const auto it = downloads.find(platformKey);
        if (it != downloads.end()) {
            if (it->isString()) {
                return it->toString().trimmed();
            }

            if (it->isObject()) {
                const QJsonObject object = it->toObject();

                const QString url =
                    object.value(QStringLiteral("url")).toString().trimmed();

                if (!url.isEmpty()) {
                    return url;
                }

                return object.value(QStringLiteral("downloadUrl"))
                    .toString()
                    .trimmed();
            }
        }
    }

    // Backward-compatible schema retained from the updater used by
    // RF StateSide's other Qt applications:
    // {
    //   "platforms": {
    //     "windows": { "downloadUrl": "..." }
    //   }
    // }
    if (root.contains(QStringLiteral("platforms"))
        && root.value(QStringLiteral("platforms")).isObject()) {
        const QJsonObject platforms =
            root.value(QStringLiteral("platforms")).toObject();

        const auto it = platforms.find(platformKey);
        if (it != platforms.end() && it->isObject()) {
            const QJsonObject object = it->toObject();

            const QString downloadUrl =
                object.value(QStringLiteral("downloadUrl"))
                    .toString()
                    .trimmed();

            if (!downloadUrl.isEmpty()) {
                return downloadUrl;
            }

            return object.value(QStringLiteral("url"))
                .toString()
                .trimmed();
        }
    }

    return {};
}

void Updater::processReply(QNetworkReply* reply)
{
    if (reply->error() != QNetworkReply::NoError) {
        emit updateCheckFailed(reply->errorString());
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument document =
        QJsonDocument::fromJson(reply->readAll(), &parseError);

    if (parseError.error != QJsonParseError::NoError
        || !document.isObject()) {
        emit updateCheckFailed(QStringLiteral("Invalid JSON update manifest."));
        return;
    }

    const QJsonObject root = document.object();

    const QString latestVersionString =
        root.value(QStringLiteral("version")).toString().trimmed();

    const QString releaseNotes =
        root.value(QStringLiteral("changelog")).toString();

    if (latestVersionString.isEmpty()) {
        emit updateCheckFailed(
            QStringLiteral("Update manifest is missing 'version'."));
        return;
    }

    const QVersionNumber current =
        parseNumericVersion(m_currentAppVersion);

    const QVersionNumber latest =
        parseNumericVersion(latestVersionString);

    if (current.isNull()) {
        emit updateCheckFailed(
            QStringLiteral("Current BrickSuite version is malformed."));
        return;
    }

    if (latest.isNull()) {
        emit updateCheckFailed(
            QStringLiteral("Update manifest contains a malformed version."));
        return;
    }

    if (QVersionNumber::compare(latest, current) <= 0) {
        emit noUpdateAvailable();
        return;
    }

    const QString platformKey = detectPlatformKey();
    const QString downloadUrl = pickDownloadUrl(root, platformKey);

    if (downloadUrl.isEmpty()) {
        emit updateCheckFailed(
            QStringLiteral("No download URL is available for platform '%1'.")
                .arg(platformKey));
        return;
    }

    emit updateAvailable(latestVersionString,
                         downloadUrl,
                         releaseNotes);
}

QString Updater::detectPlatformKey() const
{
#if defined(Q_OS_WIN)
    return QStringLiteral("windows");
#elif defined(Q_OS_MACOS) || defined(Q_OS_MAC)
    return QStringLiteral("macos");
#elif defined(Q_OS_LINUX)
#  if defined(Q_PROCESSOR_ARM_64) || defined(__aarch64__)
    return QStringLiteral("linuxarm");
#  else
    return QStringLiteral("linux64");
#  endif
#else
    return QStringLiteral("unknown");
#endif
}
