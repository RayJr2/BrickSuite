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

    void getSetDetails(const QString& setNumber, const QString& apiKey);

    void getSetParts(const QString& setNumber, const QString& apiKey);

    static bool isSessionBlocked();

    static QString sessionBlockReason();

signals:
    void connectionTestFinished(const RebrickableApiClient::ConnectionResult& result);

    void partColorsFinished(const RebrickableApiClient::PartColorsResult& result);

    void partDetailsFinished(const RebrickableApiClient::PartDetailsResult& result);

    void setDetailsFinished(const RebrickableApiClient::SetDetailsResult& result);

    void setPartsFinished(const RebrickableApiClient::SetPartsResult& result);

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
                    std::function<void()> blockedHandler);

    static void enqueueRequest(QueuedRequest request);

    static void processRequestQueue();

    static void ensureRequestTimer();

    static void handle429();

    static QQueue<QueuedRequest> s_requestQueue;

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