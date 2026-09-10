#pragma once

#include "BrickSuiteConnectionState.h"

#include <QObject>
#include <memory>

class BrickSuiteWebSocketClient;
class BrickSuiteWebSocketServer;
class HostReadProtocolService;
class RemoteReadApplicationServices;
class RemoteMutationApplicationServices;
class RemotePullingApplicationService;
class HostMutationProtocolService;
class RemoteSessionState;
class OperationalInvalidationPublisher;
struct OperationalInvalidation;

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
    RemoteReadApplicationServices* remoteReads() const;
    RemoteMutationApplicationServices* remoteMutations() const;
    RemotePullingApplicationService* remotePulling() const;
    RemoteSessionState* remoteSession() const;
    OperationalInvalidationPublisher* invalidationPublisher() const;
    BrickSuiteConnectionStatus connectionStatus() const;
    QString serverStatusText() const;
    QString generateOrRotateHostToken(QString* error = nullptr);
    bool saveClientToken(const QString& token, QString* error = nullptr);
    QString clientToken(QString* error = nullptr) const;

signals:
    void statusChanged();
    void invalidationReceived(const OperationalInvalidation& invalidation);
    void remotePullingMutationCommitted(int workspaceId, int buildId);

private:
    BrickSuiteWebSocketServer* m_server = nullptr;
    BrickSuiteWebSocketClient* m_client = nullptr;
    QString m_serverError;
    std::unique_ptr<HostReadProtocolService> m_hostReads;
    std::unique_ptr<HostMutationProtocolService> m_hostMutations;
    std::unique_ptr<RemoteReadApplicationServices> m_remoteReads;
    std::unique_ptr<RemoteMutationApplicationServices> m_remoteMutations;
    std::unique_ptr<RemotePullingApplicationService> m_remotePulling;
    std::unique_ptr<RemoteSessionState> m_remoteSession;
    std::unique_ptr<OperationalInvalidationPublisher> m_invalidationPublisher;
};
