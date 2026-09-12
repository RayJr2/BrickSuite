#include "BrickSuiteNetworkManager.h"

#include "BrickSuiteAuthentication.h"
#include "BrickSuiteWebSocketClient.h"
#include "BrickSuiteWebSocketServer.h"
#include "RemoteSessionState.h"
#include "OperationalInvalidationPublisher.h"
#include "HostDataEpoch.h"
#include "../database/DatabaseManager.h"
#include "../services/application/HostReadProtocolService.h"
#include "../services/application/RemoteReadApplicationServices.h"
#include "../services/application/RemoteMutationApplicationServices.h"
#include "../services/application/HostMutationProtocolService.h"
#include "../services/application/HostPullingMutationService.h"
#include "../services/application/HostInventoryMutationService.h"
#include "../services/application/HostStorageProtocolMutationService.h"
#include "../services/application/RemotePullingApplicationService.h"
#include "../services/application/RemoteInventoryMutationApplicationService.h"
#include "../services/application/RemoteStorageMutationApplicationService.h"
#include "../services/application/RemotePartReferenceMutationApplicationService.h"
#include "../services/application/RemoteCollectionMutationApplicationService.h"
#include "../services/application/HostPartReferenceMutationService.h"
#include "../services/application/HostCollectionMutationService.h"
#include "../services/application/HostBuildMutationService.h"
#include "../services/application/RemoteBuildMutationApplicationService.h"
#include "../services/application/HostMutationPublicationService.h"
#include "../services/application/HostMaintenanceCoordinator.h"
#include "../services/application/HostReadExecutor.h"
#include "../services/application/HostWriteExecutor.h"
#include "../services/CredentialStore.h"
#include "../settings/UserSettings.h"

#include <QHostAddress>

namespace {
constexpr auto kHostTokenCredential = "BrickSuiteHostAccessToken";
constexpr auto kClientTokenCredential = "BrickSuiteClientAccessToken";
}

