#include "../src/network/BrickSuiteAuthentication.h"
#include "../src/network/BrickSuiteHostIdentity.h"
#include "../src/network/BrickSuiteProtocol.h"
#include "../src/network/BrickSuiteOperationDispatcher.h"
#include "../src/network/BrickSuiteWebSocketClient.h"
#include "../src/network/BrickSuiteWebSocketServer.h"
#include "../src/network/OperationalInvalidation.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonArray>
#include <QTimer>
#include <QStandardPaths>
#include <QFile>
#include <QWebSocket>

#include "../src/services/CredentialStore.h"

#include <iostream>
#include <functional>

namespace {

bool check(bool condition, const char* message)
{
    if (!condition) std::cerr << "FAIL: " << message << '\n';
    return condition;
}

bool waitForResult(BrickSuiteWebSocketClient& client, bool* success,
                   QString* message, int timeoutMs = 5000)
{
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    const auto connection = QObject::connect(
        &client, &BrickSuiteWebSocketClient::testConnectionCompleted, &loop,
        [&](bool ok, const QString& text) {
            *success = ok;
            *message = text;
            loop.quit();
        });
    timeout.start(timeoutMs);
    loop.exec();
    QObject::disconnect(connection);
    return !timeout.isActive() ? false : true;
}

bool waitUntil(const std::function<bool()>& predicate, int timeoutMs = 5000)
{
    QElapsedTimer timer;
    timer.start();
    while (!predicate() && timer.elapsed() < timeoutMs)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    return predicate();
}

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication application(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("BrickSuiteM26Test"));
    QCoreApplication::setApplicationName(QStringLiteral("SecureHostFoundationTest"));
    QStandardPaths::setTestModeEnabled(true);
    bool ok = true;

    const auto request = BrickSuiteProtocol::request(
        QStringLiteral("system.ping"), {{QStringLiteral("value"), 7}});
    const auto parsedRequest = BrickSuiteProtocol::parse(
        BrickSuiteProtocol::serialize(request));
    ok &= check(parsedRequest.valid
                && parsedRequest.message.requestId == request.requestId
                && parsedRequest.message.operation == request.operation,
                "request serialization round-trip");

    const auto response = BrickSuiteProtocol::response(request,
        {{QStringLiteral("answer"), 42}});
    const auto parsedResponse = BrickSuiteProtocol::parse(
        BrickSuiteProtocol::serialize(response));
    ok &= check(parsedResponse.valid
                && parsedResponse.message.type == BrickSuiteProtocol::MessageType::Response
                && parsedResponse.message.payload.value(QStringLiteral("answer")).toInt() == 42,
                "response serialization round-trip");

