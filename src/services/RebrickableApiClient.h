#pragma once

#include <QList>
#include <QObject>
#include <QString>

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

    explicit RebrickableApiClient(QObject* parent = nullptr);

    void testConnection(const QString& apiKey);

    void getPartColors(const QString& partNumber, const QString& apiKey);

signals:
    void connectionTestFinished(const RebrickableApiClient::ConnectionResult& result);

    void partColorsFinished(const RebrickableApiClient::PartColorsResult& result);

private:
    void handleConnectionTestReply(QNetworkReply* reply);

    QNetworkAccessManager* m_networkManager = nullptr;
};

Q_DECLARE_METATYPE(RebrickableApiClient::ConnectionResult)
Q_DECLARE_METATYPE(RebrickableApiClient::PartColor)
Q_DECLARE_METATYPE(RebrickableApiClient::PartColorsResult)