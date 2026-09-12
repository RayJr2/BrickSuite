#pragma once

#include "BrickSuiteConnectionState.h"

#include <QObject>
#include <functional>
#include <memory>

class BrickSuiteWebSocketClient;
class BrickSuiteWebSocketServer;
class HostReadProtocolService;
class RemoteReadApplicationServices;
class RemoteMutationApplicationServices;
class RemotePullingApplicationService;
class RemoteInventoryMutationApplicationService;
class RemoteStorageMutationApplicationService;
class RemotePartReferenceMutationApplicationService;
class RemoteCollectionMutationApplicationService;
class RemoteBuildMutationApplicationService;
class HostMutationProtocolService;
class HostMaintenanceCoordinator;
class RemoteSessionState;
class OperationalInvalidationPublisher;
class BrickSuitePairingService;
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
    void quiesceForDatabaseRestore(std::function<void(bool, const QString&)> completion);
    BrickSuiteWebSocketServer* server() const;
    BrickSuiteWebSocketClient* client() const;
    RemoteReadApplicationServices* remoteReads() const;
    RemoteMutationApplicationServices* remoteMutations() const;
    RemotePullingApplicationService* remotePulling() const;
    RemoteInventoryMutationApplicationService* remoteInventoryMutations() const;
    RemoteStorageMutationApplicationService* remoteStorageMutations() const;
    RemotePartReferenceMutationApplicationService* remotePartReferenceMutations() const;
    RemoteCollectionMutationApplicationService* remoteCollectionMutations() const;
    RemoteBuildMutationApplicationService* remoteBuildMutations() const;
    RemoteSessionState* remoteSession() const;
    OperationalInvalidationPublisher* invalidationPublisher() const;
    HostMaintenanceCoordinator* maintenanceCoordinator() const;
    BrickSuiteConnectionStatus connectionStatus() const;
    QString serverStatusText() const;
    QString generateOrRotateHostToken(QString* error = nullptr);
    bool saveClientToken(const QString& token, QString* error = nullptr);
    QString clientToken(QString* error = nullptr) const;
    bool savePairedClientCredential(const QString& credential, QString* error = nullptr);
    QString pairedClientCredential(QString* error = nullptr) const;
    BrickSuitePairingService* pairingService() const;

signals:
    void statusChanged();
    void invalidationReceived(const OperationalInvalidation& invalidation);
    void remotePullingMutationCommitted(int workspaceId, int buildId);
    void remoteInventoryMutationCommitted(int workspaceId);
    void remoteStorageMutationCommitted(int workspaceId, int storageId);
    void remotePartReferenceMutationCommitted(const QString& partNumber);
    void remoteCollectionMutationCommitted(int workspaceId, int collectionItemId);
    void remoteBuildMutationCommitted(int workspaceId, int buildId,
                                      bool inventoryChanged,
                                      bool collectionChanged,
                                      bool pullingAffected);

private:
    BrickSuiteWebSocketServer* m_server = nullptr;
    BrickSuiteWebSocketClient* m_client = nullptr;
    QString m_serverError;
    std::unique_ptr<HostReadProtocolService> m_hostReads;
    std::unique_ptr<HostMutationProtocolService> m_hostMutations;
    std::unique_ptr<HostMaintenanceCoordinator> m_maintenanceCoordinator;
    std::unique_ptr<RemoteReadApplicationServices> m_remoteReads;
    std::unique_ptr<RemoteMutationApplicationServices> m_remoteMutations;
    std::unique_ptr<RemotePullingApplicationService> m_remotePulling;
    std::unique_ptr<RemoteInventoryMutationApplicationService> m_remoteInventoryMutations;
    std::unique_ptr<RemoteStorageMutationApplicationService> m_remoteStorageMutations;
    std::unique_ptr<RemotePartReferenceMutationApplicationService> m_remotePartReferenceMutations;
    std::unique_ptr<RemoteCollectionMutationApplicationService> m_remoteCollectionMutations;
    std::unique_ptr<RemoteBuildMutationApplicationService> m_remoteBuildMutations;
    std::unique_ptr<RemoteSessionState> m_remoteSession;
    std::unique_ptr<OperationalInvalidationPublisher> m_invalidationPublisher;
};
