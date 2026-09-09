#include "BrickSuiteNetworkManager.h"

#include "BrickSuiteAuthentication.h"
#include "BrickSuiteWebSocketClient.h"
#include "BrickSuiteWebSocketServer.h"
#include "../database/DatabaseManager.h"
#include "../services/application/HostReadProtocolService.h"
#include "../services/application/RemoteReadApplicationServices.h"
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
    m_remoteReads = std::make_unique<RemoteReadApplicationServices>(*m_client, this);
    m_hostReads = std::make_unique<HostReadProtocolService>(
        DatabaseManager::instance().databasePath(), this);
    m_hostReads->registerOperations(m_server->operationDispatcher());
    connect(m_server, &BrickSuiteWebSocketServer::statusChanged,
            this, &BrickSuiteNetworkManager::statusChanged);
    connect(m_client, &BrickSuiteWebSocketClient::statusChanged,
            this, [this](const BrickSuiteConnectionStatus&) { emit statusChanged(); });
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
    m_client->disconnectFromHost();
    m_server->stop();
}

BrickSuiteWebSocketServer* BrickSuiteNetworkManager::server() const { return m_server; }
BrickSuiteWebSocketClient* BrickSuiteNetworkManager::client() const { return m_client; }
RemoteReadApplicationServices* BrickSuiteNetworkManager::remoteReads() const
{ return m_remoteReads.get(); }
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
