/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 */
#include "BricksetService.h"

#include "../ApiNetworkService.h"
#include "../ApiProvider.h"
#include "../ApiRequestContext.h"
#include "../../core/AppVersion.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>

namespace {
QByteArray brickSuiteUserAgent()
{
    return QStringLiteral("BrickSuite/%1").arg(AppVersion::version()).toUtf8();
}
}

int BricksetService::s_sessionGetSetsCallCount = 0;
bool BricksetService::s_keyUsageKnown = false;
QString BricksetService::s_keyUsageDate;
int BricksetService::s_authoritativeTodayGetSetsCount = 0;
int BricksetService::s_sessionGetSetsCountAtUsageRefresh = 0;

BricksetService::BricksetService(QObject* parent)
    : QObject(parent)
    , m_networkService(new ApiNetworkService(this))
{
}

void BricksetService::testConnection(const QString& apiKey)
{
    const QString trimmedApiKey = apiKey.trimmed();

    if (trimmedApiKey.isEmpty()) {
        ConnectionResult result;
        result.message = QStringLiteral("Brickset API key is empty.");
        result.error.type = ApiErrorType::Configuration;
        result.error.message = result.message;

        emit connectionTestFinished(result);
        return;
    }

    QUrl url(QStringLiteral("https://brickset.com/api/v3.asmx/checkKey"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("apiKey"), trimmedApiKey);
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, brickSuiteUserAgent());

    ApiRequestContext context;
    context.provider = ApiProvider::Brickset;
    context.operation = QStringLiteral("CheckKey");

    QNetworkReply* reply = m_networkService->get(request, context);

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        handleConnectionTestReply(reply);
    });
}

void BricksetService::handleConnectionTestReply(QNetworkReply* reply)
{
    ConnectionResult result;

    const QVariant statusAttribute =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);

    if (statusAttribute.isValid())
        result.httpStatusCode = statusAttribute.toInt();

    result.error.httpStatusCode = result.httpStatusCode;

    const QByteArray responseData = reply->readAll();

    if (reply->error() != QNetworkReply::NoError) {
        result.message =
            QStringLiteral("Unable to connect to Brickset: %1").arg(reply->errorString());
        result.error.type = ApiErrorType::Network;
        result.error.message = result.message;

        reply->deleteLater();
        emit connectionTestFinished(result);
        return;
    }

    const QJsonDocument document = QJsonDocument::fromJson(responseData);

    if (!document.isObject()) {
        result.message = QStringLiteral("Brickset returned an unexpected response.");
        result.error.type = ApiErrorType::InvalidResponse;
        result.error.message = result.message;

        reply->deleteLater();
        emit connectionTestFinished(result);
        return;
    }

    const QJsonObject root = document.object();
    const QString status = root.value(QStringLiteral("status")).toString();
    const QString providerMessage = root.value(QStringLiteral("message")).toString();

    if (status.compare(QStringLiteral("success"), Qt::CaseInsensitive) == 0) {
        result.success = true;
        result.message = QStringLiteral("Connection to Brickset succeeded.");
    } else if (providerMessage.contains(QStringLiteral("Invalid API key"),
                                        Qt::CaseInsensitive)) {
        result.message = QStringLiteral("Brickset rejected the API key.");
        result.error.type = ApiErrorType::Authentication;
        result.error.message = result.message;
        result.error.providerMessage = providerMessage;
    } else {
        result.message = providerMessage.isEmpty()
                             ? QStringLiteral("Brickset returned a provider error.")
                             : providerMessage;
        result.error.type = ApiErrorType::Provider;
        result.error.message = result.message;
        result.error.providerMessage = providerMessage;
    }

    reply->deleteLater();
    emit connectionTestFinished(result);
}

