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

#include "RebrickableService.h"

#include "../ApiNetworkService.h"

#include "../../settings/UserSettings.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>

bool RebrickableService::s_sessionBlocked =
#if BRICKSUITE_REBRICKABLE_API_BLOCKED
    true;
#else
    false;
#endif

QString RebrickableService::s_sessionBlockReason =
#if BRICKSUITE_REBRICKABLE_API_BLOCKED
    "Rebrickable API access is disabled by the BrickSuite build configuration.";
#else
    QString();
#endif

QQueue<RebrickableService::QueuedRequest> RebrickableService::s_foregroundRequestQueue;

QQueue<RebrickableService::QueuedRequest> RebrickableService::s_backgroundRequestQueue;

QElapsedTimer RebrickableService::s_lastRequestTimer;

QTimer* RebrickableService::s_requestTimer = nullptr;

RebrickableService::RebrickableService(QObject* parent)
    : QObject(parent)
{
    m_networkService = new ApiNetworkService(this);
}

void RebrickableService::testConnection(const QString& apiKey)
{
    if (isSessionBlocked()) {
        ConnectionResult result;

        result.success = false;
        result.httpStatusCode = 403;

        result.message = sessionBlockReason();

        emit connectionTestFinished(result);

        return;
    }

    const QString trimmedApiKey = apiKey.trimmed();

    if (trimmedApiKey.isEmpty()) {
        ConnectionResult result;

        result.success = false;
        result.message = "Rebrickable API key is empty.";

        emit connectionTestFinished(result);

        return;
    }

    QNetworkRequest request(QUrl("https://rebrickable.com/api/v3/lego/colors/"));

    request.setRawHeader("Authorization", QString("key %1").arg(trimmedApiKey).toUtf8());

    request.setHeader(QNetworkRequest::UserAgentHeader, "BrickSuite/1.0");

    enqueueGet(
        request,
        QStringLiteral("TestConnection"),
        [this](QNetworkReply* reply) { handleConnectionTestReply(reply); },
        [this]() {
            ConnectionResult result;

            result.success = false;
            result.httpStatusCode = 403;
            result.message = sessionBlockReason();

            emit connectionTestFinished(result);
        });
}

void RebrickableService::handleConnectionTestReply(QNetworkReply* reply)
{
    ConnectionResult result;

    const QVariant statusAttribute = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);

    if (statusAttribute.isValid()) {
        result.httpStatusCode = statusAttribute.toInt();
    }

    const QByteArray responseData = reply->readAll();

    QString circuitBreakerReason;

    if (result.httpStatusCode == 403 && detectCloudflareIpBan(responseData, circuitBreakerReason)) {
        qCritical() << "Rebrickable session circuit breaker triggered:"
                    << circuitBreakerReason;
        tripSessionCircuitBreaker(circuitBreakerReason);

        result.message = sessionBlockReason();

        reply->deleteLater();

        emit connectionTestFinished(result);

        return;
    }

    //
    // Rebrickable specifically warns clients not
    // to continue sending requests after HTTP 429.
    //
    if (result.httpStatusCode == 429) {
        qCritical() << "Rebrickable returned HTTP 429 during connection test.";
        handle429();

        result.message = sessionBlockReason();

        reply->deleteLater();

        emit connectionTestFinished(result);

        return;
    }

    if (reply->error() == QNetworkReply::NoError && result.httpStatusCode == 200) {
        const QJsonDocument document = QJsonDocument::fromJson(responseData);

        if (!document.isObject()) {
            result.message = "Rebrickable returned an unexpected response.";
        } else {
            const QJsonObject root = document.object();

            if (!root.contains("results")) {
                result.message = "Rebrickable returned an unexpected response.";
            } else {
                result.success = true;

                result.message = "Connection to Rebrickable succeeded.";
            }
        }
    } else if (result.httpStatusCode == 401) {
        result.message = "Rebrickable rejected the API key.";
    } else if (result.httpStatusCode == 403) {
        result.message = "Rebrickable denied access to this request.";
    } else if (reply->error() != QNetworkReply::NoError) {
        result.message = QString("Unable to connect to Rebrickable: %1").arg(reply->errorString());
    } else {
        result.message = QString("Rebrickable returned HTTP status %1.").arg(result.httpStatusCode);
    }

    qInfo() << "Rebrickable connection test completed."
            << "Success:" << result.success
            << "HTTP:" << result.httpStatusCode
            << "Message:" << result.message;

    reply->deleteLater();

    emit connectionTestFinished(result);
}

