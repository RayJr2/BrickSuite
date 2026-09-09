#pragma once

#include "OperationalInvalidation.h"
#include <QObject>

class BrickSuiteWebSocketServer;

class OperationalInvalidationPublisher : public QObject
{
    Q_OBJECT
public:
    explicit OperationalInvalidationPublisher(BrickSuiteWebSocketServer& server,
                                               QObject* parent = nullptr);
    void publish(const OperationalInvalidation& invalidation);

private:
    BrickSuiteWebSocketServer& m_server;
};