void BricksetService::getSetDetails(const QString& fullSetNumber,
                                    const QString& apiKey)
{
    const QString trimmedSetNumber = fullSetNumber.trimmed();
    const QString trimmedApiKey = apiKey.trimmed();

    if (trimmedSetNumber.isEmpty() || trimmedApiKey.isEmpty()) {
        SetDetailsResult result;
        result.requestedSetNumber = trimmedSetNumber;
        result.message = trimmedSetNumber.isEmpty()
                             ? QStringLiteral("Brickset set number is empty.")
                             : QStringLiteral("Brickset API key is empty.");
        result.error.type = ApiErrorType::Configuration;
        result.error.message = result.message;

        emit setDetailsFinished(result);
        return;
    }

    QJsonObject paramsObject;
    paramsObject.insert(QStringLiteral("setNumber"), trimmedSetNumber);
    paramsObject.insert(QStringLiteral("extendedData"), 1);

    const QString params =
        QString::fromUtf8(QJsonDocument(paramsObject).toJson(QJsonDocument::Compact));

    QNetworkRequest request(
        QUrl(QStringLiteral("https://brickset.com/api/v3.asmx/getSets")));

    request.setHeader(QNetworkRequest::UserAgentHeader, brickSuiteUserAgent());
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/x-www-form-urlencoded"));

    // Brickset documents userHash as optional for normal catalog lookups,
    // but the ASP.NET web-service method still requires the parameter name to
    // be present. Build the form body explicitly so an empty value is emitted
    // as "userHash=" rather than being omitted or serialized ambiguously.
    const QByteArray body =
        QByteArrayLiteral("apiKey=")
        + QUrl::toPercentEncoding(trimmedApiKey)
        + QByteArrayLiteral("&userHash=&params=")
        + QUrl::toPercentEncoding(params);

    ApiRequestContext context;
    context.provider = ApiProvider::Brickset;
    context.operation = QStringLiteral("GetSetDetails");

    ++s_sessionGetSetsCallCount;

    QNetworkReply* reply = m_networkService->post(request, body, context);

    connect(reply, &QNetworkReply::finished, this,
            [this, reply, trimmedSetNumber]() {
                handleSetDetailsReply(reply, trimmedSetNumber);
            });
}