void RebrickableService::getPartColors(const QString& partNumber, const QString& apiKey)
{
    if (isSessionBlocked()) {
        PartColorsResult result;

        result.success = false;
        result.httpStatusCode = 403;

        result.partNumber = partNumber.trimmed();

        result.message = sessionBlockReason();

        emit partColorsFinished(result);

        return;
    }

    const QString trimmedPartNumber = partNumber.trimmed();

    const QString trimmedApiKey = apiKey.trimmed();

    if (trimmedPartNumber.isEmpty() || trimmedApiKey.isEmpty()) {
        PartColorsResult result;

        result.partNumber = trimmedPartNumber;

        result.message = "Part number or Rebrickable API key is missing.";

        emit partColorsFinished(result);

        return;
    }

    const QUrl url(QString("https://rebrickable.com/api/v3/lego/parts/%1/colors/")
                       .arg(QUrl::toPercentEncoding(trimmedPartNumber)));

    QNetworkRequest request(url);

    request.setRawHeader("Authorization", QString("key %1").arg(trimmedApiKey).toUtf8());

    request.setHeader(QNetworkRequest::UserAgentHeader, "BrickSuite/1.0");

    enqueueGet(
        request,
        QStringLiteral("GetPartColors"),

        //
        // Actual reply handler.
        //
        [this, trimmedPartNumber](QNetworkReply* reply) {
            PartColorsResult result;

            result.partNumber = trimmedPartNumber;

            const QVariant statusAttribute = reply->attribute(
                QNetworkRequest::HttpStatusCodeAttribute);

            if (statusAttribute.isValid()) {
                result.httpStatusCode = statusAttribute.toInt();
            }

            const QByteArray responseData = reply->readAll();

            QString circuitBreakerReason;

            if (result.httpStatusCode == 403
                && detectCloudflareIpBan(responseData, circuitBreakerReason)) {
                tripSessionCircuitBreaker(circuitBreakerReason);

                result.message = sessionBlockReason();

                reply->deleteLater();

                emit partColorsFinished(result);

                return;
            }

            if (result.httpStatusCode == 429) {
                handle429();

                result.message = sessionBlockReason();

                reply->deleteLater();

                emit partColorsFinished(result);

                return;
            }

            if (reply->error() == QNetworkReply::NoError && result.httpStatusCode == 200) {
                const QJsonDocument document = QJsonDocument::fromJson(responseData);

                if (document.isObject()) {
                    const QJsonObject root = document.object();

                    const QJsonArray results = root.value("results").toArray();

                    for (const QJsonValue& value : results) {
                        if (!value.isObject())
                            continue;

                        const QJsonObject object = value.toObject();

                        PartColor color;

                        color.rebrickableColorId = object.value("color_id").toInt();

                        color.name = object.value("color_name").toString();

                        result.colors.append(color);
                    }

                    result.success = true;

                    result.message = QString("%1 known colors found.").arg(result.colors.size());
                } else {
                    result.message = "Rebrickable returned an unexpected response.";
                }
            } else if (result.httpStatusCode == 401) {
                result.message = "Rebrickable rejected the API key.";
            } else if (result.httpStatusCode == 404) {
                result.message = "The part was not found on Rebrickable.";
            } else if (reply->error() != QNetworkReply::NoError) {
                result.message = QString("Unable to retrieve part colors: %1")
                                     .arg(reply->errorString());
            } else {
                result.message = QString("Rebrickable returned HTTP status %1.")
                                     .arg(result.httpStatusCode);
            }

            reply->deleteLater();

            emit partColorsFinished(result);
        },

        //
        // Called if the circuit breaker trips
        // while this request is still waiting.
        //
        [this, trimmedPartNumber]() {
            PartColorsResult result;

            result.success = false;
            result.httpStatusCode = 403;

            result.partNumber = trimmedPartNumber;

            result.message = sessionBlockReason();

            emit partColorsFinished(result);
        });
}

