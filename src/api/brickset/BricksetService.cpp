/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 */
#include "BricksetService.h"

#include "../ApiNetworkService.h"
#include "../ApiProvider.h"
#include "../ApiRequestContext.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>

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
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("BrickSuite/0.2.0"));

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
