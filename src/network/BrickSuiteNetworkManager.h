#pragma once

#include "BrickSuiteConnectionState.h"

#include <QObject>
#include <memory>

class BrickSuiteWebSocketClient;
class BrickSuiteWebSocketServer;
class HostReadProtocolService;

class BrickSuiteNetworkManager : public QObject
{
    Q_OBJECT
public:
    explicit BrickSuiteNetworkManager(QObject* parent = nullptr);
    ~BrickSuiteNetworkManager() override;

    void startConfiguredMode();
    bool restartServer(QString* error = nullptr);
    void stop();
    BrickSuiteWebSocketServer* server() const;
    BrickSuiteWebSocketClient* client() const;
    BrickSuiteConnectionStatus connectionStatus() const;
    QString serverStatusText() const;
    QString generateOrRotateHostToken(QString* error = nullptr);
    bool saveClientToken(const QString& token, QString* error = nullptr);
    QString clientToken(QString* error = nullptr) const;

signals:
    void statusChanged();

private:
    BrickSuiteWebSocketServer* m_server = nullptr;
    BrickSuiteWebSocketClient* m_client = nullptr;
    QString m_serverError;
    std::unique_ptr<HostReadProtocolService> m_hostReads;
};