void BricksetService::handleSetDetailsReply(QNetworkReply* reply,
                                            const QString& requestedSetNumber)
{
    SetDetailsResult result;
    result.requestedSetNumber = requestedSetNumber;

    const QVariant statusAttribute =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);

    if (statusAttribute.isValid())
        result.httpStatusCode = statusAttribute.toInt();

    result.error.httpStatusCode = result.httpStatusCode;

    const QByteArray responseData = reply->readAll();

    const QJsonDocument document = QJsonDocument::fromJson(responseData);
    const bool hasJsonObject = document.isObject();

    QJsonObject root;
    QString status;
    QString providerMessage;

    if (hasJsonObject) {
        root = document.object();
        status = root.value(QStringLiteral("status")).toString();
        providerMessage = root.value(QStringLiteral("message")).toString();
    }

    // QNetworkReply reports HTTP 4xx/5xx as an error. Prefer a Brickset JSON
    // error message when one is present; otherwise preserve the transport
    // diagnostic and include a small response-body hint for troubleshooting.
    if (reply->error() != QNetworkReply::NoError) {
        if (!providerMessage.isEmpty()) {
            result.message = providerMessage;
            result.error.type =
                providerMessage.contains(QStringLiteral("Invalid API key"),
                                         Qt::CaseInsensitive)
                    ? ApiErrorType::Authentication
                    : ApiErrorType::Provider;
            result.error.providerMessage = providerMessage;
        } else {
            QString bodyHint = QString::fromUtf8(responseData).trimmed();

            // Avoid flooding UI/logs if the server returns an HTML error page.
            if (bodyHint.size() > 500)
                bodyHint = bodyHint.left(500) + QStringLiteral("...");

            result.message =
                QStringLiteral("Unable to retrieve set details from Brickset: %1")
                    .arg(reply->errorString());

            if (!bodyHint.isEmpty()) {
                result.message += QStringLiteral("\n\nBrickset response: %1")
                                      .arg(bodyHint);
            }

            result.error.type = ApiErrorType::Network;
        }

        result.error.message = result.message;

        reply->deleteLater();
        emit setDetailsFinished(result);
        return;
    }

    if (!hasJsonObject) {
        result.message = QStringLiteral("Brickset returned an unexpected response.");
        result.error.type = ApiErrorType::InvalidResponse;
        result.error.message = result.message;

        reply->deleteLater();
        emit setDetailsFinished(result);
        return;
    }

    if (status.compare(QStringLiteral("success"), Qt::CaseInsensitive) != 0) {
        result.message = providerMessage.isEmpty()
                             ? QStringLiteral("Brickset returned a provider error.")
                             : providerMessage;

        if (providerMessage.contains(QStringLiteral("Invalid API key"),
                                     Qt::CaseInsensitive)) {
            result.error.type = ApiErrorType::Authentication;
        } else if (providerMessage.contains(QStringLiteral("API limit exceeded"),
                                            Qt::CaseInsensitive)) {
            result.error.type = ApiErrorType::RateLimit;
        } else {
            result.error.type = ApiErrorType::Provider;
        }
        result.error.message = result.message;
        result.error.providerMessage = providerMessage;

        reply->deleteLater();
        emit setDetailsFinished(result);
        return;
    }

    result.matches = root.value(QStringLiteral("matches")).toInt();

    const QJsonArray sets = root.value(QStringLiteral("sets")).toArray();

    if (result.matches < 1 || sets.isEmpty() || !sets.first().isObject()) {
        result.message =
            QStringLiteral("Set %1 was not found on Brickset.").arg(requestedSetNumber);
        result.error.type = ApiErrorType::Provider;
        result.error.message = result.message;

        reply->deleteLater();
        emit setDetailsFinished(result);
        return;
    }

    const QJsonObject object = sets.first().toObject();

    SetDetails details;
    details.bricksetSetId = object.value(QStringLiteral("setID")).toInt();
    details.number = object.value(QStringLiteral("number")).toString();
    details.numberVariant = object.value(QStringLiteral("numberVariant")).toInt();

    if (!details.number.isEmpty() && details.numberVariant > 0) {
        details.fullSetNumber =
            QStringLiteral("%1-%2").arg(details.number).arg(details.numberVariant);
    } else {
        details.fullSetNumber = requestedSetNumber;
    }

    details.name = object.value(QStringLiteral("name")).toString();
    details.year = object.value(QStringLiteral("year")).toInt();

    details.theme = object.value(QStringLiteral("theme")).toString();
    details.themeGroup = object.value(QStringLiteral("themeGroup")).toString();
    details.subtheme = object.value(QStringLiteral("subtheme")).toString();
    details.category = object.value(QStringLiteral("category")).toString();

    details.released = object.value(QStringLiteral("released")).toBool();

    details.pieces = object.value(QStringLiteral("pieces")).toInt();
    details.minifigs = object.value(QStringLiteral("minifigs")).toInt();

    details.launchDate = object.value(QStringLiteral("launchDate")).toString();
    details.exitDate = object.value(QStringLiteral("exitDate")).toString();

    const QJsonObject image = object.value(QStringLiteral("image")).toObject();
    details.thumbnailUrl = image.value(QStringLiteral("thumbnailURL")).toString();
    details.imageUrl = image.value(QStringLiteral("imageURL")).toString();

    details.bricksetUrl = object.value(QStringLiteral("bricksetURL")).toString();

    details.rating = object.value(QStringLiteral("rating")).toDouble();
    details.ratingCount = object.value(QStringLiteral("ratingCount")).toInt();
    details.reviewCount = object.value(QStringLiteral("reviewCount")).toInt();

    details.packagingType = object.value(QStringLiteral("packagingType")).toString();
    details.availability = object.value(QStringLiteral("availability")).toString();

    details.instructionsCount =
        object.value(QStringLiteral("instructionsCount")).toInt();
    details.additionalImageCount =
        object.value(QStringLiteral("additionalImageCount")).toInt();

    const QJsonObject ageRange = object.value(QStringLiteral("ageRange")).toObject();
    details.minimumAge = ageRange.value(QStringLiteral("min")).toInt();

    const QJsonObject barcode = object.value(QStringLiteral("barcode")).toObject();
    details.ean = barcode.value(QStringLiteral("EAN")).toString();
    details.upc = barcode.value(QStringLiteral("UPC")).toString();

    const QJsonObject extendedData =
        object.value(QStringLiteral("extendedData")).toObject();
    details.descriptionHtml =
        extendedData.value(QStringLiteral("description")).toString();

    details.lastUpdated = object.value(QStringLiteral("lastUpdated")).toString();

    result.set = details;
    result.success = true;
    result.message =
        QStringLiteral("Brickset set details retrieved for %1.").arg(details.fullSetNumber);

    reply->deleteLater();
    emit setDetailsFinished(result);
}

int BricksetService::sessionGetSetsCallCount()
{
    return s_sessionGetSetsCallCount;
}