void RebrickableService::getPartColorDetails(const QString& partNumber,
                                               int rebrickableColorId,
                                               const QString& apiKey,
                                               RequestPriority priority)
{
    const QString trimmedPartNumber = partNumber.trimmed();

    const QString trimmedApiKey = apiKey.trimmed();

    PartColorDetailsResult initialResult;

    initialResult.partColor.partNumber = trimmedPartNumber;

    initialResult.partColor.rebrickableColorId = rebrickableColorId;

    if (isSessionBlocked()) {
        initialResult.httpStatusCode = 403;

        initialResult.message = sessionBlockReason();

        emit partColorDetailsFinished(initialResult);

        return;
    }

    if (trimmedPartNumber.isEmpty() || rebrickableColorId < 0 || trimmedApiKey.isEmpty()) {
        initialResult.message = "Part number, color ID, or Rebrickable "
                                "API key is missing.";

        emit partColorDetailsFinished(initialResult);

        return;
    }

    const QUrl url(QString("https://rebrickable.com/api/v3/lego/"
                           "parts/%1/colors/%2/")
                       .arg(QString::fromUtf8(QUrl::toPercentEncoding(trimmedPartNumber)))
                       .arg(rebrickableColorId));

    QNetworkRequest request(url);

    request.setRawHeader("Authorization", QString("key %1").arg(trimmedApiKey).toUtf8());

    request.setHeader(QNetworkRequest::UserAgentHeader, "BrickSuite/1.0");

    enqueueGet(
        request,
        QStringLiteral("GetPartColorDetails"),
        [this, trimmedPartNumber, rebrickableColorId](QNetworkReply* reply) {
            PartColorDetailsResult result;

            result.partColor.partNumber = trimmedPartNumber;

            result.partColor.rebrickableColorId = rebrickableColorId;

            const QVariant statusAttribute = reply->attribute(
                QNetworkRequest::HttpStatusCodeAttribute);

            if (statusAttribute.isValid()) {
                result.httpStatusCode = statusAttribute.toInt();
            }

            const QByteArray responseData = reply->readAll();

            QString circuitBreakerReason;

            if (result.httpStatusCode == 403
                && detectCloudflareIpBan(responseData, circuitBreakerReason)) {
                tripSessionCircuitBreaker(circuitBreakerReason);

                result.message = sessionBlockReason();

                reply->deleteLater();

                emit partColorDetailsFinished(result);

                return;
            }

            if (reply->error() == QNetworkReply::NoError && result.httpStatusCode == 200) {
                const QJsonDocument document = QJsonDocument::fromJson(responseData);

                if (!document.isObject()) {
                    result.message = "Rebrickable returned an "
                                     "unexpected Part Color response.";
                } else {
                    const QJsonObject root = document.object();

                    result.partColor.partImageUrl = root.value("part_img_url").toString();

                    result.success = true;

                    result.message = QString("Part color details "
                                             "retrieved for %1 / "
                                             "color %2.")
                                         .arg(trimmedPartNumber)
                                         .arg(rebrickableColorId);
                }
            } else if (result.httpStatusCode == 401) {
                result.message = "Rebrickable rejected the API key.";
            } else if (result.httpStatusCode == 404) {
                result.message = QString("Part %1 in Rebrickable "
                                         "Color ID %2 was not found.")
                                     .arg(trimmedPartNumber)
                                     .arg(rebrickableColorId);
            } else if (result.httpStatusCode == 429) {
                handle429();

                result.message = sessionBlockReason();

                reply->deleteLater();

                emit partColorDetailsFinished(result);

                return;
            } else {
                result.message = QString("Unable to retrieve Part "
                                         "Color details: %1")
                                     .arg(reply->errorString());
            }

            reply->deleteLater();

            emit partColorDetailsFinished(result);
        },
        [this, trimmedPartNumber, rebrickableColorId]() {
            PartColorDetailsResult result;

            result.success = false;

            result.httpStatusCode = 403;

            result.partColor.partNumber = trimmedPartNumber;

            result.partColor.rebrickableColorId = rebrickableColorId;

            result.message = sessionBlockReason();

            emit partColorDetailsFinished(result);
        },
        priority,
        QSet<int>{404},
        ApiLogPolicy::ErrorsOnly);
}