BrickSuiteNetworkManager::BrickSuiteNetworkManager(QObject* parent)
    : QObject(parent)
    , m_server(new BrickSuiteWebSocketServer(this))
    , m_client(new BrickSuiteWebSocketClient(this))
{
    const HostDataEpoch::LoadResult epoch = HostDataEpoch::loadExisting();
    if (epoch.success) {
        m_server->operationDispatcher().setDataEpoch(epoch.epoch);
    } else {
        m_serverError = epoch.error;
        qCritical().noquote() << "BrickSuite Host disabled:" << m_serverError;
    }
    m_remoteSession = std::make_unique<RemoteSessionState>(this);
    m_invalidationPublisher = std::make_unique<OperationalInvalidationPublisher>(*m_server, this);
    m_remoteReads = std::make_unique<RemoteReadApplicationServices>(*m_client,
                                                                     m_remoteSession.get(), this);
    m_remoteMutations = std::make_unique<RemoteMutationApplicationServices>(*m_client, this);
    m_remotePulling = std::make_unique<RemotePullingApplicationService>(*m_remoteMutations, this);
    m_remoteInventoryMutations =
        std::make_unique<RemoteInventoryMutationApplicationService>(*m_remoteMutations, this);
    m_remoteStorageMutations =
        std::make_unique<RemoteStorageMutationApplicationService>(*m_remoteMutations, this);
    m_remotePartReferenceMutations =
        std::make_unique<RemotePartReferenceMutationApplicationService>(*m_remoteMutations, this);
    m_remoteCollectionMutations =
        std::make_unique<RemoteCollectionMutationApplicationService>(*m_remoteMutations, this);
    m_remoteBuildMutations =
        std::make_unique<RemoteBuildMutationApplicationService>(*m_remoteMutations, this);
    m_hostReads = std::make_unique<HostReadProtocolService>(
        DatabaseManager::instance().databasePath(), this);
    m_hostReads->registerOperations(m_server->operationDispatcher());
    m_hostMutations = std::make_unique<HostMutationProtocolService>(
        DatabaseManager::instance().databasePath(),
        [this](HostMutationPublicationService::Workflow workflow,
               const HostMutationPublicationService::Scope& scope) {
            QMetaObject::invokeMethod(this, [this, workflow, scope] {
                HostMutationPublicationService service([this](const OperationalInvalidation& value) {
                    m_invalidationPublisher->publish(value);
                });
                QString error;
                if (!service.publish(workflow, scope, &error))
                    qWarning().noquote() << "Committed remote mutation invalidation failed:" << error;
                if (workflow == HostMutationPublicationService::Workflow::Pulling
                    && scope.workspaceId && scope.buildId)
                    emit remotePullingMutationCommitted(int(*scope.workspaceId), int(*scope.buildId));
                if (workflow == HostMutationPublicationService::Workflow::Inventory
                    && scope.workspaceId)
                    emit remoteInventoryMutationCommitted(int(*scope.workspaceId));
                if (workflow == HostMutationPublicationService::Workflow::Storage
                    && scope.workspaceId && scope.storageLocationId)
                    emit remoteStorageMutationCommitted(int(*scope.workspaceId),int(*scope.storageLocationId));
                if (workflow == HostMutationPublicationService::Workflow::PartReferenceCustomization)
                    emit remotePartReferenceMutationCommitted(scope.partNumber);
                if (workflow == HostMutationPublicationService::Workflow::Collection
                    && scope.workspaceId && scope.collectionItemId)
                    emit remoteCollectionMutationCommitted(int(*scope.workspaceId),int(*scope.collectionItemId));
                if ((workflow == HostMutationPublicationService::Workflow::BuildMetadata
                     || workflow == HostMutationPublicationService::Workflow::BuildRequirements
                     || workflow == HostMutationPublicationService::Workflow::BuildCancellation
                     || workflow == HostMutationPublicationService::Workflow::BuildDisassembly
                     || workflow == HostMutationPublicationService::Workflow::CompleteSetSpare)
                    && scope.workspaceId && scope.buildId)
                    emit remoteBuildMutationCommitted(
                        int(*scope.workspaceId), int(*scope.buildId),
                        scope.inventoryChanged, scope.collectionChanged,
                        workflow != HostMutationPublicationService::Workflow::BuildMetadata);
            }, Qt::QueuedConnection);
        }, epoch.success ? epoch.epoch : QString(), this);
    m_hostMutations->registerOperation(m_server->operationDispatcher(),
        QStringLiteral("builds.pulling.record"), QStringLiteral("builds.pulling.write"),
        &HostPullingMutationService::createMutation);
    const QStringList inventoryOperations={QStringLiteral("inventory.add"),
        QStringLiteral("inventory.edit"), QStringLiteral("inventory.move"),
        QStringLiteral("inventory.correct"), QStringLiteral("inventory.remove"),
        QStringLiteral("inventory.markLost"), QStringLiteral("inventory.markFound")};
    for (const QString& operation : inventoryOperations) {
        m_hostMutations->registerOperation(m_server->operationDispatcher(), operation, operation,
            [operation](const RemoteMutationDto::Metadata& metadata,
                        RemoteMutationDto::Error* error) {
                return HostInventoryMutationService::createMutation(operation, metadata, error);
            });
    }
    const QStringList storageOperations={QStringLiteral("storage.add"),QStringLiteral("storage.edit"),QStringLiteral("storage.setActive")};
    for(const QString&operation:storageOperations)m_hostMutations->registerOperation(m_server->operationDispatcher(),operation,operation,[operation](const RemoteMutationDto::Metadata&metadata,RemoteMutationDto::Error*error){return HostStorageProtocolMutationService::createMutation(operation,metadata,error);});
    const QStringList partReferenceOperations={QStringLiteral("partReference.customizations.add"),QStringLiteral("partReference.customizations.remove")};
    for(const QString&operation:partReferenceOperations)m_hostMutations->registerOperation(m_server->operationDispatcher(),operation,operation,[operation](const RemoteMutationDto::Metadata&metadata,RemoteMutationDto::Error*error){return HostPartReferenceMutationService::createMutation(operation,metadata,error);});
    const QStringList collectionOperations={QStringLiteral("collection.add"),QStringLiteral("collection.edit"),QStringLiteral("collection.setActive")};
    for(const QString&operation:collectionOperations)m_hostMutations->registerOperation(m_server->operationDispatcher(),operation,operation,[operation](const RemoteMutationDto::Metadata&metadata,RemoteMutationDto::Error*error){return HostCollectionMutationService::createMutation(operation,metadata,error);});
    const QStringList buildOperations={QStringLiteral("builds.add"),QStringLiteral("builds.edit"),QStringLiteral("builds.setActive"),QStringLiteral("builds.complete"),QStringLiteral("builds.cancel"),QStringLiteral("builds.disassemble"),QStringLiteral("builds.spare.store"),QStringLiteral("builds.requirements.add"),QStringLiteral("builds.requirements.edit"),QStringLiteral("builds.requirements.remove"),QStringLiteral("builds.allocations.set"),QStringLiteral("builds.allocateAvailable")};
    for(const QString&operation:buildOperations)m_hostMutations->registerOperation(m_server->operationDispatcher(),operation,operation,[operation](const RemoteMutationDto::Metadata&metadata,RemoteMutationDto::Error*error){return HostBuildMutationService::createMutation(operation,metadata,error);});
    m_maintenanceCoordinator = std::make_unique<HostMaintenanceCoordinator>(
        *m_server, m_hostReads->executor(), m_hostMutations->executor(), this);
    connect(m_server, &BrickSuiteWebSocketServer::statusChanged,
            this, &BrickSuiteNetworkManager::statusChanged);
    connect(m_client, &BrickSuiteWebSocketClient::statusChanged,
            this, [this](const BrickSuiteConnectionStatus&) { emit statusChanged(); });
    connect(m_client, &BrickSuiteWebSocketClient::authenticatedSessionEstablishedWithEpoch,
            m_remoteSession.get(), &RemoteSessionState::authenticatedWithEpoch);
    connect(m_client, &BrickSuiteWebSocketClient::authenticatedSessionLost,
            m_remoteSession.get(), &RemoteSessionState::disconnected);
    connect(m_client, &BrickSuiteWebSocketClient::invalidationReceived, this,
            [this](const OperationalInvalidation& invalidation, quint64 generation) {
        if (!m_remoteSession->acceptsEvent(generation, invalidation.workspaceId)) {
            qDebug() << "Obsolete session or Workspace invalidation ignored.";
            return;
        }
        emit invalidationReceived(invalidation);
    });
}

