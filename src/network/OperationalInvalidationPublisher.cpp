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
