/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 *
 * This file is part of BrickSuite.
 *
 * BrickSuite is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, version 3 of the License.
 *
 * BrickSuite is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with BrickSuite. If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPointer>
#include <QTimer>
#include <cstring>

// Controlled delivery includes late signals on abort, exercising cleanup ordering.
class SupportFixtureReply final : public QNetworkReply
{
public:
    QByteArray data;
    qint64 offset = 0;
    bool aborted = false;
    SupportFixtureReply(const QNetworkRequest& request, QObject* parent, int status,
        QByteArray bytes, NetworkError error, int delay, qint64 length, bool redirect)
        : QNetworkReply(parent), data(std::move(bytes))
    {
        setRequest(request); setUrl(request.url()); setOperation(QNetworkAccessManager::GetOperation);
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute, status);
        if (length >= 0) setHeader(QNetworkRequest::ContentLengthHeader, length);
        if (redirect) setAttribute(QNetworkRequest::RedirectionTargetAttribute, QUrl("https://rfstateside.com/elsewhere"));
        open(QIODevice::ReadOnly);
        if (delay >= 0) QTimer::singleShot(delay, this, [this, error] {
            if (aborted) return;
            emit metaDataChanged();
            if (aborted) return;
            if (error != NoError) setError(error, "Synthetic network failure");
            emit readyRead();
            if (aborted) return;
            setFinished(true); emit finished();
        });
    }
    void abort() override { aborted = true; setFinished(true); emit readyRead(); emit finished(); }
    qint64 bytesAvailable() const override { return data.size() - offset + QNetworkReply::bytesAvailable(); }
protected:
    qint64 readData(char* target, qint64 maximum) override {
        const auto count = qMin(maximum, qint64(data.size()) - offset);
        if (!count) return -1;
        std::memcpy(target, data.constData() + offset, size_t(count)); offset += count; return count;
    }
};
class SupportFixtureNetwork final : public QNetworkAccessManager
{
public:
    int calls = 0, status = 200, delay = 0;
    qint64 length = -1;
    QByteArray body = "rfss-checkout-v1";
    QNetworkReply::NetworkError error = QNetworkReply::NoError;
    bool redirect = false, sentBody = false;
    Operation operation = UnknownOperation;
    QNetworkRequest request;
    QPointer<SupportFixtureReply> reply;
protected:
    QNetworkReply* createRequest(Operation op, const QNetworkRequest& req, QIODevice* outgoing) override {
        ++calls; operation = op; request = req; sentBody = outgoing != nullptr;
        reply = new SupportFixtureReply(req, this, status, body, error, delay, length, redirect);
        return reply;
    }
};
