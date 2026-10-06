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

#include "OperationalInvalidationPublisher.h"
#include "BrickSuiteWebSocketServer.h"

#include <QMetaObject>
#include <QPointer>
#include <QThread>

OperationalInvalidationPublisher::OperationalInvalidationPublisher(
    BrickSuiteWebSocketServer& server, QObject* parent)
    : QObject(parent), m_server(server)
{}

void OperationalInvalidationPublisher::publish(const OperationalInvalidation& invalidation)
{
    if (QThread::currentThread() == m_server.thread()) {
        m_server.broadcastInvalidation(invalidation);
        return;
    }
    QPointer<BrickSuiteWebSocketServer> server(&m_server);
    QMetaObject::invokeMethod(&m_server, [server, invalidation]() {
        if (server) server->broadcastInvalidation(invalidation);
    }, Qt::QueuedConnection);
}