void RebrickableService::getPartImageUrls(const QStringList& partNumbers,
                                               const QString& apiKey,
                                               RequestPriority priority)
{
    PartImageUrlsResult invalidResult;

    if (isSessionBlocked()) {
        invalidResult.success = false;
        invalidResult.httpStatusCode = 403;
        invalidResult.message = sessionBlockReason();

        emit partImageUrlsFinished(invalidResult);
        return;
    }

    const QString trimmedApiKey = apiKey.trimmed();

    QStringList cleanedPartNumbers;
    cleanedPartNumbers.reserve(partNumbers.size());

    for (const QString& partNumber : partNumbers) {
        const QString trimmed = partNumber.trimmed();

        if (!trimmed.isEmpty() && !cleanedPartNumbers.contains(trimmed)) {
            cleanedPartNumbers.append(trimmed);
        }
    }

    if (cleanedPartNumbers.isEmpty() || trimmedApiKey.isEmpty()) {
        invalidResult.message = "Part numbers or Rebrickable API key are missing.";

        emit partImageUrlsFinished(invalidResult);
        return;
    }

    //
    // Rebrickable recommends using the parts list endpoint with
    // part_nums rather than issuing one API request per part.
    // The caller intentionally keeps each batch small.
    //
    QUrl url("https://rebrickable.com/api/v3/lego/parts/");

    QUrlQuery query;
    query.addQueryItem("part_nums", cleanedPartNumbers.join(','));
    query.addQueryItem("inc_part_details", "1");
    query.addQueryItem("inc_color_details", "0");
    query.addQueryItem("page_size", QString::number(cleanedPartNumbers.size()));
    url.setQuery(query);

    QNetworkRequest request(url);

    request.setRawHeader("Authorization", QString("key %1").arg(trimmedApiKey).toUtf8());
    request.setHeader(QNetworkRequest::UserAgentHeader, "BrickSuite/1.0");

    enqueueGet(
        request,
        QStringLiteral("GetPartImageUrls"),
        [this](QNetworkReply* reply) {
            PartImageUrlsResult result;

            const QVariant statusAttribute = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);

            if (statusAttribute.isValid()) {
                result.httpStatusCode = statusAttribute.toInt();
            }

            const QByteArray responseData = reply->readAll();

            QString circuitBreakerReason;

            if (result.httpStatusCode == 403
                && detectCloudflareIpBan(responseData, circuitBreakerReason)) {
                tripSessionCircuitBreaker(circuitBreakerReason);

                result.message = sessionBlockReason();

                reply->deleteLater();
                emit partImageUrlsFinished(result);
                return;
            }

            if (result.httpStatusCode == 429) {
                handle429();

                result.message = sessionBlockReason();

                reply->deleteLater();
                emit partImageUrlsFinished(result);
                return;
            }

            if (reply->error() == QNetworkReply::NoError && result.httpStatusCode == 200) {
                const QJsonDocument document = QJsonDocument::fromJson(responseData);

                if (!document.isObject()) {
                    result.message = "Rebrickable returned an unexpected parts response.";
                } else {
                    const QJsonObject root = document.object();
                    const QJsonArray parts = root.value("results").toArray();

                    for (const QJsonValue& value : parts) {
                        if (!value.isObject())
                            continue;

                        const QJsonObject partObject = value.toObject();

                        PartImageUrl part;
                        part.partNumber = partObject.value("part_num").toString().trimmed();
                        part.partImageUrl = partObject.value("part_img_url").toString().trimmed();

                        if (!part.partNumber.isEmpty()) {
                            result.parts.append(part);
                        }
                    }

                    result.success = true;
                    result.message = QString("Retrieved image information for %1 part(s).")
                                         .arg(result.parts.size());
                }
            } else if (result.httpStatusCode == 401) {
                result.message = "Rebrickable rejected the API key.";
            } else if (reply->error() != QNetworkReply::NoError) {
                result.message = QString("Unable to retrieve part image information: %1")
                                     .arg(reply->errorString());
            } else {
                result.message = QString("Rebrickable returned HTTP status %1.")
                                     .arg(result.httpStatusCode);
            }

            reply->deleteLater();
            emit partImageUrlsFinished(result);
        },
        [this]() {
            PartImageUrlsResult result;
            result.success = false;
            result.httpStatusCode = 403;
            result.message = sessionBlockReason();

            emit partImageUrlsFinished(result);
        },
        priority,
        {},
        ApiLogPolicy::ErrorsOnly);
}