void BricksetService::getKeyUsageStats(const QString& apiKey)
{
    const QString trimmedApiKey = apiKey.trimmed();

    if (trimmedApiKey.isEmpty()) {
        KeyUsageResult result;
        result.message = QStringLiteral("Brickset API key is empty.");
        result.error.type = ApiErrorType::Configuration;
        result.error.message = result.message;

        emit keyUsageStatsFinished(result);
        return;
    }

    QUrl url(QStringLiteral("https://brickset.com/api/v3.asmx/getKeyUsageStats"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("apiKey"), trimmedApiKey);
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, brickSuiteUserAgent());

    ApiRequestContext context;
    context.provider = ApiProvider::Brickset;
    context.operation = QStringLiteral("GetKeyUsageStats");

    QNetworkReply* reply = m_networkService->get(request, context);

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        handleKeyUsageStatsReply(reply);
    });
}

void BricksetService::handleKeyUsageStatsReply(QNetworkReply* reply)
{
    KeyUsageResult result;

    const QVariant statusAttribute =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);

    if (statusAttribute.isValid())
        result.httpStatusCode = statusAttribute.toInt();

    result.error.httpStatusCode = result.httpStatusCode;

    const QByteArray responseData = reply->readAll();

    if (reply->error() != QNetworkReply::NoError) {
        result.message =
            QStringLiteral("Unable to retrieve Brickset key usage statistics: %1")
                .arg(reply->errorString());
        result.error.type = ApiErrorType::Network;
        result.error.message = result.message;

        reply->deleteLater();
        emit keyUsageStatsFinished(result);
        return;
    }

    const QJsonDocument document = QJsonDocument::fromJson(responseData);

    if (!document.isObject()) {
        result.message = QStringLiteral("Brickset returned an unexpected usage response.");
        result.error.type = ApiErrorType::InvalidResponse;
        result.error.message = result.message;

        reply->deleteLater();
        emit keyUsageStatsFinished(result);
        return;
    }

    const QJsonObject root = document.object();
    const QString status = root.value(QStringLiteral("status")).toString();
    const QString providerMessage = root.value(QStringLiteral("message")).toString();

    if (status.compare(QStringLiteral("success"), Qt::CaseInsensitive) != 0) {
        result.message = providerMessage.isEmpty()
                             ? QStringLiteral("Brickset returned a provider error.")
                             : providerMessage;
        result.error.type =
            providerMessage.contains(QStringLiteral("Invalid API key"), Qt::CaseInsensitive)
                ? ApiErrorType::Authentication
                : ApiErrorType::Provider;
        result.error.message = result.message;
        result.error.providerMessage = providerMessage;

        reply->deleteLater();
        emit keyUsageStatsFinished(result);
        return;
    }

    result.matches = root.value(QStringLiteral("matches")).toInt();

    const QString today = QDateTime::currentDateTimeUtc().date().toString(Qt::ISODate);
    const QJsonArray usage = root.value(QStringLiteral("apiKeyUsage")).toArray();

    for (const QJsonValue& value : usage) {
        if (!value.isObject())
            continue;

        const QJsonObject object = value.toObject();

        KeyUsageEntry entry;
        entry.dateStamp = object.value(QStringLiteral("dateStamp")).toString();
        entry.count = object.value(QStringLiteral("count")).toInt();

        result.entries.append(entry);

        if (entry.dateStamp.startsWith(today))
            result.todayCount = entry.count;
    }

    result.success = true;

    s_keyUsageKnown = true;
    s_keyUsageDate = today;
    s_authoritativeTodayGetSetsCount = result.todayCount;
    s_sessionGetSetsCountAtUsageRefresh = s_sessionGetSetsCallCount;

    result.message =
        QStringLiteral("Brickset key usage statistics retrieved. Today's getSets count: %1.")
            .arg(result.todayCount);

    reply->deleteLater();
    emit keyUsageStatsFinished(result);
}

bool BricksetService::keyUsageKnown()
{
    return s_keyUsageKnown
           && s_keyUsageDate == QDateTime::currentDateTimeUtc().date().toString(Qt::ISODate);
}

int BricksetService::authoritativeTodayGetSetsCount()
{
    return keyUsageKnown() ? s_authoritativeTodayGetSetsCount : -1;
}

int BricksetService::effectiveTodayGetSetsCount()
{
    if (!keyUsageKnown())
        return -1;

    const int callsSinceRefresh =
        qMax(0, s_sessionGetSetsCallCount - s_sessionGetSetsCountAtUsageRefresh);

    return s_authoritativeTodayGetSetsCount + callsSinceRefresh;
}

void BricksetService::invalidateKeyUsageCache()
{
    s_keyUsageKnown = false;
    s_keyUsageDate.clear();
    s_authoritativeTodayGetSetsCount = 0;
    s_sessionGetSetsCountAtUsageRefresh = s_sessionGetSetsCallCount;
}

