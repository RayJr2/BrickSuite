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

#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QElapsedTimer>
#include <QUrl>
class QNetworkAccessManager;
class QNetworkReply;

// An explicit-action-only selector. No UI, persistent state or provider queues.
class SupportAvailabilityService final : public QObject
{
    Q_OBJECT
public:
    explicit SupportAvailabilityService(QObject* parent = nullptr,
        QNetworkAccessManager* network = nullptr, int deadlineMs = 2500);
    ~SupportAvailabilityService() override;
    void start();
    void cancel();
    bool busy() const { return !m_reply.isNull(); }
signals:
    void selected(const QUrl& url);
private:
    bool consume();
    void complete();
    void settle(bool available);
    QNetworkAccessManager* m_network;
    QPointer<QNetworkReply> m_reply;
    QTimer m_deadline;
    QElapsedTimer m_elapsed;
    QByteArray m_body;
    int m_deadlineMs;
};