void RebrickableService::getPartDetails(const QString& partNumber, const QString& apiKey)
{
    if (isSessionBlocked()) {
        PartDetailsResult result;

        result.success = false;
        result.httpStatusCode = 403;

        result.part.partNumber = partNumber.trimmed();

        result.message = sessionBlockReason();

        emit partDetailsFinished(result);

        return;
    }

    const QString trimmedPartNumber = partNumber.trimmed();

    const QString trimmedApiKey = apiKey.trimmed();

    if (trimmedPartNumber.isEmpty() || trimmedApiKey.isEmpty()) {
        PartDetailsResult result;

        result.part.partNumber = trimmedPartNumber;

        result.message = "Part number or Rebrickable API key is missing.";

        emit partDetailsFinished(result);

        return;
    }

    const QUrl url(QString("https://rebrickable.com/api/v3/lego/parts/%1/"
                           "?inc_part_details=1")
                       .arg(QUrl::toPercentEncoding(trimmedPartNumber)));

    QNetworkRequest request(url);

    request.setRawHeader("Authorization", QString("key %1").arg(trimmedApiKey).toUtf8());

    request.setHeader(QNetworkRequest::UserAgentHeader, "BrickSuite/1.0");

    enqueueGet(
        request,
        QStringLiteral("GetPartDetails"),

        //
        // Actual reply handler.
        //
        [this, trimmedPartNumber](QNetworkReply* reply) {
            PartDetailsResult result;

            result.part.partNumber = trimmedPartNumber;

            const QVariant statusAttribute = reply->attribute(
                QNetworkRequest::HttpStatusCodeAttribute);

            if (statusAttribute.isValid()) {
                result.httpStatusCode = statusAttribute.toInt();
            }

            const QByteArray responseData = reply->readAll();

            QString circuitBreakerReason;

            if (result.httpStatusCode == 403
                && detectCloudflareIpBan(responseData, circuitBreakerReason)) {
                tripSessionCircuitBreaker(circuitBreakerReason);

                result.message = sessionBlockReason();

                reply->deleteLater();

                emit partDetailsFinished(result);

                return;
            }

            if (result.httpStatusCode == 429) {
                handle429();

                result.message = sessionBlockReason();

                reply->deleteLater();

                emit partDetailsFinished(result);

                return;
            }

            if (reply->error() == QNetworkReply::NoError && result.httpStatusCode == 200) {
                const QJsonDocument document = QJsonDocument::fromJson(responseData);

                if (!document.isObject()) {
                    result.message = "Rebrickable returned an unexpected response.";
                } else {
                    const QJsonObject root = document.object();

                    PartDetails details;

                    details.partNumber = root.value("part_num").toString();

                    details.name = root.value("name").toString();

                    details.partCategoryId = root.value("part_cat_id").toInt();

                    details.yearFrom = root.value("year_from").toInt();

                    details.yearTo = root.value("year_to").toInt();

                    details.partUrl = root.value("part_url").toString();

                    details.partImageUrl = root.value("part_img_url").toString();

                    //
                    // Prints
                    //
                    const QJsonArray prints = root.value("prints").toArray();

                    for (const QJsonValue& value : prints) {
                        details.prints.append(value.toString());
                    }

                    //
                    // Molds
                    //
                    const QJsonArray molds = root.value("molds").toArray();

                    for (const QJsonValue& value : molds) {
                        details.molds.append(value.toString());
                    }

                    //
                    // Alternates
                    //
                    const QJsonArray alternates = root.value("alternates").toArray();

                    for (const QJsonValue& value : alternates) {
                        details.alternates.append(value.toString());
                    }

                    //
                    // External IDs
                    //
                    const QJsonObject externalIds = root.value("external_ids").toObject();

                    for (auto it = externalIds.constBegin(); it != externalIds.constEnd(); ++it) {
                        QStringList ids;

                        const QJsonArray values = it.value().toArray();

                        for (const QJsonValue& value : values) {
                            ids.append(value.toString());
                        }

                        details.externalIds.insert(it.key(), ids);
                    }

                    //
                    // print_of may be null.
                    //
                    if (!root.value("print_of").isNull()) {
                        details.printOf = root.value("print_of").toString();
                    }

                    result.part = details;

                    result.success = true;

                    result.message = QString("Part details retrieved for %1.")
                                         .arg(details.partNumber);
                }
            } else if (result.httpStatusCode == 401) {
                result.message = "Rebrickable rejected the API key.";
            } else if (result.httpStatusCode == 404) {
                result.message = "The part was not found on Rebrickable.";
            } else if (reply->error() != QNetworkReply::NoError) {
                result.message = QString("Unable to retrieve part details: %1")
                                     .arg(reply->errorString());
            } else {
                result.message = QString("Rebrickable returned HTTP status %1.")
                                     .arg(result.httpStatusCode);
            }

            reply->deleteLater();

            emit partDetailsFinished(result);
        },

        //
        // Called if this request is queued and another
        // request trips the circuit breaker first.
        //
        [this, trimmedPartNumber]() {
            PartDetailsResult result;

            result.success = false;
            result.httpStatusCode = 403;

            result.part.partNumber = trimmedPartNumber;

            result.message = sessionBlockReason();

            emit partDetailsFinished(result);
        });
}

bool RebrickableService::isSessionBlocked()
{
    return s_sessionBlocked;
}

QString RebrickableService::sessionBlockReason()
{
    return s_sessionBlockReason;
}

void RebrickableService::tripSessionCircuitBreaker(const QString& reason)
{
    s_sessionBlocked = true;

    s_sessionBlockReason = reason.trimmed();

    if (s_sessionBlockReason.isEmpty()) {
        s_sessionBlockReason = "Rebrickable access has been disabled "
                               "for this BrickSuite session.";
    }
}

bool RebrickableService::detectCloudflareIpBan(const QByteArray& responseData, QString& reason)
{
    if (responseData.isEmpty())
        return false;

    QJsonParseError parseError;

    const QJsonDocument document = QJsonDocument::fromJson(responseData, &parseError);

    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return false;
    }

    const QJsonObject root = document.object();

    const int errorCode = root.value("error_code").toInt();

    const QString errorName = root.value("error_name").toString();

    const bool cloudflareError = root.value("cloudflare_error").toBool();

    if (errorCode != 1006 && errorName.compare("ip_banned", Qt::CaseInsensitive) != 0) {
        return false;
    }

    if (!cloudflareError && errorCode != 1006) {
        return false;
    }

    const QString detail = root.value("detail").toString();

    const QString rayId = root.value("ray_id").toString();

    reason = "Rebrickable access is blocked for the "
             "current network/IP address.";

    if (!detail.isEmpty()) {
        reason += QString("\n\n%1").arg(detail);
    }

    if (!rayId.isEmpty()) {
        reason += QString("\n\nCloudflare Ray ID: %1").arg(rayId);
    }

    reason += "\n\nBrickSuite will not make any more "
              "Rebrickable API requests during this session.";

    return true;
}