    const auto error = BrickSuiteProtocol::errorResponse(
        request, QStringLiteral("FORBIDDEN"), QStringLiteral("Not allowed."));
    const auto parsedError = BrickSuiteProtocol::parse(BrickSuiteProtocol::serialize(error));
    ok &= check(parsedError.valid && parsedError.message.error.code == QStringLiteral("FORBIDDEN"),
                "error serialization round-trip");
    OperationalInvalidation codecEvent;
    codecEvent.sequence = 42;
    codecEvent.domains = {OperationalInvalidationDomain::Inventory,
                          OperationalInvalidationDomain::InventoryHistory};
    codecEvent.workspaceId = 7;
    codecEvent.inventoryRecordId = 19;
    const auto wireEvent = BrickSuiteProtocol::event(
        OperationalInvalidation::Operation, codecEvent.toPayload());
    const auto parsedWireEvent = BrickSuiteProtocol::parse(
        BrickSuiteProtocol::serialize(wireEvent));
    OperationalInvalidation decodedEvent;
    QString invalidationError;
    ok &= check(parsedWireEvent.valid
                && parsedWireEvent.message.type == BrickSuiteProtocol::MessageType::Event
                && parsedWireEvent.message.requestId.isEmpty()
                && OperationalInvalidation::fromPayload(parsedWireEvent.message.payload,
                                                         &decodedEvent, &invalidationError)
                && decodedEvent.sequence == 42 && decodedEvent.domains == codecEvent.domains
                && decodedEvent.workspaceId == 7 && decodedEvent.inventoryRecordId == 19,
                "invalidation event round-trip without request correlation");
    OperationalInvalidation invalidEvent;
    invalidEvent.sequence = 1;
    invalidEvent.domains = {OperationalInvalidationDomain::Inventory};
    ok &= check(!OperationalInvalidation::validate(invalidEvent, true, &invalidationError),
                "Workspace-scoped invalidation without Workspace rejected");
    invalidEvent.domains.clear();
    ok &= check(!OperationalInvalidation::validate(invalidEvent, true, &invalidationError),
                "empty invalidation domain list rejected");
    for (int value = int(OperationalInvalidationDomain::Workspaces);
         value <= int(OperationalInvalidationDomain::PartReferenceCustomizations); ++value) {
        const auto domain = static_cast<OperationalInvalidationDomain>(value);
        const QString name = operationalInvalidationDomainName(domain);
        ok &= check(!name.isEmpty() && operationalInvalidationDomainFromName(name) == domain,
                    "supported invalidation domain wire name round-trip");
    }
    ok &= check(BrickSuiteProtocol::parse(BrickSuiteProtocol::serialize(
                    BrickSuiteProtocol::event(QStringLiteral("shared.notSupported"),
                                              codecEvent.toPayload()))).valid
                && QStringLiteral("shared.notSupported") != OperationalInvalidation::Operation,
                "unknown event operation remains distinguishable for client rejection");
    QJsonObject unknownDomain = codecEvent.toPayload();
    unknownDomain.insert(QStringLiteral("domains"), QJsonArray{QStringLiteral("unknown")});
    ok &= check(!OperationalInvalidation::fromPayload(unknownDomain, &decodedEvent,
                                                       &invalidationError),
                "unknown invalidation domain rejected");
    QJsonObject malformedId = codecEvent.toPayload();
    malformedId.insert(QStringLiteral("workspaceId"), -1);
    ok &= check(!OperationalInvalidation::fromPayload(malformedId, &decodedEvent,
                                                       &invalidationError),
                "malformed invalidation identifier rejected");
    ok &= check(!BrickSuiteProtocol::parse(QByteArrayLiteral("not-json")).valid,
                "malformed JSON rejected");
    ok &= check(!BrickSuiteProtocol::parse(QByteArrayLiteral("{}")) .valid,
                "missing fields rejected");
    ok &= check(!BrickSuiteProtocol::parse(QByteArray(
                    BrickSuiteProtocol::MaximumMessageBytes + 1, 'x')).valid,
                "oversized payload rejected");
    BrickSuiteOperationDispatcher dispatcher;
    const auto unauthenticatedCapabilities = dispatcher.dispatch(
        BrickSuiteProtocol::request(QStringLiteral("system.capabilities")), false);
    ok &= check(unauthenticatedCapabilities.type == BrickSuiteProtocol::MessageType::Error
                && unauthenticatedCapabilities.error.code == QStringLiteral("AUTH_REQUIRED"),
                "capabilities require authentication");
    const auto unknownOperation = dispatcher.dispatch(
        BrickSuiteProtocol::request(QStringLiteral("inventory.read")), true);
    ok &= check(unknownOperation.error.code == QStringLiteral("UNKNOWN_OPERATION"),
                "unregistered business operation rejected");

    QString cryptoError;
    const QByteArray challenge = BrickSuiteAuthentication::secureRandom(32, &cryptoError);
    const QByteArray otherChallenge = BrickSuiteAuthentication::secureRandom(32, &cryptoError);
    const QByteArray nonce = BrickSuiteAuthentication::secureRandom(32, &cryptoError);
    const QByteArray session = BrickSuiteAuthentication::secureRandom(24, &cryptoError);
    const QByteArray proof = BrickSuiteAuthentication::hmacSha256(
        QByteArrayLiteral("secret"), BrickSuiteAuthentication::authenticationInput(
            challenge, nonce, session, 1, 0), &cryptoError);
    const QByteArray replayProof = BrickSuiteAuthentication::hmacSha256(
        QByteArrayLiteral("secret"), BrickSuiteAuthentication::authenticationInput(
            otherChallenge, nonce, session, 1, 0), &cryptoError);
    ok &= check(proof.size() == 32 && replayProof.size() == 32
                && !BrickSuiteAuthentication::constantTimeEquals(proof, replayProof),
                "fresh challenge prevents proof replay");
    ok &= check(!BrickSuiteAuthentication::constantTimeEquals(
                    proof, BrickSuiteAuthentication::hmacSha256(
                        QByteArrayLiteral("wrong"), BrickSuiteAuthentication::authenticationInput(
                            challenge, nonce, session, 1, 0))),
                "wrong secret proof rejected");

