/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 */
#pragma once

#include "ApiRequestContext.h"

#include <QObject>

class QByteArray;
class QNetworkAccessManager;
class QNetworkReply;
class QNetworkRequest;

class ApiNetworkService : public QObject
{
    Q_OBJECT

public:
    explicit ApiNetworkService(QObject* parent = nullptr);

    QNetworkReply* get(const QNetworkRequest& request,
                       const ApiRequestContext& context);

    QNetworkReply* post(const QNetworkRequest& request,
                        const QByteArray& body,
                        const ApiRequestContext& context);

private:
    void attachCompletionLogging(QNetworkReply* reply,
                                 const ApiRequestContext& context);

    QNetworkAccessManager* m_networkManager = nullptr;
};