void RebrickableService::ensureRequestTimer()
{
    if (s_requestTimer)
        return;

    s_requestTimer = new QTimer(QCoreApplication::instance());

    s_requestTimer->setSingleShot(true);

    QObject::connect(s_requestTimer, &QTimer::timeout, []() {
        RebrickableService::processRequestQueue();
    });
}

void RebrickableService::enqueueRequest(QueuedRequest request, RequestPriority priority)
{
    ensureRequestTimer();

    if (priority == RequestPriority::Background) {
        s_backgroundRequestQueue.enqueue(std::move(request));
    } else {
        s_foregroundRequestQueue.enqueue(std::move(request));
    }

    processRequestQueue();
}

void RebrickableService::processRequestQueue()
{
    ensureRequestTimer();

    const bool queuesEmpty = s_foregroundRequestQueue.isEmpty()
                             && s_backgroundRequestQueue.isEmpty();

    if (queuesEmpty)
        return;

    //
    // If the circuit breaker has tripped, drain both
    // queues without dispatching network requests.
    //
    if (isSessionBlocked()) {
        while (!s_foregroundRequestQueue.isEmpty()) {
            QueuedRequest request = s_foregroundRequestQueue.dequeue();

            request();
        }

        while (!s_backgroundRequestQueue.isEmpty()) {
            QueuedRequest request = s_backgroundRequestQueue.dequeue();

            request();
        }

        return;
    }

    const int minimumIntervalMs = UserSettings::instance().rebrickableMinimumRequestIntervalMs();

    if (s_lastRequestTimer.isValid()) {
        const qint64 elapsedMs = s_lastRequestTimer.elapsed();

        if (elapsedMs < minimumIntervalMs) {
            const int remainingMs = minimumIntervalMs - static_cast<int>(elapsedMs);

            s_requestTimer->start(remainingMs);

            return;
        }
    }

    //
    // Interactive requests always win.
    //
    QueuedRequest request;

    if (!s_foregroundRequestQueue.isEmpty()) {
        request = s_foregroundRequestQueue.dequeue();
    } else {
        request = s_backgroundRequestQueue.dequeue();
    }

    if (s_lastRequestTimer.isValid()) {
        s_lastRequestTimer.restart();
    } else {
        s_lastRequestTimer.start();
    }

    request();

    if (!s_foregroundRequestQueue.isEmpty() || !s_backgroundRequestQueue.isEmpty()) {
        s_requestTimer->start(minimumIntervalMs);
    }
}

void RebrickableService::enqueueGet(const QNetworkRequest& request,
                                      const QString& operation,
                                      ReplyHandler replyHandler,
                                      std::function<void()> blockedHandler,
                                      RequestPriority priority,
                                      const QSet<int>& expectedHttpStatusCodes,
                                      ApiLogPolicy logPolicy)
{
    QPointer<RebrickableService> self(this);

    enqueueRequest(
        [self,
         request,
         operation,
         expectedHttpStatusCodes,
         logPolicy,
         replyHandler = std::move(replyHandler),
         blockedHandler = std::move(blockedHandler)]() mutable {
            if (!self)
                return;

            if (isSessionBlocked()) {
                if (blockedHandler)
                    blockedHandler();

                return;
            }

            QNetworkReply* reply = self->m_networkService->get(
                request,
                ApiRequestContext{ApiProvider::Rebrickable,
                                  operation,
                                  QString(),
                                  expectedHttpStatusCodes,
                                  logPolicy});

            QObject::connect(reply,
                             &QNetworkReply::finished,
                             self,
                             [self, reply, replyHandler = std::move(replyHandler)]() mutable {
                                 if (!self) {
                                     reply->deleteLater();
                                     return;
                                 }

                                 replyHandler(reply);
                             });
        },
        priority);
}

void RebrickableService::handle429()
{
    tripSessionCircuitBreaker("Rebrickable returned HTTP 429 because the API "
                              "request rate was throttled.\n\n"
                              "BrickSuite will not automatically retry the request "
                              "and will not make any more Rebrickable API requests "
                              "during this session.");
}