    const auto identity = BrickSuiteHostIdentity::generateEphemeral();
    ok &= check(identity.success && !identity.certificate.isNull()
                && !identity.privateKey.isNull()
                && BrickSuiteHostIdentity::normalizedFingerprint(identity.fingerprint).size() == 64,
                "ephemeral EC certificate/key generation");

    const auto persistedIdentity = BrickSuiteHostIdentity::loadOrCreate();
    const auto reloadedIdentity = BrickSuiteHostIdentity::loadOrCreate();
    ok &= check(persistedIdentity.success && reloadedIdentity.success
                && persistedIdentity.fingerprint == reloadedIdentity.fingerprint,
                "persisted Host identity survives restart simulation");
    const auto regeneratedIdentity = BrickSuiteHostIdentity::regenerate();
    ok &= check(regeneratedIdentity.success
                && regeneratedIdentity.fingerprint != persistedIdentity.fingerprint,
                "explicit regeneration changes Host fingerprint");

    BrickSuiteWebSocketServer server;
    server.operationDispatcher().registerOperation(QStringLiteral("test.operational"), true,
        [](const QJsonObject&) { return QJsonObject{{QStringLiteral("accepted"), true}}; });
    QString serverError;
    const QString token = QStringLiteral("test-token-with-at-least-256-bits-not-required-for-fixture");
    ok &= check(server.startWithIdentity(QHostAddress::LocalHost, 0, token,
                                         identity, &serverError),
                "secure loopback server starts");
    ok &= check(server.serverPort() != 0, "ephemeral server port assigned");

    BrickSuiteWebSocketServer conflictingServer;
    ok &= check(!conflictingServer.startWithIdentity(QHostAddress::LocalHost,
                    server.serverPort(), token, identity, &serverError),
                "bind conflict reports failure without changing port");

    BrickSuiteWebSocketClient firstUseClient;
    QString firstUseFingerprint;
    QEventLoop firstUseLoop;
    QTimer firstUseTimeout;
    firstUseTimeout.setSingleShot(true);
    QObject::connect(&firstUseTimeout, &QTimer::timeout,
                     &firstUseLoop, &QEventLoop::quit);
    QObject::connect(&firstUseClient, &BrickSuiteWebSocketClient::trustRequired,
                     &firstUseLoop, [&](const QString& fingerprint) {
        firstUseFingerprint = fingerprint;
        firstUseLoop.quit();
    });
    firstUseClient.configure(
        QUrl(QStringLiteral("wss://127.0.0.1:%1").arg(server.serverPort())),
        QString(), token, false);
    firstUseTimeout.start(5000);
    firstUseClient.connectToHost();
    firstUseLoop.exec();
    ok &= check(firstUseTimeout.isActive()
                && firstUseFingerprint == identity.fingerprint,
                "first-use flow exposes presented fingerprint without trusting it");
    firstUseClient.disconnectFromHost();

    BrickSuiteWebSocketClient client;
    bool success = false;
    QString resultMessage;
    client.configure(QUrl(QStringLiteral("wss://127.0.0.1:%1").arg(server.serverPort())),
                     identity.fingerprint, token, false);
    client.connectToHost();
    ok &= check(waitForResult(client, &success, &resultMessage) && success,
                "pinned WSS challenge-response and capabilities succeeds");
    ok &= check(client.status().state == BrickSuiteConnectionState::ConnectedAuthenticated,
                "client reaches authenticated state");
    ok &= check(!client.capabilities().value(QStringLiteral("sharedBusinessDataAvailable")).toBool(),
                "M26.3 does not advertise shared business operations");
    ok &= check(client.supportsCapability(OperationalInvalidation::Capability),
                "Host advertises invalidation capability");

    BrickSuiteWebSocketClient secondClient;
    secondClient.configure(QUrl(QStringLiteral("wss://127.0.0.1:%1").arg(server.serverPort())),
                           identity.fingerprint, token, false);
    secondClient.connectToHost();
    bool secondSuccess = false;
    ok &= check(waitForResult(secondClient, &secondSuccess, &resultMessage) && secondSuccess,
                "second authenticated Client connects");

