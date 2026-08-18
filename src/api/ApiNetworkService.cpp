/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 */
#include "ApiNetworkService.h"

#include <QDebug>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

ApiNetworkService::ApiNetworkService(QObject* parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
{
}

QNetworkReply* ApiNetworkService::get(const QNetworkRequest& request,
                                      const ApiRequestContext& context)
{
    QNetworkReply* reply = m_networkManager->get(request);
    attachCompletionLogging(reply, context);
    return reply;
}

QNetworkReply* ApiNetworkService::post(const QNetworkRequest& request,
                                       const QByteArray& body,
                                       const ApiRequestContext& context)
{
    QNetworkReply* reply = m_networkManager->post(request, body);
    attachCompletionLogging(reply, context);
    return reply;
}

void ApiNetworkService::attachCompletionLogging(QNetworkReply* reply,
                                                const ApiRequestContext& context)
{
    if (!reply)
        return;

    connect(reply, &QNetworkReply::finished, this, [reply, context]() {
        const QVariant statusAttribute =
            reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);

        const int httpStatusCode = statusAttribute.isValid()
                                       ? statusAttribute.toInt()
                                       : 0;

        // Some provider lookups legitimately use an HTTP status such as 404 to
        // mean "no provider-specific record exists".  The caller identifies
        // those statuses in the request context so routine lookup misses do not
        // flood BrickSuite's application log as apparent network failures.
        if (context.expectedHttpStatusCodes.contains(httpStatusCode))
            return;

        const bool succeeded = reply->error() == QNetworkReply::NoError
                               && httpStatusCode >= 200
                               && httpStatusCode < 400;

        // Routine successful HTTP traffic is intentionally quiet. Provider
        // services log meaningful user-facing milestones where useful, while
        // the shared network layer reports only unexpected failures.
        if (succeeded)
            return;

        qWarning() << "API request failed."
                   << "Provider:" << apiProviderName(context.provider)
                   << "Operation:" << context.operation
                   << "HTTP:" << httpStatusCode
                   << "NetworkError:" << static_cast<int>(reply->error());
    });
}