void RebrickableService::getSetDetails(const QString& setNumber, const QString& apiKey)
{
    const QString trimmedSetNumber = setNumber.trimmed();

    if (isSessionBlocked()) {
        SetDetailsResult result;

        result.success = false;
        result.httpStatusCode = 403;
        result.set.setNumber = trimmedSetNumber;

        result.message = sessionBlockReason();

        emit setDetailsFinished(result);

        return;
    }

    const QString trimmedApiKey = apiKey.trimmed();

    if (trimmedSetNumber.isEmpty() || trimmedApiKey.isEmpty()) {
        SetDetailsResult result;

        result.set.setNumber = trimmedSetNumber;

        result.message = "Set number or Rebrickable API key is missing.";

        emit setDetailsFinished(result);

        return;
    }

    const QUrl url(QString("https://rebrickable.com/api/v3/lego/sets/%1/")
                       .arg(QUrl::toPercentEncoding(trimmedSetNumber)));

    QNetworkRequest request(url);

    request.setRawHeader("Authorization", QString("key %1").arg(trimmedApiKey).toUtf8());

    request.setHeader(QNetworkRequest::UserAgentHeader, "BrickSuite/1.0");

    enqueueGet(
        request,
        QStringLiteral("GetSetDetails"),

        [this, trimmedSetNumber](QNetworkReply* reply) {
            SetDetailsResult result;

            result.set.setNumber = trimmedSetNumber;

            const QVariant statusAttribute = reply->attribute(
                QNetworkRequest::HttpStatusCodeAttribute);

            if (statusAttribute.isValid()) {
                result.httpStatusCode = statusAttribute.toInt();
            }

            const QByteArray responseData = reply->readAll();

            QString circuitBreakerReason;

            if (result.httpStatusCode == 403
                && detectCloudflareIpBan(responseData, circuitBreakerReason)) {
                tripSessionCircuitBreaker(circuitBreakerReason);

                result.message = sessionBlockReason();

                reply->deleteLater();

                emit setDetailsFinished(result);

                return;
            }

            if (result.httpStatusCode == 429) {
                handle429();

                result.message = sessionBlockReason();

                reply->deleteLater();

                emit setDetailsFinished(result);

                return;
            }

            if (reply->error() == QNetworkReply::NoError && result.httpStatusCode == 200) {
                const QJsonDocument document = QJsonDocument::fromJson(responseData);

                if (!document.isObject()) {
                    result.message = "Rebrickable returned an unexpected "
                                     "Set Details response.";
                } else {
                    const QJsonObject root = document.object();

                    SetDetails details;

                    details.setNumber = root.value("set_num").toString();

                    details.name = root.value("name").toString();

                    details.year = root.value("year").toInt();

                    details.themeId = root.value("theme_id").toInt();

                    details.numberOfParts = root.value("num_parts").toInt();

                    details.setImageUrl = root.value("set_img_url").toString();

                    details.setUrl = root.value("set_url").toString();

                    details.lastModifiedUtc = root.value("last_modified_dt").toString();

                    result.set = details;

                    result.success = true;

                    result.message = QString("Set details retrieved for %1.").arg(details.setNumber);
                }
            } else if (result.httpStatusCode == 401) {
                result.message = "Rebrickable rejected the API key.";
            } else if (result.httpStatusCode == 404) {
                result.message = QString("Set %1 was not found on Rebrickable.")
                                     .arg(trimmedSetNumber);
            } else if (reply->error() != QNetworkReply::NoError) {
                result.message = QString("Unable to retrieve Set Details: %1")
                                     .arg(reply->errorString());
            } else {
                result.message = QString("Rebrickable returned HTTP status %1.")
                                     .arg(result.httpStatusCode);
            }

            reply->deleteLater();

            emit setDetailsFinished(result);
        },

        [this, trimmedSetNumber]() {
            SetDetailsResult result;

            result.success = false;
            result.httpStatusCode = 403;

            result.set.setNumber = trimmedSetNumber;

            result.message = sessionBlockReason();

            emit setDetailsFinished(result);
        });
}

