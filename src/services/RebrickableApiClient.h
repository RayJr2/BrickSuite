#pragma once

#include <QHash>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

class QNetworkAccessManager;
class QNetworkReply;

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

    explicit RebrickableApiClient(QObject* parent = nullptr);

    void testConnection(const QString& apiKey);

    void getPartColors(const QString& partNumber, const QString& apiKey);

    void getPartDetails(const QString& partNumber, const QString& apiKey);

signals:
    void connectionTestFinished(const RebrickableApiClient::ConnectionResult& result);

    void partColorsFinished(const RebrickableApiClient::PartColorsResult& result);

    void partDetailsFinished(const RebrickableApiClient::PartDetailsResult& result);

private:
    void handleConnectionTestReply(QNetworkReply* reply);

    QNetworkAccessManager* m_networkManager = nullptr;
};

Q_DECLARE_METATYPE(RebrickableApiClient::ConnectionResult)
Q_DECLARE_METATYPE(RebrickableApiClient::PartColor)
Q_DECLARE_METATYPE(RebrickableApiClient::PartColorsResult)
Q_DECLARE_METATYPE(RebrickableApiClient::PartDetails)
Q_DECLARE_METATYPE(RebrickableApiClient::PartDetailsResult)