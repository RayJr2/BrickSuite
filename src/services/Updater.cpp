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
                                 const QString& platformKey)
{
    // Exact structured key only: never fall back from an architecture-specific
    // macOS entry to generic macos or another architecture.
    if (platformKey != QStringLiteral("windows")
        && platformKey != QStringLiteral("linux64")
        && platformKey != QStringLiteral("macos-arm64")
        && platformKey != QStringLiteral("macos-x86_64")) return {};
    const auto validUrl = [](const QString& value) {
        const QString text = value.trimmed();
        const QUrl url(text, QUrl::StrictMode);
        return url.isValid() && !url.host().isEmpty()
            && (url.scheme() == QStringLiteral("https") || url.scheme() == QStringLiteral("http"))
            ? text : QString();
    };
    if (root.contains(QStringLiteral("downloads"))
        && root.value(QStringLiteral("downloads")).isObject()) {
        const QJsonObject downloads =
            root.value(QStringLiteral("downloads")).toObject();

        const auto it = downloads.find(platformKey);
        if (it != downloads.end()) {
            if (it->isString()) {
                return validUrl(it->toString());
            }

            if (it->isObject()) {
                const QJsonObject object = it->toObject();

                const QString url =
                    object.value(QStringLiteral("url")).toString().trimmed();

                if (!url.isEmpty()) {
                    return validUrl(url);
                }

                return validUrl(object.value(QStringLiteral("downloadUrl")).toString());
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
                return validUrl(downloadUrl);
            }

            return validUrl(object.value(QStringLiteral("url")).toString());
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
            QStringLiteral("No compatible update package is available for this BrickSuite build ('%1').")
                .arg(platformKey));
        return;
    }

    emit updateAvailable(latestVersionString,
                         downloadUrl,
                         releaseNotes);
}

QString Updater::platformKey(const QString& operatingSystem,
                             const QString& buildArchitecture)
{
    if (operatingSystem == QStringLiteral("macos")) {
        if (buildArchitecture == QStringLiteral("arm64")) return QStringLiteral("macos-arm64");
        if (buildArchitecture == QStringLiteral("x86_64")) return QStringLiteral("macos-x86_64");
    } else if (buildArchitecture == QStringLiteral("x86_64")) {
        if (operatingSystem == QStringLiteral("windows")) return QStringLiteral("windows");
        if (operatingSystem == QStringLiteral("linux")) return QStringLiteral("linux64");
    }
    return QStringLiteral("unsupported-%1-%2").arg(operatingSystem, buildArchitecture);
}

QString Updater::detectPlatformKey() const
{
    // Qt compiler-target macros describe this executable's architecture, not
    // the host CPU. An x86_64 process under Rosetta must still select Intel.
#if defined(Q_PROCESSOR_ARM_64)
    const QString architecture = QStringLiteral("arm64");
#elif defined(Q_PROCESSOR_X86_64)
    const QString architecture = QStringLiteral("x86_64");
#else
    const QString architecture = QStringLiteral("unknown");
#endif
#if defined(Q_OS_WIN)
    return platformKey(QStringLiteral("windows"), architecture);
#elif defined(Q_OS_MACOS) || defined(Q_OS_MAC)
    return platformKey(QStringLiteral("macos"), architecture);
#elif defined(Q_OS_LINUX)
    return platformKey(QStringLiteral("linux"), architecture);
#else
    return platformKey(QStringLiteral("unknown"), architecture);
#endif
}