void RebrickableService::getSetParts(const QString& setNumber, const QString& apiKey)
{
    const QString trimmedSetNumber = setNumber.trimmed();

    if (isSessionBlocked()) {
        SetPartsResult result;

        result.success = false;
        result.httpStatusCode = 403;
        result.setNumber = trimmedSetNumber;

        result.message = sessionBlockReason();

        emit setPartsFinished(result);

        return;
    }

    const QString trimmedApiKey = apiKey.trimmed();

    if (trimmedSetNumber.isEmpty() || trimmedApiKey.isEmpty()) {
        SetPartsResult result;

        result.setNumber = trimmedSetNumber;

        result.message = "Set number or Rebrickable API key is missing.";

        emit setPartsFinished(result);

        return;
    }

    const QUrl url(QString("https://rebrickable.com/api/v3/lego/sets/"
                           "%1/parts/?page=1&page_size=500")
                       .arg(QUrl::toPercentEncoding(trimmedSetNumber)));

    QNetworkRequest request(url);

    request.setRawHeader("Authorization", QString("key %1").arg(trimmedApiKey).toUtf8());

    request.setHeader(QNetworkRequest::UserAgentHeader, "BrickSuite/1.0");

    enqueueGet(
        request,
        QStringLiteral("GetSetParts"),

        [this, trimmedSetNumber](QNetworkReply* reply) {
            SetPartsResult result;

            result.setNumber = trimmedSetNumber;

            const QVariant statusAttribute = reply->attribute(
                QNetworkRequest::HttpStatusCodeAttribute);

            if (statusAttribute.isValid()) {
                result.httpStatusCode = statusAttribute.toInt();
            }

            const QByteArray responseData = reply->readAll();

            QString circuitBreakerReason;

            if (result.httpStatusCode == 403
                && detectCloudflareIpBan(responseData, circuitBreakerReason)) {
                tripSessionCircuitBreaker(circuitBreakerReason);

                result.message = sessionBlockReason();

                reply->deleteLater();

                emit setPartsFinished(result);

                return;
            }

            if (result.httpStatusCode == 429) {
                handle429();

                result.message = sessionBlockReason();

                reply->deleteLater();

                emit setPartsFinished(result);

                return;
            }

            if (reply->error() == QNetworkReply::NoError && result.httpStatusCode == 200) {
                const QJsonDocument document = QJsonDocument::fromJson(responseData);

                if (!document.isObject()) {
                    result.message = "Rebrickable returned an unexpected "
                                     "Set Parts response.";
                } else {
                    const QJsonObject root = document.object();

                    result.totalCount = root.value("count").toInt();

                    if (!root.value("next").isNull()) {
                        result.nextUrl = root.value("next").toString();
                    }

                    if (!root.value("previous").isNull()) {
                        result.previousUrl = root.value("previous").toString();
                    }

                    const QJsonArray results = root.value("results").toArray();

                    for (const QJsonValue& value : results) {
                        if (!value.isObject())
                            continue;

                        const QJsonObject object = value.toObject();

                        const QJsonObject partObject = object.value("part").toObject();

                        const QJsonObject colorObject = object.value("color").toObject();

                        SetPart setPart;

                        //
                        // Rebrickable returned both id and
                        // inv_part_id with the same value in
                        // the supplied sample. We retain
                        // inv_part_id because that is the
                        // explicit provider inventory-part ID.
                        //
                        setPart.inventoryPartId = object.value("inv_part_id").toInt();

                        setPart.setNumber = object.value("set_num").toString();

                        setPart.partNumber = partObject.value("part_num").toString();

                        setPart.partName = partObject.value("name").toString();

                        setPart.partCategoryId = partObject.value("part_cat_id").toInt();

                        setPart.partUrl = partObject.value("part_url").toString();

                        setPart.partImageUrl = partObject.value("part_img_url").toString();

                        setPart.rebrickableColorId = colorObject.value("id").toInt();

                        setPart.colorName = colorObject.value("name").toString();

                        setPart.colorRgb = colorObject.value("rgb").toString();

                        setPart.colorIsTransparent = colorObject.value("is_trans").toBool();

                        setPart.quantity = object.value("quantity").toInt();

                        setPart.isSpare = object.value("is_spare").toBool();

                        setPart.elementId = object.value("element_id").toString();

                        setPart.numberOfSets = object.value("num_sets").toInt();

                        result.parts.append(setPart);
                    }

                    result.success = true;

                    result.message = QString("%1 Set Part rows retrieved "
                                             "for %2.")
                                         .arg(result.parts.size())
                                         .arg(trimmedSetNumber);
                }
            } else if (result.httpStatusCode == 401) {
                result.message = "Rebrickable rejected the API key.";
            } else if (result.httpStatusCode == 404) {
                result.message = QString("Set %1 was not found on Rebrickable.")
                                     .arg(trimmedSetNumber);
            } else if (reply->error() != QNetworkReply::NoError) {
                result.message = QString("Unable to retrieve Set Parts: %1")
                                     .arg(reply->errorString());
            } else {
                result.message = QString("Rebrickable returned HTTP status %1.")
                                     .arg(result.httpStatusCode);
            }

            reply->deleteLater();

            emit setPartsFinished(result);
        },

        [this, trimmedSetNumber]() {
            SetPartsResult result;

            result.success = false;
            result.httpStatusCode = 403;
            result.setNumber = trimmedSetNumber;

            result.message = sessionBlockReason();

            emit setPartsFinished(result);
        });
}