BrickSuiteNetworkManager::~BrickSuiteNetworkManager() { stop(); }

void BrickSuiteNetworkManager::startConfiguredMode()
{
    UserSettings& settings = UserSettings::instance();
    if (settings.sharedDataSource() == SharedDataSource::ThisComputer) {
        if (settings.brickSuiteServerEnabled()) {
            QString error;
            if (!restartServer(&error))
                qWarning().noquote() << "BrickSuite Server did not start:" << error;
        }
        return;
    }
    if (settings.brickSuiteServerEnabled()) {
        qWarning() << "BrickSuite Server is configured but suppressed in BrickSuite Host client mode.";
    }
    QString credentialError;
    const QString token = clientToken(&credentialError);
    m_client->configure(QUrl(settings.brickSuiteHostEndpoint()),
                        settings.brickSuiteTrustedFingerprint(), token,
                        settings.brickSuiteReconnectAutomatically());
    if (settings.brickSuiteReconnectAutomatically())
        m_client->connectToHost();
}

bool BrickSuiteNetworkManager::restartServer(QString* error)
{
    UserSettings& settings = UserSettings::instance();
    m_server->stop();
    const HostDataEpoch::LoadResult epoch = HostDataEpoch::loadExisting();
    if (!epoch.success) {
        m_serverError = epoch.error;
        if (error) *error = m_serverError;
        return false;
    }
    m_server->operationDispatcher().setDataEpoch(epoch.epoch);
    m_serverError.clear();
    if (!settings.brickSuiteServerEnabled()) return true;
    if (settings.sharedDataSource() != SharedDataSource::ThisComputer) {
        m_serverError = QStringLiteral("BrickSuite Server can run only when Shared Data Source is This Computer.");
        if (error) *error = m_serverError;
        return false;
    }
    const auto token = CredentialStore::read(QString::fromLatin1(kHostTokenCredential));
    if (!token.success || !token.found || token.value.isEmpty()) {
        m_serverError = token.success
            ? QStringLiteral("Generate a BrickSuite Server access token first.")
            : token.error;
        if (error) *error = m_serverError;
        return false;
    }
    QHostAddress address;
    if (!address.setAddress(settings.brickSuiteServerBindAddress())) {
        m_serverError = QStringLiteral("The configured Server bind address is invalid.");
        if (error) *error = m_serverError;
        return false;
    }
    if (!m_server->start(address, static_cast<quint16>(settings.brickSuiteServerPort()),
                         token.value, &m_serverError)) {
        if (error) *error = m_serverError;
        return false;
    }
    return true;
}

void BrickSuiteNetworkManager::stop()
{
    if (m_maintenanceCoordinator) m_maintenanceCoordinator->beginShutdown();
    m_client->disconnectFromHost();
    m_server->stop();
}

