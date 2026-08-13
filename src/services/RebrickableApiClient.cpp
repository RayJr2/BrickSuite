#include "RebrickableApiClient.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

RebrickableApiClient::RebrickableApiClient(QObject* parent)
    : QObject(parent)
{
    m_networkManager = new QNetworkAccessManager(this);
}

void RebrickableApiClient::testConnection(const QString& apiKey)
{
    const QString trimmedApiKey = apiKey.trimmed();

    if (trimmedApiKey.isEmpty()) {
        ConnectionResult result;

        result.success = false;
        result.message = "Rebrickable API key is empty.";

        emit connectionTestFinished(result);

        return;
    }

    QNetworkRequest request(QUrl("https://rebrickable.com/api/v3/lego/colors/"));

    request.setRawHeader("Authorization", QString("key %1").arg(trimmedApiKey).toUtf8());

    request.setHeader(QNetworkRequest::UserAgentHeader, "BrickSuite/1.0");

    QNetworkReply* reply = m_networkManager->get(request);

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        handleConnectionTestReply(reply);
    });
}

void RebrickableApiClient::handleConnectionTestReply(QNetworkReply* reply)
{
    ConnectionResult result;

    const QVariant statusAttribute = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);

    if (statusAttribute.isValid()) {
        result.httpStatusCode = statusAttribute.toInt();
    }

    const QByteArray responseData = reply->readAll();

    if (reply->error() == QNetworkReply::NoError && result.httpStatusCode == 200) {
        const QJsonDocument document = QJsonDocument::fromJson(responseData);

        if (!document.isObject()) {
            result.message = "Rebrickable returned an unexpected response.";
        } else {
            const QJsonObject root = document.object();

            if (!root.contains("results")) {
                result.message = "Rebrickable returned an unexpected response.";
            } else {
                result.success = true;

                result.message = "Connection to Rebrickable succeeded.";
            }
        }
    } else if (result.httpStatusCode == 401) {
        result.message = "Rebrickable rejected the API key.";
    } else if (result.httpStatusCode == 403) {
        result.message = "Rebrickable denied access to this request.";
    } else if (result.httpStatusCode == 429) {
        result.message = "Rebrickable is temporarily throttling API requests. "
                         "Please wait and try again.";
    } else if (reply->error() != QNetworkReply::NoError) {
        result.message = QString("Unable to connect to Rebrickable: %1").arg(reply->errorString());
    } else {
        result.message = QString("Rebrickable returned HTTP status %1.").arg(result.httpStatusCode);
    }

    reply->deleteLater();

    emit connectionTestFinished(result);
}

void RebrickableApiClient::getPartColors(const QString& partNumber, const QString& apiKey)
{
    const QString trimmedPartNumber = partNumber.trimmed();

    const QString trimmedApiKey = apiKey.trimmed();

    if (trimmedPartNumber.isEmpty() || trimmedApiKey.isEmpty()) {
        PartColorsResult result;

        result.partNumber = trimmedPartNumber;

        result.message = "Part number or Rebrickable API key is missing.";

        emit partColorsFinished(result);

        return;
    }

    const QUrl url(QString("https://rebrickable.com/api/v3/lego/parts/%1/colors/")
                       .arg(QUrl::toPercentEncoding(trimmedPartNumber)));

    QNetworkRequest request(url);

    request.setRawHeader("Authorization", QString("key %1").arg(trimmedApiKey).toUtf8());

    request.setHeader(QNetworkRequest::UserAgentHeader, "BrickSuite/1.0");

    QNetworkReply* reply = m_networkManager->get(request);

    connect(reply, &QNetworkReply::finished, this, [this, reply, trimmedPartNumber, trimmedApiKey]() {
        PartColorsResult result;

        result.partNumber = trimmedPartNumber;

        const QVariant statusAttribute = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);

        if (statusAttribute.isValid()) {
            result.httpStatusCode = statusAttribute.toInt();
        }

        const QByteArray responseData = reply->readAll();

        if (reply->error() == QNetworkReply::NoError && result.httpStatusCode == 200) {
            const QJsonDocument document = QJsonDocument::fromJson(responseData);

            if (document.isObject()) {
                const QJsonObject root = document.object();

                const QJsonArray results = root.value("results").toArray();

                for (const QJsonValue& value : results) {
                    if (!value.isObject())
                        continue;

                    const QJsonObject object = value.toObject();

                    PartColor color;

                    color.rebrickableColorId = object.value("color_id").toInt();

                    color.name = object.value("color_name").toString();

                    result.colors.append(color);
                }

                result.success = true;

                result.message = QString("%1 known colors found.").arg(result.colors.size());
            } else {
                result.message = "Rebrickable returned an unexpected response.";
            }
        } else if (result.httpStatusCode == 401) {
            result.message = "Rebrickable rejected the API key.";
        } else if (result.httpStatusCode == 404) {
            result.message = "The part was not found on Rebrickable.";
        } else if (result.httpStatusCode == 429) {
            result.message = "Rebrickable is temporarily throttling requests.";
        } else {
            result.message = QString("Unable to retrieve part colors: %1").arg(reply->errorString());
        }

        reply->deleteLater();

        emit partColorsFinished(result);
    });
}

