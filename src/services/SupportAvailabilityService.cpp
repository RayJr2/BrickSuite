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

#include "SupportAvailabilityService.h"
#include "../core/AppConstants.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

namespace { constexpr qint64 BodyLimit = 64; }

SupportAvailabilityService::SupportAvailabilityService(QObject* parent,
    QNetworkAccessManager* network, int deadlineMs)
    : QObject(parent), m_network(network ? network : new QNetworkAccessManager(this)),
      m_deadlineMs(deadlineMs)
{
    m_deadline.setSingleShot(true);
    m_deadline.setTimerType(Qt::PreciseTimer);
    connect(&m_deadline, &QTimer::timeout, this, [this] { settle(false); });
}
SupportAvailabilityService::~SupportAvailabilityService() { cancel(); }

void SupportAvailabilityService::cancel()
{
    m_deadline.stop();
    auto* reply = m_reply.data();
    m_reply.clear();
    m_body.clear();
    if (reply) {
        reply->disconnect(this);
        if (!reply->isFinished()) reply->abort();
        reply->deleteLater();
    }
}

void SupportAvailabilityService::start()
{
    if (busy()) return;
    m_body.clear();
    m_elapsed.start();
    m_deadline.start(m_deadlineMs);
    QNetworkRequest request(QUrl(QString::fromLatin1(AppConstants::SupportAvailabilityUrl)));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setAttribute(QNetworkRequest::CookieLoadControlAttribute, QNetworkRequest::Manual);
    request.setAttribute(QNetworkRequest::CookieSaveControlAttribute, QNetworkRequest::Manual);
    request.setAttribute(QNetworkRequest::AuthenticationReuseAttribute, QNetworkRequest::Manual);
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
    request.setAttribute(QNetworkRequest::CacheSaveControlAttribute, false);
    request.setRawHeader("Accept", "text/plain");
    request.setRawHeader("Accept-Encoding", "identity");
    request.setTransferTimeout(m_deadlineMs);
    auto* reply = m_network->get(request);
    m_reply = reply;
    reply->setReadBufferSize(BodyLimit + 1);
    connect(reply, &QIODevice::readyRead, this, [this, reply] {
        if (m_reply == reply && !consume()) settle(false);
    });
    connect(reply, &QNetworkReply::metaDataChanged, this, [this, reply] {
        if (m_reply == reply && (reply->header(QNetworkRequest::ContentLengthHeader).toLongLong() > BodyLimit
            || reply->attribute(QNetworkRequest::RedirectionTargetAttribute).isValid())) settle(false);
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        if (m_reply == reply) complete();
    });
    // Also support transports that return an already-finished reply.
    QTimer::singleShot(0, this, [this, reply = QPointer<QNetworkReply>(reply)] {
        if (!reply || m_reply != reply) return;
        if (reply->isFinished()) complete();
        else if (!consume()) settle(false);
    });
}

bool SupportAvailabilityService::consume()
{
    if (m_elapsed.elapsed() >= m_deadlineMs
        || m_reply->header(QNetworkRequest::ContentLengthHeader).toLongLong() > BodyLimit) return false;
    m_body += m_reply->read(BodyLimit + 1 - m_body.size());
    return m_body.size() <= BodyLimit && m_reply->bytesAvailable() == 0;
}
void SupportAvailabilityService::complete()
{
    const bool available = consume() && m_reply->error() == QNetworkReply::NoError
        && m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200
        && !m_reply->attribute(QNetworkRequest::RedirectionTargetAttribute).isValid()
        && m_body == QByteArrayLiteral("rfss-checkout-v1");
    settle(available);
}
void SupportAvailabilityService::settle(bool available)
{
    if (!busy()) return;
    const QUrl url(QString::fromLatin1(available ? AppConstants::SupportCheckoutUrl
                                               : AppConstants::SupportPayPalFallbackUrl));
    cancel(); // Invalidate and dispose of the network operation before external callbacks.
    emit selected(url);
}