    QWebSocket unauthenticatedSocket;
    QSslConfiguration unauthenticatedSsl = QSslConfiguration::defaultConfiguration();
    unauthenticatedSsl.setPeerVerifyMode(QSslSocket::VerifyNone);
    unauthenticatedSocket.setSslConfiguration(unauthenticatedSsl);
    unauthenticatedSocket.open(QUrl(QStringLiteral("wss://127.0.0.1:%1").arg(server.serverPort())));
    ok &= check(waitUntil([&]() { return unauthenticatedSocket.state() == QAbstractSocket::ConnectedState; }),
                "unauthenticated transport connects");

    int firstEvents = 0;
    int secondEvents = 0;
    QObject::connect(&client, &BrickSuiteWebSocketClient::invalidationReceived,
                     [&](const OperationalInvalidation& event, quint64) {
        if (event.sequence > 0) ++firstEvents;
    });
    QObject::connect(&secondClient, &BrickSuiteWebSocketClient::invalidationReceived,
                     [&](const OperationalInvalidation& event, quint64) {
        if (event.sequence > 0) ++secondEvents;
    });
    OperationalInvalidation broadcast;
    broadcast.domains = {OperationalInvalidationDomain::Inventory};
    broadcast.workspaceId = 7;
    ok &= check(server.broadcastInvalidation(broadcast) == 2,
                "broadcast targets authenticated compatible Clients only");
    ok &= check(waitUntil([&]() { return firstEvents == 1 && secondEvents == 1; }),
                "two authenticated Clients each receive one invalidation");

    bool pingCompleted = false;
    client.sendRequest(QStringLiteral("system.ping"), {}, &client,
        [&](const QJsonObject&) { pingCompleted = true; });
    server.broadcastInvalidation(broadcast);
    ok &= check(waitUntil([&]() { return pingCompleted && firstEvents == 2; }),
                "invalidation delivery does not disturb request correlation");

    const quint64 maintenanceSessionGeneration = client.authenticatedSessionGeneration();
    const int maintenanceDisconnects = client.transportDisconnectCountForTesting();
    const int maintenanceReconnects = client.reconnectScheduleCountForTesting();
    server.setOperationalAdmissionOpen(false);
    BrickSuiteProtocol::Error maintenanceError;
    bool maintenanceRejected = false;
    client.sendRequest(QStringLiteral("test.operational"), {}, &client, {},
        [&](const BrickSuiteProtocol::Error& error) {
            maintenanceError = error;
            maintenanceRejected = true;
        });
    ok &= check(waitUntil([&]() { return maintenanceRejected; })
                && maintenanceError.code == QStringLiteral("HOST_MAINTENANCE")
                && maintenanceError.retryable
                && client.status().state == BrickSuiteConnectionState::HostMaintenance,
                "maintenance rejects operational work definitively and updates Client state");
    ok &= check(client.socketStateForTesting() == QAbstractSocket::ConnectedState
                    && client.authenticatedForTesting()
                    && client.authenticatedSessionGeneration() == maintenanceSessionGeneration
                    && client.transportDisconnectCountForTesting() == maintenanceDisconnects
                    && client.reconnectScheduleCountForTesting() == maintenanceReconnects
                    && !client.reconnectTimerActiveForTesting(),
                "maintenance preserves the authenticated transport and schedules no reconnect");
    bool statusCompleted = false;
    QJsonObject maintenanceStatus;
    client.sendRequest(QStringLiteral("system.status"), {}, &client,
        [&](const QJsonObject& payload) {
            maintenanceStatus = payload;
            statusCompleted = true;
        });
    ok &= check(waitUntil([&]() { return statusCompleted; })
                && maintenanceStatus.value(QStringLiteral("maintenance")).toBool(),
                "maintenance-safe authenticated status request remains available");
    server.setOperationalAdmissionOpen(true);
    server.broadcastFullOperationalInvalidation();
    ok &= check(waitUntil([&]() {
        return client.status().state == BrickSuiteConnectionState::ConnectedAuthenticated;
    }), "maintenance exit invalidation restores authenticated Client state");
    ok &= check(client.socketStateForTesting() == QAbstractSocket::ConnectedState
                    && client.authenticatedForTesting()
                    && client.authenticatedSessionGeneration() == maintenanceSessionGeneration
                    && client.transportDisconnectCountForTesting() == maintenanceDisconnects
                    && client.reconnectScheduleCountForTesting() == maintenanceReconnects,
                "maintenance exit keeps the same authenticated socket and session generation");
    bool operationRecovered = false;
    client.sendRequest(QStringLiteral("test.operational"), {}, &client,
        [&](const QJsonObject& payload) {
            operationRecovered = payload.value(QStringLiteral("accepted")).toBool();
        });
    ok &= check(waitUntil([&]() { return operationRecovered; }),
                "operational requests resume on the existing connection after maintenance");

