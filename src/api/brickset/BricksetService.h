/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 */
#pragma once

#include "../ApiError.h"

#include <QObject>
#include <QString>

class ApiNetworkService;
class QNetworkReply;

class BricksetService : public QObject
{
    Q_OBJECT

public:
    struct ConnectionResult
    {
        bool success = false;
        int httpStatusCode = 0;
        QString message;
        ApiError error;
    };

    explicit BricksetService(QObject* parent = nullptr);

    void testConnection(const QString& apiKey);

signals:
    void connectionTestFinished(const BricksetService::ConnectionResult& result);

private:
    void handleConnectionTestReply(QNetworkReply* reply);

    ApiNetworkService* m_networkService = nullptr;
};

Q_DECLARE_METATYPE(BricksetService::ConnectionResult)
