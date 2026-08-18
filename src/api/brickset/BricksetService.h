/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 */
#pragma once

#include "../ApiError.h"

#include <QObject>
#include <QString>
#include <QList>

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

    struct Instruction
    {
        QString url;
        QString description;
    };

    struct InstructionsResult
    {
        bool success = false;
        int httpStatusCode = 0;
        int matches = 0;
        QString setNumber;
        QString message;
        ApiError error;
        QList<Instruction> instructions;
    };

    struct KeyUsageEntry
    {
        QString dateStamp;
        int count = 0;
    };

    struct KeyUsageResult
    {
        bool success = false;
        int httpStatusCode = 0;
        int matches = 0;
        int todayCount = 0;
        QString message;
        ApiError error;
        QList<KeyUsageEntry> entries;
    };

    explicit BricksetService(QObject* parent = nullptr);

    void testConnection(const QString& apiKey);

    void getSetDetails(const QString& fullSetNumber,
                       const QString& apiKey);

    void getKeyUsageStats(const QString& apiKey);

    void getInstructions2(const QString& setNumber,
                          const QString& apiKey);

    static int sessionGetSetsCallCount();

    static bool keyUsageKnown();
    static int authoritativeTodayGetSetsCount();
    static int effectiveTodayGetSetsCount();
    static void invalidateKeyUsageCache();

signals:
    void connectionTestFinished(const BricksetService::ConnectionResult& result);

    void setDetailsFinished(const BricksetService::SetDetailsResult& result);

    void keyUsageStatsFinished(const BricksetService::KeyUsageResult& result);

    void instructionsFinished(const BricksetService::InstructionsResult& result);

private:
    void handleConnectionTestReply(QNetworkReply* reply);

    void handleSetDetailsReply(QNetworkReply* reply,
                               const QString& requestedSetNumber);

    void handleKeyUsageStatsReply(QNetworkReply* reply);

    void handleInstructionsReply(QNetworkReply* reply,
                                 const QString& setNumber);

    ApiNetworkService* m_networkService = nullptr;

    static int s_sessionGetSetsCallCount;
    static bool s_keyUsageKnown;
    static QString s_keyUsageDate;
    static int s_authoritativeTodayGetSetsCount;
    static int s_sessionGetSetsCountAtUsageRefresh;
};

Q_DECLARE_METATYPE(BricksetService::ConnectionResult)
Q_DECLARE_METATYPE(BricksetService::SetDetails)
Q_DECLARE_METATYPE(BricksetService::SetDetailsResult)

Q_DECLARE_METATYPE(BricksetService::KeyUsageEntry)
Q_DECLARE_METATYPE(BricksetService::KeyUsageResult)

Q_DECLARE_METATYPE(BricksetService::Instruction)
Q_DECLARE_METATYPE(BricksetService::InstructionsResult)