    client.sendProtocolEventForTesting(codecEvent);
    ok &= check(waitUntil([&]() {
        return client.status().state != BrickSuiteConnectionState::ConnectedAuthenticated;
    }), "Client-originated event is rejected by disconnecting the sender");

    const int firstEventsBeforeDisconnectedBroadcast = firstEvents;
    const int secondEventsBeforeDisconnectedBroadcast = secondEvents;
    ok &= check(server.broadcastInvalidation(broadcast) == 1,
                "disconnected Client is removed from broadcast recipients");
    ok &= check(waitUntil([&]() {
                    return secondEvents == secondEventsBeforeDisconnectedBroadcast + 1;
                })
                && firstEvents == firstEventsBeforeDisconnectedBroadcast,
                "disconnected Client receives no invalidation");

    client.connectToHost();
    success = false;
    ok &= check(waitForResult(client, &success, &resultMessage) && success,
                "reconnected Client establishes a new authenticated session");
    ok &= check(server.broadcastInvalidation(broadcast) == 2,
                "reconnected Client rejoins authenticated broadcasts");
    ok &= check(waitUntil([&]() {
        return firstEvents == firstEventsBeforeDisconnectedBroadcast + 1
            && secondEvents == secondEventsBeforeDisconnectedBroadcast + 2;
    }), "new-session invalidation is accepted after reconnect");

    secondClient.disconnectFromHost();
    unauthenticatedSocket.close();

    BrickSuiteWebSocketClient wrongTokenClient;
    success = true;
    wrongTokenClient.configure(
        QUrl(QStringLiteral("wss://127.0.0.1:%1").arg(server.serverPort())),
        identity.fingerprint, QStringLiteral("wrong-token"), false);
    wrongTokenClient.connectToHost();
    ok &= check(waitForResult(wrongTokenClient, &success, &resultMessage) && !success,
                "wrong access token fails authentication");
    ok &= check(wrongTokenClient.status().state == BrickSuiteConnectionState::AuthenticationFailed,
                "wrong token produces authentication-failed state");

    BrickSuiteWebSocketClient wrongFingerprintClient;
    success = true;
    wrongFingerprintClient.configure(
        QUrl(QStringLiteral("wss://127.0.0.1:%1").arg(server.serverPort())),
        QString(64, QLatin1Char('A')), token, false);
    wrongFingerprintClient.connectToHost();
    ok &= check(waitForResult(wrongFingerprintClient, &success, &resultMessage) && !success,
                "wrong certificate fingerprint fails closed");
    ok &= check(wrongFingerprintClient.status().state
                    == BrickSuiteConnectionState::HostIdentityMismatch,
                "wrong fingerprint produces identity-mismatch state");

    BrickSuiteWebSocketClient reconnectClient;
    reconnectClient.configure(
        QUrl(QStringLiteral("wss://127.0.0.1:%1").arg(server.serverPort())),
        identity.fingerprint, token, true);
    reconnectClient.connectToHost();
    bool reconnectSuccess = false;
    ok &= check(waitForResult(reconnectClient, &reconnectSuccess, &resultMessage)
                    && reconnectSuccess,
                "automatic-reconnect Client authenticates before disconnect test");
    const int disconnectSchedules = reconnectClient.reconnectScheduleCountForTesting();
    server.stop();
    ok &= check(!server.isListening(), "server shutdown stops listener");
    ok &= check(waitUntil([&]() {
                    return reconnectClient.transportDisconnectCountForTesting() > 0
                        && reconnectClient.reconnectScheduleCountForTesting()
                            == disconnectSchedules + 1;
                }),
                "an actual transport close schedules exactly one reconnect attempt");
    reconnectClient.disconnectFromHost();

    QString cleanupError;
    ok &= check(CredentialStore::remove(
                    QStringLiteral("BrickSuiteHostTlsIdentity.Test.SecureHostFoundationTest"),
                    &cleanupError), "test Host identity credential cleanup");
    QFile::remove(BrickSuiteHostIdentity::certificatePath());
    return ok ? 0 : 1;
}