void BrickSuiteNetworkManager::quiesceForDatabaseRestore(
    std::function<void(bool, const QString&)> completion)
{
    if (!m_maintenanceCoordinator || !m_hostReads || !m_hostMutations) {
        if (completion) completion(false, QStringLiteral("Host database services are unavailable."));
        return;
    }
    auto closeWorkers = [this, completion = std::move(completion)]() mutable {
        m_maintenanceCoordinator->beginShutdown();
        m_client->disconnectFromHost();
        m_server->stop();
        struct State { int remaining = 2; bool success = true; QStringList errors; };
        auto state = std::make_shared<State>();
        auto closed = [state, completion](bool success, const QString& error) mutable {
            state->success = state->success && success;
            if (!error.isEmpty()) state->errors.append(error);
            if (--state->remaining == 0 && completion)
                completion(state->success, state->errors.join(QLatin1Char('\n')));
        };
        m_hostReads->executor().closeConnectionAsync(closed);
        m_hostMutations->executor().closeConnectionAsync(closed);
    };

    if (m_maintenanceCoordinator->state() == HostMaintenanceCoordinator::State::Maintenance) {
        closeWorkers();
        return;
    }
    if (m_maintenanceCoordinator->state() != HostMaintenanceCoordinator::State::Normal) {
        if (completion) completion(false, QStringLiteral("Host Maintenance is already changing state."));
        return;
    }
    auto connection = std::make_shared<QMetaObject::Connection>();
    *connection = connect(m_maintenanceCoordinator.get(),
                          &HostMaintenanceCoordinator::stateChanged, this,
                          [this, connection, closeWorkers](HostMaintenanceCoordinator::State state) mutable {
        if (state != HostMaintenanceCoordinator::State::Maintenance) return;
        disconnect(*connection);
        closeWorkers();
    });
    auto failureConnection = std::make_shared<QMetaObject::Connection>();
    *failureConnection = connect(m_maintenanceCoordinator.get(),
        &HostMaintenanceCoordinator::maintenanceEntryFailed, this,
        [this, connection, failureConnection, completion](const QString& error) mutable {
            disconnect(*connection);
            disconnect(*failureConnection);
            if (completion) completion(false, error);
        });
    if (!m_maintenanceCoordinator->requestEnterMaintenance()) {
        disconnect(*connection);
        disconnect(*failureConnection);
        if (completion) completion(false, QStringLiteral("Host Maintenance could not be started."));
    }
}

BrickSuiteWebSocketServer* BrickSuiteNetworkManager::server() const { return m_server; }
BrickSuiteWebSocketClient* BrickSuiteNetworkManager::client() const { return m_client; }
RemoteReadApplicationServices* BrickSuiteNetworkManager::remoteReads() const
{ return m_remoteReads.get(); }
RemoteMutationApplicationServices* BrickSuiteNetworkManager::remoteMutations() const
{ return m_remoteMutations.get(); }
RemotePullingApplicationService* BrickSuiteNetworkManager::remotePulling() const
{ return m_remotePulling.get(); }
RemoteInventoryMutationApplicationService* BrickSuiteNetworkManager::remoteInventoryMutations() const
{ return m_remoteInventoryMutations.get(); }
RemoteStorageMutationApplicationService* BrickSuiteNetworkManager::remoteStorageMutations() const
{ return m_remoteStorageMutations.get(); }
RemotePartReferenceMutationApplicationService* BrickSuiteNetworkManager::remotePartReferenceMutations() const
{ return m_remotePartReferenceMutations.get(); }
RemoteCollectionMutationApplicationService* BrickSuiteNetworkManager::remoteCollectionMutations() const
{ return m_remoteCollectionMutations.get(); }
RemoteBuildMutationApplicationService* BrickSuiteNetworkManager::remoteBuildMutations() const
{ return m_remoteBuildMutations.get(); }
RemoteSessionState* BrickSuiteNetworkManager::remoteSession() const
{ return m_remoteSession.get(); }
OperationalInvalidationPublisher* BrickSuiteNetworkManager::invalidationPublisher() const
{ return m_invalidationPublisher.get(); }
HostMaintenanceCoordinator* BrickSuiteNetworkManager::maintenanceCoordinator() const
{ return m_maintenanceCoordinator.get(); }
BrickSuiteConnectionStatus BrickSuiteNetworkManager::connectionStatus() const { return m_client->status(); }

QString BrickSuiteNetworkManager::serverStatusText() const
{
    if (m_server->isListening())
        return QStringLiteral("Listening securely on %1 clients authenticated: %2")
            .arg(m_server->serverPort()).arg(m_server->authenticatedClientCount());
    return m_serverError.isEmpty() ? QStringLiteral("Disabled") : m_serverError;
}

QString BrickSuiteNetworkManager::generateOrRotateHostToken(QString* error)
{
    const QString token = BrickSuiteAuthentication::generateAccessToken(error);
    if (token.isEmpty()) return {};
    if (!CredentialStore::write(QString::fromLatin1(kHostTokenCredential), token, error))
        return {};
    return token;
}

bool BrickSuiteNetworkManager::saveClientToken(const QString& token, QString* error)
{
    return CredentialStore::write(QString::fromLatin1(kClientTokenCredential), token, error);
}

QString BrickSuiteNetworkManager::clientToken(QString* error) const
{
    const auto result = CredentialStore::read(QString::fromLatin1(kClientTokenCredential));
    if (!result.success) {
        if (error) *error = result.error;
        return {};
    }
    return result.found ? result.value : QString();
}