void RebrickableApiClient::getPartDetails(const QString& partNumber, const QString& apiKey)
{
    const QString trimmedPartNumber = partNumber.trimmed();

    const QString trimmedApiKey = apiKey.trimmed();

    if (trimmedPartNumber.isEmpty() || trimmedApiKey.isEmpty()) {
        PartDetailsResult result;

        result.part.partNumber = trimmedPartNumber;

        result.message = "Part number or Rebrickable API key is missing.";

        emit partDetailsFinished(result);

        return;
    }

    const QUrl url(QString("https://rebrickable.com/api/v3/lego/parts/%1/")
                       .arg(QUrl::toPercentEncoding(trimmedPartNumber)));

    QNetworkRequest request(url);

    request.setRawHeader("Authorization", QString("key %1").arg(trimmedApiKey).toUtf8());

    request.setHeader(QNetworkRequest::UserAgentHeader, "BrickSuite/1.0");

    QNetworkReply* reply = m_networkManager->get(request);

    connect(reply, &QNetworkReply::finished, this, [this, reply, trimmedPartNumber]() {
        PartDetailsResult result;

        result.part.partNumber = trimmedPartNumber;

        const QVariant statusAttribute = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);

        if (statusAttribute.isValid()) {
            result.httpStatusCode = statusAttribute.toInt();
        }

        const QByteArray responseData = reply->readAll();

        if (reply->error() == QNetworkReply::NoError && result.httpStatusCode == 200) {
            const QJsonDocument document = QJsonDocument::fromJson(responseData);

            if (!document.isObject()) {
                result.message = "Rebrickable returned an unexpected response.";
            } else {
                const QJsonObject root = document.object();

                PartDetails details;

                details.partNumber = root.value("part_num").toString();

                details.name = root.value("name").toString();

                details.partCategoryId = root.value("part_cat_id").toInt();

                details.yearFrom = root.value("year_from").toInt();

                details.yearTo = root.value("year_to").toInt();

                details.partUrl = root.value("part_url").toString();

                details.partImageUrl = root.value("part_img_url").toString();

                //
                // Prints
                //
                const QJsonArray prints = root.value("prints").toArray();

                for (const QJsonValue& value : prints) {
                    details.prints.append(value.toString());
                }

                //
                // Molds
                //
                const QJsonArray molds = root.value("molds").toArray();

                for (const QJsonValue& value : molds) {
                    details.molds.append(value.toString());
                }

                //
                // Alternates
                //
                const QJsonArray alternates = root.value("alternates").toArray();

                for (const QJsonValue& value : alternates) {
                    details.alternates.append(value.toString());
                }

                //
                // External IDs
                //
                const QJsonObject externalIds = root.value("external_ids").toObject();

                for (auto it = externalIds.constBegin(); it != externalIds.constEnd(); ++it) {
                    QStringList ids;

                    const QJsonArray values = it.value().toArray();

                    for (const QJsonValue& value : values) {
                        ids.append(value.toString());
                    }

                    details.externalIds.insert(it.key(), ids);
                }

                //
                // print_of may be null.
                //
                if (!root.value("print_of").isNull()) {
                    details.printOf = root.value("print_of").toString();
                }

                result.part = details;

                result.success = true;

                result.message = QString("Part details retrieved for %1.").arg(details.partNumber);
            }
        } else if (result.httpStatusCode == 401) {
            result.message = "Rebrickable rejected the API key.";
        } else if (result.httpStatusCode == 404) {
            result.message = "The part was not found on Rebrickable.";
        } else if (result.httpStatusCode == 429) {
            result.message = "Rebrickable is temporarily throttling requests.";
        } else if (reply->error() != QNetworkReply::NoError) {
            result.message = QString("Unable to retrieve part details: %1").arg(reply->errorString());
        } else {
            result.message = QString("Rebrickable returned HTTP status %1.")
                                 .arg(result.httpStatusCode);
        }

        reply->deleteLater();

        emit partDetailsFinished(result);
    });
}