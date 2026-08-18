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

    struct SetDetails
    {
        int bricksetSetId = 0;

        QString number;
        int numberVariant = 0;
        QString fullSetNumber;

        QString name;
        int year = 0;

        QString theme;
        QString themeGroup;
        QString subtheme;
        QString category;

        bool released = false;

        int pieces = 0;
        int minifigs = 0;

        QString launchDate;
        QString exitDate;

        QString thumbnailUrl;
        QString imageUrl;
        QString bricksetUrl;

        double rating = 0.0;
        int ratingCount = 0;
        int reviewCount = 0;

        QString packagingType;
        QString availability;

        int instructionsCount = 0;
        int additionalImageCount = 0;

        int minimumAge = 0;

        QString ean;
        QString upc;

        QString descriptionHtml;
        QString lastUpdated;
    };

    struct SetDetailsResult
    {
        bool success = false;
        int httpStatusCode = 0;
        int matches = 0;
        QString requestedSetNumber;
        QString message;
        ApiError error;
        SetDetails set;
    };

    explicit BricksetService(QObject* parent = nullptr);

    void testConnection(const QString& apiKey);

    void getSetDetails(const QString& fullSetNumber,
                       const QString& apiKey);

    static int sessionGetSetsCallCount();

signals:
    void connectionTestFinished(const BricksetService::ConnectionResult& result);

    void setDetailsFinished(const BricksetService::SetDetailsResult& result);

private:
    void handleConnectionTestReply(QNetworkReply* reply);

    void handleSetDetailsReply(QNetworkReply* reply,
                               const QString& requestedSetNumber);

    ApiNetworkService* m_networkService = nullptr;

    static int s_sessionGetSetsCallCount;
};

Q_DECLARE_METATYPE(BricksetService::ConnectionResult)
Q_DECLARE_METATYPE(BricksetService::SetDetails)
Q_DECLARE_METATYPE(BricksetService::SetDetailsResult)
