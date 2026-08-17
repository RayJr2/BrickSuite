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

#include <QByteArray>
#include <QElapsedTimer>
#include <QHash>
#include <QList>
#include <QObject>
#include <QQueue>
#include <QString>
#include <QStringList>
#include <functional>

class QNetworkAccessManager;
class QNetworkReply;
class QNetworkRequest;
class QTimer;

class RebrickableApiClient : public QObject
{
    Q_OBJECT

public:
    enum class RequestPriority { Foreground, Background };

    struct ConnectionResult
    {
        bool success = false;

        int httpStatusCode = 0;

        QString message;
    };

    struct PartColor
    {
        int rebrickableColorId = 0;
        QString name;
    };

    struct PartColorsResult
    {
        bool success = false;

        int httpStatusCode = 0;

        QString partNumber;
        QString message;

        QList<PartColor> colors;
    };

    struct PartDetails
    {
        QString partNumber;
        QString name;

        int partCategoryId = 0;

        int yearFrom = 0;
        int yearTo = 0;

        QString partUrl;
        QString partImageUrl;

        QStringList prints;
        QStringList molds;
        QStringList alternates;

        QHash<QString, QStringList> externalIds;

        QString printOf;
    };

    struct PartDetailsResult
    {
        bool success = false;

        int httpStatusCode = 0;

        QString message;

        PartDetails part;
    };

    struct PartImageUrl
    {
        QString partNumber;
        QString partImageUrl;
    };

    struct PartImageUrlsResult
    {
        bool success = false;

        int httpStatusCode = 0;

        QString message;

        QList<PartImageUrl> parts;
    };

    struct PartColorDetails
    {
        QString partNumber;

        int rebrickableColorId = 0;

        QString partImageUrl;
    };

    struct PartColorDetailsResult
    {
        bool success = false;

        int httpStatusCode = 0;

        QString message;

        PartColorDetails partColor;
    };

    struct SetDetails
    {
        QString setNumber;
        QString name;

        int year = 0;
        int themeId = 0;
        int numberOfParts = 0;

        QString setImageUrl;
        QString setUrl;
        QString lastModifiedUtc;
    };

    struct SetDetailsResult
    {
        bool success = false;

        int httpStatusCode = 0;

        QString message;

        SetDetails set;
    };

    struct SetPart
    {
        int inventoryPartId = 0;

        QString setNumber;

        QString partNumber;
        QString partName;

        int partCategoryId = 0;

        QString partUrl;
        QString partImageUrl;

        int rebrickableColorId = 0;

        QString colorName;
        QString colorRgb;

        bool colorIsTransparent = false;

        int quantity = 0;

        bool isSpare = false;

        QString elementId;

        int numberOfSets = 0;
    };

    struct SetPartsResult
    {
        bool success = false;

        int httpStatusCode = 0;

        QString setNumber;
        QString message;

        int totalCount = 0;

        QString nextUrl;
        QString previousUrl;

        QList<SetPart> parts;
    };

    explicit RebrickableApiClient(QObject* parent = nullptr);

    void testConnection(const QString& apiKey);

    void getPartColors(const QString& partNumber, const QString& apiKey);

    void getPartDetails(const QString& partNumber, const QString& apiKey);

    void getPartImageUrls(const QStringList& partNumbers,
                          const QString& apiKey,
                          RequestPriority priority = RequestPriority::Background);

    void getSetDetails(const QString& setNumber, const QString& apiKey);

    void getSetParts(const QString& setNumber, const QString& apiKey);

    void getPartColorDetails(const QString& partNumber,
                             int rebrickableColorId,
                             const QString& apiKey,
                             RequestPriority priority = RequestPriority::Foreground);

    static bool isSessionBlocked();

    static QString sessionBlockReason();

signals:
    void connectionTestFinished(const RebrickableApiClient::ConnectionResult& result);

    void partColorsFinished(const RebrickableApiClient::PartColorsResult& result);

    void partDetailsFinished(const RebrickableApiClient::PartDetailsResult& result);

    void partImageUrlsFinished(const RebrickableApiClient::PartImageUrlsResult& result);

    void setDetailsFinished(const RebrickableApiClient::SetDetailsResult& result);

    void setPartsFinished(const RebrickableApiClient::SetPartsResult& result);

    void partColorDetailsFinished(const RebrickableApiClient::PartColorDetailsResult& result);

private:
    void handleConnectionTestReply(QNetworkReply* reply);

    QNetworkAccessManager* m_networkManager = nullptr;

    static bool detectCloudflareIpBan(const QByteArray& responseData, QString& reason);

    static void tripSessionCircuitBreaker(const QString& reason);

    static bool s_sessionBlocked;
    static QString s_sessionBlockReason;

    using QueuedRequest = std::function<void()>;

    using ReplyHandler = std::function<void(QNetworkReply*)>;

    void enqueueGet(const QNetworkRequest& request,
                    ReplyHandler replyHandler,
                    std::function<void()> blockedHandler,
                    RequestPriority priority = RequestPriority::Foreground);

    static void enqueueRequest(QueuedRequest request, RequestPriority priority);

    static QQueue<QueuedRequest> s_foregroundRequestQueue;
    static QQueue<QueuedRequest> s_backgroundRequestQueue;

    static void processRequestQueue();

    static void ensureRequestTimer();

    static void handle429();

    static QElapsedTimer s_lastRequestTimer;

    static QTimer* s_requestTimer;
};

Q_DECLARE_METATYPE(RebrickableApiClient::ConnectionResult)
Q_DECLARE_METATYPE(RebrickableApiClient::PartColor)
Q_DECLARE_METATYPE(RebrickableApiClient::PartColorsResult)
Q_DECLARE_METATYPE(RebrickableApiClient::PartDetails)
Q_DECLARE_METATYPE(RebrickableApiClient::PartDetailsResult)
Q_DECLARE_METATYPE(RebrickableApiClient::SetDetails)
Q_DECLARE_METATYPE(RebrickableApiClient::SetDetailsResult)
Q_DECLARE_METATYPE(RebrickableApiClient::SetPart)
Q_DECLARE_METATYPE(RebrickableApiClient::SetPartsResult)
Q_DECLARE_METATYPE(RebrickableApiClient::PartColorDetails)
Q_DECLARE_METATYPE(RebrickableApiClient::PartColorDetailsResult)
Q_DECLARE_METATYPE(RebrickableApiClient::PartImageUrl)
Q_DECLARE_METATYPE(RebrickableApiClient::PartImageUrlsResult)