void BricksetService::getInstructions2(const QString& setNumber,
                                       const QString& apiKey)
{
    const QString trimmedSetNumber = setNumber.trimmed();
    const QString trimmedApiKey = apiKey.trimmed();

    if (trimmedSetNumber.isEmpty() || trimmedApiKey.isEmpty()) {
        InstructionsResult result;
        result.setNumber = trimmedSetNumber;
        result.message = trimmedSetNumber.isEmpty()
                             ? QStringLiteral("Brickset set number is empty.")
                             : QStringLiteral("Brickset API key is empty.");
        result.error.type = ApiErrorType::Configuration;
        result.error.message = result.message;

        emit instructionsFinished(result);
        return;
    }

    QUrl url(QStringLiteral("https://brickset.com/api/v3.asmx/getInstructions2"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("apiKey"), trimmedApiKey);
    query.addQueryItem(QStringLiteral("setNumber"), trimmedSetNumber);
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, brickSuiteUserAgent());

    ApiRequestContext context;
    context.provider = ApiProvider::Brickset;
    context.operation = QStringLiteral("GetInstructions2");

    QNetworkReply* reply = m_networkService->get(request, context);

    connect(reply, &QNetworkReply::finished, this,
            [this, reply, trimmedSetNumber]() {
                handleInstructionsReply(reply, trimmedSetNumber);
            });
}

void BricksetService::handleInstructionsReply(QNetworkReply* reply,
                                              const QString& setNumber)
{
    InstructionsResult result;
    result.setNumber = setNumber;

    const QVariant statusAttribute =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);

    if (statusAttribute.isValid())
        result.httpStatusCode = statusAttribute.toInt();

    result.error.httpStatusCode = result.httpStatusCode;

    const QByteArray responseData = reply->readAll();
    const QJsonDocument document = QJsonDocument::fromJson(responseData);
    const bool hasJsonObject = document.isObject();

    QJsonObject root;
    QString status;
    QString providerMessage;

    if (hasJsonObject) {
        root = document.object();
        status = root.value(QStringLiteral("status")).toString();
        providerMessage = root.value(QStringLiteral("message")).toString();
    }

    if (reply->error() != QNetworkReply::NoError) {
        result.message =
            !providerMessage.isEmpty()
                ? providerMessage
                : QStringLiteral("Unable to retrieve Brickset instructions: %1")
                      .arg(reply->errorString());

        if (providerMessage.contains(QStringLiteral("Invalid API key"),
                                     Qt::CaseInsensitive)) {
            result.error.type = ApiErrorType::Authentication;
        } else {
            result.error.type = ApiErrorType::Network;
        }

        result.error.message = result.message;
        result.error.providerMessage = providerMessage;

        reply->deleteLater();
        emit instructionsFinished(result);
        return;
    }

    if (!hasJsonObject) {
        result.message = QStringLiteral("Brickset returned an unexpected instructions response.");
        result.error.type = ApiErrorType::InvalidResponse;
        result.error.message = result.message;

        reply->deleteLater();
        emit instructionsFinished(result);
        return;
    }

    if (status.compare(QStringLiteral("success"), Qt::CaseInsensitive) != 0) {
        result.message = providerMessage.isEmpty()
                             ? QStringLiteral("Brickset returned a provider error.")
                             : providerMessage;

        result.error.type =
            providerMessage.contains(QStringLiteral("Invalid API key"),
                                     Qt::CaseInsensitive)
                ? ApiErrorType::Authentication
                : ApiErrorType::Provider;

        result.error.message = result.message;
        result.error.providerMessage = providerMessage;

        reply->deleteLater();
        emit instructionsFinished(result);
        return;
    }

    result.matches = root.value(QStringLiteral("matches")).toInt();

    const QJsonArray instructions =
        root.value(QStringLiteral("instructions")).toArray();

    for (const QJsonValue& value : instructions) {
        if (!value.isObject())
            continue;

        const QJsonObject object = value.toObject();

        Instruction instruction;
        instruction.url = object.value(QStringLiteral("URL")).toString();
        instruction.description =
            object.value(QStringLiteral("description")).toString();

        if (!instruction.url.isEmpty())
            result.instructions.append(instruction);
    }

    result.success = true;
    result.message =
        QStringLiteral("Brickset returned %1 instruction file(s) for %2.")
            .arg(result.instructions.size())
            .arg(setNumber);

    reply->deleteLater();
    emit instructionsFinished(result);
}
