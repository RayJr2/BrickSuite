#include "../src/network/BrickSuiteAuthentication.h"
#include "../src/network/BrickSuiteHostIdentity.h"
#include "../src/network/BrickSuiteProtocol.h"
#include "../src/network/BrickSuiteOperationDispatcher.h"
#include "../src/network/BrickSuiteWebSocketClient.h"
#include "../src/network/BrickSuiteWebSocketServer.h"
#include "../src/network/PairedDeviceAdministrationService.h"
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
#include <QTemporaryDir>
#include <QWebSocket>

#include "../src/services/CredentialStore.h"

#include <iostream>
#include <functional>
#include <algorithm>

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

    PairedDeviceRegistry previousRegistry;
    QString previousRegistryError;
    if (previousRegistry.load(&previousRegistryError)) {
        for (const auto& device : previousRegistry.devices())
            CredentialStore::remove(device.credentialReference, nullptr);
    }
    QFile::remove(PairedDeviceRegistry::defaultPath());

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
         value <= int(OperationalInvalidationDomain::Buildability); ++value) {
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
    const auto secondIdentity = BrickSuiteHostIdentity::generateEphemeral();
    const auto mismatchedIdentity = BrickSuiteHostIdentity::validate(
        identity.certificate, secondIdentity.privateKey);
    ok &= check(!mismatchedIdentity.success
                    && mismatchedIdentity.error.contains(QStringLiteral("do not match")),
                "certificate/private-key mismatch rejected");
    const auto expiredIdentity = BrickSuiteHostIdentity::generateEphemeralForTesting(-7200, -3600);
    ok &= check(!expiredIdentity.success
                    && expiredIdentity.error.contains(QStringLiteral("expired")),
                "expired certificate rejected deterministically");
    const auto futureIdentity = BrickSuiteHostIdentity::generateEphemeralForTesting(3600, 7200);
    ok &= check(!futureIdentity.success
                    && futureIdentity.error.contains(QStringLiteral("not yet valid")),
                "not-yet-valid certificate rejected deterministically");
    ok &= check(!BrickSuiteHostIdentity::validate(QSslCertificate(), identity.privateKey).success,
                "malformed certificate rejected");
    ok &= check(!BrickSuiteHostIdentity::validate(identity.certificate, QSslKey()).success,
                "malformed private key rejected");

    const auto persistedIdentity = BrickSuiteHostIdentity::loadOrCreate();
    const auto reloadedIdentity = BrickSuiteHostIdentity::loadOrCreate();
    ok &= check(persistedIdentity.success && reloadedIdentity.success
                && persistedIdentity.fingerprint == reloadedIdentity.fingerprint,
                "persisted Host identity survives restart simulation");
    const QString hostTokenCredential =
        QStringLiteral("BrickSuiteHostAccessToken.Test.SecureHostFoundationTest");
    QString credentialError;
    const QString retainedToken = BrickSuiteAuthentication::generateAccessToken(&credentialError);
    ok &= check(CredentialStore::write(hostTokenCredential, retainedToken, &credentialError),
                "Host token fixture stored securely");
    const auto regeneratedIdentity = BrickSuiteHostIdentity::regenerate();
    ok &= check(regeneratedIdentity.success
                && regeneratedIdentity.fingerprint != persistedIdentity.fingerprint,
                "explicit regeneration changes Host fingerprint");
    const auto tokenAfterRegeneration = CredentialStore::read(hostTokenCredential);
    ok &= check(tokenAfterRegeneration.success && tokenAfterRegeneration.found
                    && tokenAfterRegeneration.value == retainedToken,
                "Host identity regeneration does not rotate Host token");
    const QString identityCredential =
        QStringLiteral("BrickSuiteHostTlsIdentity.Test.SecureHostFoundationTest");
    ok &= check(CredentialStore::write(identityCredential, QStringLiteral("not-a-certificate"),
                                        &credentialError)
                    && !BrickSuiteHostIdentity::loadOrCreate().success,
                "malformed persisted certificate fails without silent regeneration");
    const QString certificateWithoutKey = QString::fromLatin1(
        regeneratedIdentity.certificate.toPem()) + QStringLiteral("\nnot-a-private-key");
    ok &= check(CredentialStore::write(identityCredential, certificateWithoutKey,
                                        &credentialError)
                    && !BrickSuiteHostIdentity::loadOrCreate().success,
                "malformed persisted private key fails without silent regeneration");
    ok &= check(BrickSuiteHostIdentity::regenerate().success,
                "explicit regeneration recovers an invalid persisted identity");

    BrickSuiteWebSocketServer server;
    server.operationDispatcher().registerOperation(QStringLiteral("test.operational"), true,
        [](const QJsonObject&) { return QJsonObject{{QStringLiteral("accepted"), true}}; });
    server.operationDispatcher().registerAsyncOperation(QStringLiteral("test.oversized"), true,
        [](const BrickSuiteProtocol::Message& request,
           BrickSuiteOperationDispatcher::Completion completion) {
            completion(BrickSuiteProtocol::response(request,
                {{QStringLiteral("data"), QString(int(BrickSuiteProtocol::MaximumMessageBytes),
                                                  QLatin1Char('x'))}}));
        });
    QString serverError;
    const QString token = QStringLiteral("test-token-with-at-least-256-bits-not-required-for-fixture");
    ok &= check(server.startWithIdentity(QHostAddress::LocalHost, 0, token,
                                         identity, &serverError),
                "secure loopback server starts");
    ok &= check(server.serverPort() != 0, "ephemeral server port assigned");

    QTemporaryDir administrationTemporary;
    PairedDeviceRegistry unhealthyRegistry(
        administrationTemporary.filePath(QStringLiteral("paired-devices.json")));
    ok &= check(unhealthyRegistry.load(&serverError), "administration registry fixture loads");
    const QString unhealthyDeviceId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString fixtureTimestamp = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    ok &= check(unhealthyRegistry.add({unhealthyDeviceId, QStringLiteral("Orphan device"),
                    fixtureTimestamp, fixtureTimestamp, QStringLiteral("0.4.0"),
                    QStringLiteral("Windows"), QStringLiteral("missing-credential"), true},
                    &serverError),
                "administration orphan fixture persists");
    PairedDeviceAdministrationService unhealthyAdministration(
        unhealthyRegistry, server,
        [](const QString&) {
            return PairedDeviceAdministrationService::CredentialState{true, false, {}};
        },
        [](const QString&, QString* error) {
            if (error) *error = QStringLiteral("injected deletion failure");
            return false;
        });
    const auto unhealthyDevices = unhealthyAdministration.devices(&serverError);
    ok &= check(serverError.isEmpty() && unhealthyDevices.size() == 1
                    && !unhealthyDevices.first().credentialAvailable,
                "missing device credential is exposed as unusable and remains fail closed");
    const auto failedCredentialDeletion = unhealthyAdministration.revokeDevice(unhealthyDeviceId);
    ok &= check(!failedCredentialDeletion.success
                    && unhealthyRegistry.find(unhealthyDeviceId).has_value()
                    && !unhealthyRegistry.find(unhealthyDeviceId)->active,
                "credential deletion failure leaves an accurately inactive device record");

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
    bool oversizedRejected = false;
    BrickSuiteProtocol::Error oversizedError;
    client.sendRequest(QStringLiteral("test.oversized"), {}, &client, {},
        [&](const BrickSuiteProtocol::Error& error) {
            oversizedError = error;
            oversizedRejected = true;
        });
    ok &= check(waitUntil([&] { return oversizedRejected; })
                    && oversizedError.code == QStringLiteral("RESULT_TOO_LARGE")
                    && !oversizedError.retryable
                    && client.socketStateForTesting() == QAbstractSocket::ConnectedState,
                "oversized Host result becomes a structured non-fatal error");
    bool postOversizedPing = false;
    client.sendRequest(QStringLiteral("system.ping"), {}, &client,
        [&](const QJsonObject&) { postOversizedPing = true; });
    ok &= check(waitUntil([&] { return postOversizedPing; }),
                "connection remains usable after oversized result rejection");

    BrickSuiteWebSocketClient secondClient;
    secondClient.configure(QUrl(QStringLiteral("wss://127.0.0.1:%1").arg(server.serverPort())),
                           identity.fingerprint, token, false);
    secondClient.connectToHost();
    bool secondSuccess = false;
    ok &= check(waitForResult(secondClient, &secondSuccess, &resultMessage) && secondSuccess,
                "second authenticated Client connects");
    ok &= check(server.legacyAuthenticatedClientCountForTesting() == 2
                    && server.authenticatedDeviceIdsForTesting().isEmpty(),
                "Protocol 1.2 clients remain distinct legacy shared-token sessions");

    const auto pairingAttempt = server.pairingService()->start(&serverError);
    BrickSuiteWebSocketClient pairingClient;
    QString pairedDeviceId;
    QString pairedCredential;
    QObject::connect(&pairingClient, &BrickSuiteWebSocketClient::pairingCompleted,
                     [&](const QString& deviceId, const QString& credential) {
        pairedDeviceId = deviceId;
        pairedCredential = credential;
    });
    pairingClient.beginPairing(
        QUrl(QStringLiteral("wss://127.0.0.1:%1").arg(server.serverPort())),
        identity.fingerprint, pairingAttempt.code, QStringLiteral("Protocol 1.3 test device"));
    pairingClient.connectToHost();
    ok &= check(waitUntil([&] { return !pairedDeviceId.isEmpty(); })
                    && !pairedCredential.isEmpty() && !server.pairingService()->attempt().active,
                "Protocol 1.3 pairs over pinned WSS and consumes the one-time code");
    pairingClient.disconnectFromHost();

    BrickSuiteWebSocketClient pairedClient;
    pairedClient.configurePairedDevice(
        QUrl(QStringLiteral("wss://127.0.0.1:%1").arg(server.serverPort())),
        identity.fingerprint, pairedDeviceId, pairedCredential, false);
    pairedClient.connectToHost();
    bool pairedSuccess = false;
    ok &= check(waitForResult(pairedClient, &pairedSuccess, &resultMessage) && pairedSuccess,
                "Protocol 1.3 authenticates with its per-device credential");
    ok &= check(server.authenticatedDeviceIdsForTesting().contains(pairedDeviceId),
                "authenticated Protocol 1.3 session is bound to the Host-issued device ID");

    BrickSuiteWebSocketClient wrongPairedCredentialClient;
    wrongPairedCredentialClient.configurePairedDevice(
        QUrl(QStringLiteral("wss://127.0.0.1:%1").arg(server.serverPort())),
        identity.fingerprint, pairedDeviceId, QStringLiteral("wrong-device-credential"), false);
    wrongPairedCredentialClient.connectToHost();
    bool wrongPairedSuccess = true;
    ok &= check(waitForResult(wrongPairedCredentialClient, &wrongPairedSuccess, &resultMessage)
                    && !wrongPairedSuccess,
                "Protocol 1.3 rejects a wrong per-device credential");

    const auto secondPairingAttempt = server.pairingService()->start(&serverError);
    BrickSuiteWebSocketClient secondPairingClient;
    QString secondPairedDeviceId;
    QString secondPairedCredential;
    QObject::connect(&secondPairingClient, &BrickSuiteWebSocketClient::pairingCompleted,
                     [&](const QString& deviceId, const QString& credential) {
        secondPairedDeviceId = deviceId;
        secondPairedCredential = credential;
    });
    secondPairingClient.beginPairing(
        QUrl(QStringLiteral("wss://127.0.0.1:%1").arg(server.serverPort())),
        identity.fingerprint, secondPairingAttempt.code,
        QStringLiteral("Protocol 1.3 test device"));
    secondPairingClient.connectToHost();
    ok &= check(waitUntil([&] { return !secondPairedDeviceId.isEmpty(); })
                    && secondPairedDeviceId != pairedDeviceId
                    && secondPairedCredential != pairedCredential,
                "second device receives a distinct identity and credential");
    secondPairingClient.disconnectFromHost();
    BrickSuiteWebSocketClient secondPairedClient;
    secondPairedClient.configurePairedDevice(
        QUrl(QStringLiteral("wss://127.0.0.1:%1").arg(server.serverPort())),
        identity.fingerprint, secondPairedDeviceId, secondPairedCredential, false);
    secondPairedClient.connectToHost();
    bool secondPairedSuccess = false;
    ok &= check(waitForResult(secondPairedClient, &secondPairedSuccess, &resultMessage)
                    && secondPairedSuccess
                    && server.authenticatedDeviceIdsForTesting().contains(pairedDeviceId)
                    && server.authenticatedDeviceIdsForTesting().contains(secondPairedDeviceId),
                "two paired devices connect simultaneously with distinct session identities");
    BrickSuiteWebSocketClient crossCredentialClient;
    crossCredentialClient.configurePairedDevice(
        QUrl(QStringLiteral("wss://127.0.0.1:%1").arg(server.serverPort())),
        identity.fingerprint, pairedDeviceId, secondPairedCredential, false);
    crossCredentialClient.connectToHost();
    bool crossSuccess = true;
    ok &= check(waitForResult(crossCredentialClient, &crossSuccess, &resultMessage) && !crossSuccess,
                "one device credential cannot authenticate as another device");
    BrickSuiteWebSocketClient unknownDeviceClient;
    unknownDeviceClient.configurePairedDevice(
        QUrl(QStringLiteral("wss://127.0.0.1:%1").arg(server.serverPort())),
        identity.fingerprint, QUuid::createUuid().toString(QUuid::WithoutBraces),
        pairedCredential, false);
    unknownDeviceClient.connectToHost();
    bool unknownSuccess = true;
    ok &= check(waitForResult(unknownDeviceClient, &unknownSuccess, &resultMessage) && !unknownSuccess,
                "unknown Protocol 1.3 device ID fails closed");
    BrickSuiteWebSocketClient duplicateDeviceSession;
    duplicateDeviceSession.configurePairedDevice(
        QUrl(QStringLiteral("wss://127.0.0.1:%1").arg(server.serverPort())),
        identity.fingerprint, secondPairedDeviceId, secondPairedCredential, false);
    duplicateDeviceSession.connectToHost();
    bool duplicateSessionSuccess = false;
    ok &= check(waitForResult(duplicateDeviceSession, &duplicateSessionSuccess, &resultMessage)
                    && duplicateSessionSuccess,
                "one paired device may hold multiple authenticated sessions");
    PairedDeviceAdministrationService administration(*server.pairedDeviceRegistry(), server);
    QString administrationError;
    auto administeredDevices = administration.devices(&administrationError);
    const auto secondAdministered = std::find_if(
        administeredDevices.cbegin(), administeredDevices.cend(),
        [&](const PairedDeviceAdministrationService::DeviceInfo& device) {
            return device.record.deviceId == secondPairedDeviceId;
        });
    ok &= check(administrationError.isEmpty() && administeredDevices.size() == 2
                    && secondAdministered != administeredDevices.cend()
                    && secondAdministered->connectedSessions == 2,
                "device administration lists paired devices");
    const auto renamed = administration.renameDevice(
        secondPairedDeviceId, QStringLiteral("Protocol 1.3 test device"));
    ok &= check(renamed.success
                    && server.pairedDeviceRegistry()->find(secondPairedDeviceId)->friendlyName
                        == QStringLiteral("Protocol 1.3 test device"),
                "device rename persists and duplicate friendly names remain valid");
    const auto secondCredentialBeforeRevoke = CredentialStore::read(
        BrickSuitePairingService::credentialReference(secondPairedDeviceId));
    ok &= check(secondCredentialBeforeRevoke.success && secondCredentialBeforeRevoke.found
                    && secondCredentialBeforeRevoke.value == secondPairedCredential,
                "rename preserves device identity and credential");
    int revocationNotices = 0;
    QObject::connect(&secondPairedClient, &BrickSuiteWebSocketClient::deviceRevoked,
                     [&] { ++revocationNotices; });
    const auto revoked = administration.revokeDevice(secondPairedDeviceId);
    ok &= check(revoked.success
                    && waitUntil([&] {
                        return secondPairedClient.status().state
                                == BrickSuiteConnectionState::DeviceRevoked
                            && duplicateDeviceSession.status().state
                                == BrickSuiteConnectionState::DeviceRevoked;
                    })
                    && revocationNotices == 1
                    && !secondPairedClient.reconnectTimerActiveForTesting()
                    && pairedClient.status().state
                        == BrickSuiteConnectionState::ConnectedAuthenticated
                    && server.legacyAuthenticatedClientCountForTesting() == 2,
                "targeted revocation disconnects only the selected paired device");
    ok &= check(!server.pairedDeviceRegistry()->find(secondPairedDeviceId).has_value()
                    && !CredentialStore::read(
                        BrickSuitePairingService::credentialReference(secondPairedDeviceId)).found,
                "targeted revocation removes registry record and protected credential");
    BrickSuiteWebSocketClient revokedReconnectClient;
    revokedReconnectClient.configurePairedDevice(
        QUrl(QStringLiteral("wss://127.0.0.1:%1").arg(server.serverPort())),
        identity.fingerprint, secondPairedDeviceId, secondPairedCredential, false);
    revokedReconnectClient.connectToHost();
    bool revokedReconnectSuccess = true;
    ok &= check(waitForResult(revokedReconnectClient, &revokedReconnectSuccess, &resultMessage)
                    && !revokedReconnectSuccess,
                "revoked credential cannot authenticate again");

    const auto replacementPairingAttempt = server.pairingService()->start(&serverError);
    BrickSuiteWebSocketClient cancelledPairingClient;
    cancelledPairingClient.beginPairing(
        QUrl(QStringLiteral("wss://127.0.0.1:%1").arg(server.serverPort())),
        identity.fingerprint, replacementPairingAttempt.code,
        QStringLiteral("Cancelled pairing"));
    ok &= check(cancelledPairingClient.pairingPendingForTesting(),
                "pairing intent is retained before enrollment starts");
    cancelledPairingClient.cancelPairing();
    ok &= check(!cancelledPairingClient.pairingPendingForTesting(),
                "explicit pairing cancellation clears pending intent");

    BrickSuiteWebSocketClient invalidCodePairingClient;
    bool invalidPairingFailed = false;
    QObject::connect(&invalidCodePairingClient, &BrickSuiteWebSocketClient::pairingFailed,
                     [&](const QString&) { invalidPairingFailed = true; });
    invalidCodePairingClient.beginPairing(
        QUrl(QStringLiteral("wss://127.0.0.1:%1").arg(server.serverPort())),
        identity.fingerprint, QStringLiteral("0000000000000000"),
        QStringLiteral("Invalid-code pairing"));
    invalidCodePairingClient.connectToHost();
    ok &= check(waitUntil([&] { return invalidPairingFailed; })
                    && !invalidCodePairingClient.pairingPendingForTesting(),
                "rejected pairing code clears pending intent");
    invalidCodePairingClient.disconnectFromHost();
    ok &= check(waitUntil([&] {
                    return invalidCodePairingClient.socketStateForTesting()
                        == QAbstractSocket::UnconnectedState;
                }), "invalid-code enrollment socket closes before the next fixture");

    BrickSuiteWebSocketClient mismatchedPairingClient;
    mismatchedPairingClient.beginPairing(
        QUrl(QStringLiteral("wss://127.0.0.1:%1").arg(server.serverPort())),
        QString(64, QLatin1Char('A')), replacementPairingAttempt.code,
        QStringLiteral("Mismatched Host pairing"));
    mismatchedPairingClient.connectToHost();
    ok &= check(waitUntil([&] {
                    return mismatchedPairingClient.status().state
                        == BrickSuiteConnectionState::HostIdentityMismatch;
                }) && !mismatchedPairingClient.pairingPendingForTesting(),
                "Host identity mismatch clears pending pairing intent");
    mismatchedPairingClient.disconnectFromHost();
    ok &= check(waitUntil([&] {
                    return mismatchedPairingClient.socketStateForTesting()
                        == QAbstractSocket::UnconnectedState;
                }), "identity-mismatch enrollment socket closes before re-pairing");

    const auto successfulReplacementAttempt = server.pairingService()->start(&serverError);
    QString replacementDeviceId;
    QString replacementDeviceCredential;
    QObject::connect(&secondPairedClient, &BrickSuiteWebSocketClient::pairingCompleted,
                     [&](const QString& deviceId, const QString& credential) {
        replacementDeviceId = deviceId;
        replacementDeviceCredential = credential;
    });
    // Exercise the Settings action sequence exactly: request closure of the revoked
    // socket, retain one pairing intent, then issue one explicit connect request.
    secondPairedClient.disconnectFromHost();
    secondPairedClient.beginPairing(
        QUrl(QStringLiteral("wss://127.0.0.1:%1").arg(server.serverPort())),
        identity.fingerprint, successfulReplacementAttempt.code,
        QStringLiteral("Protocol 1.3 test device"));
    secondPairedClient.connectToHost();
    ok &= check(waitUntil([&] { return !replacementDeviceId.isEmpty(); })
                    && replacementDeviceId != secondPairedDeviceId
                    && replacementDeviceCredential != secondPairedCredential
                    && !secondPairedClient.pairingPendingForTesting(),
                "revoked Remote pairs again against the already trusted Host identity");
    secondPairedClient.configurePairedDevice(
        QUrl(QStringLiteral("wss://127.0.0.1:%1").arg(server.serverPort())),
        identity.fingerprint, replacementDeviceId, replacementDeviceCredential, false);
    secondPairedClient.connectToHost();
    bool replacementAuthenticated = false;
    ok &= check(waitForResult(secondPairedClient, &replacementAuthenticated, &resultMessage)
                    && replacementAuthenticated
                    && server.authenticatedDeviceIdsForTesting().contains(replacementDeviceId),
                "re-paired Remote authenticates with its new device identity and credential");
    secondPairedClient.disconnectFromHost();

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
    ok &= check(server.broadcastInvalidation(broadcast) == 3,
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
    ok &= check(server.broadcastInvalidation(broadcast) == 2,
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
    ok &= check(server.broadcastInvalidation(broadcast) == 3,
                "reconnected Client rejoins authenticated broadcasts");
    ok &= check(waitUntil([&]() {
        return firstEvents == firstEventsBeforeDisconnectedBroadcast + 1
            && secondEvents == secondEventsBeforeDisconnectedBroadcast + 2;
    }), "new-session invalidation is accepted after reconnect");

    secondClient.disconnectFromHost();
    pairedClient.disconnectFromHost();
    unauthenticatedSocket.close();

    const auto failPairedAuthentication = [&]() {
        BrickSuiteWebSocketClient attempt;
        attempt.configurePairedDevice(
            QUrl(QStringLiteral("wss://127.0.0.1:%1").arg(server.serverPort())),
            identity.fingerprint, pairedDeviceId, QStringLiteral("wrong-device-credential"), false);
        attempt.connectToHost();
        bool attemptSuccess = true;
        const bool failed = waitForResult(attempt, &attemptSuccess, &resultMessage)
            && !attemptSuccess;
        attempt.disconnectFromHost();
        waitUntil([&] {
            return attempt.socketStateForTesting() == QAbstractSocket::UnconnectedState;
        });
        return failed;
    };
    ok &= check(failPairedAuthentication() && failPairedAuthentication()
                    && failPairedAuthentication(),
                "paired-device failures accumulate across independent connections");
    BrickSuiteWebSocketClient throttledDeviceClient;
    throttledDeviceClient.configurePairedDevice(
        QUrl(QStringLiteral("wss://127.0.0.1:%1").arg(server.serverPort())),
        identity.fingerprint, pairedDeviceId, pairedCredential, false);
    throttledDeviceClient.connectToHost();
    bool throttledSuccess = true;
    ok &= check(waitForResult(throttledDeviceClient, &throttledSuccess, &resultMessage)
                    && !throttledSuccess
                    && throttledDeviceClient.status().state
                        == BrickSuiteConnectionState::AuthenticationThrottled
                    && !throttledDeviceClient.reconnectTimerActiveForTesting(),
                "correct credential is temporarily throttled without a reconnect storm");
    throttledDeviceClient.disconnectFromHost();
    BrickSuiteWebSocketClient unrelatedDeviceClient;
    unrelatedDeviceClient.configurePairedDevice(
        QUrl(QStringLiteral("wss://127.0.0.1:%1").arg(server.serverPort())),
        identity.fingerprint, replacementDeviceId, replacementDeviceCredential, false);
    unrelatedDeviceClient.connectToHost();
    bool unrelatedDeviceSuccess = false;
    ok &= check(waitForResult(unrelatedDeviceClient, &unrelatedDeviceSuccess, &resultMessage)
                    && unrelatedDeviceSuccess,
                "throttling one paired identity does not block another paired device");
    unrelatedDeviceClient.disconnectFromHost();

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
    wrongTokenClient.disconnectFromHost();
    waitUntil([&] {
        return wrongTokenClient.socketStateForTesting() == QAbstractSocket::UnconnectedState;
    });
    const auto failLegacyAuthentication = [&]() {
        BrickSuiteWebSocketClient attempt;
        attempt.configure(
            QUrl(QStringLiteral("wss://127.0.0.1:%1").arg(server.serverPort())),
            identity.fingerprint, QStringLiteral("another-wrong-token"), false);
        attempt.connectToHost();
        bool attemptSuccess = true;
        const bool failed = waitForResult(attempt, &attemptSuccess, &resultMessage)
            && !attemptSuccess;
        attempt.disconnectFromHost();
        waitUntil([&] {
            return attempt.socketStateForTesting() == QAbstractSocket::UnconnectedState;
        });
        return failed;
    };
    ok &= check(failLegacyAuthentication() && failLegacyAuthentication(),
                "legacy shared-token failures accumulate across independent connections");
    BrickSuiteWebSocketClient throttledLegacyClient;
    throttledLegacyClient.configure(
        QUrl(QStringLiteral("wss://127.0.0.1:%1").arg(server.serverPort())),
        identity.fingerprint, token, false);
    throttledLegacyClient.connectToHost();
    bool throttledLegacySuccess = true;
    ok &= check(waitForResult(throttledLegacyClient, &throttledLegacySuccess, &resultMessage)
                    && !throttledLegacySuccess
                    && throttledLegacyClient.status().state
                        == BrickSuiteConnectionState::AuthenticationThrottled,
                "legacy shared-token authentication is throttled across reconnects");
    throttledLegacyClient.disconnectFromHost();
    server.stop();
    ok &= check(server.startWithIdentity(QHostAddress::LocalHost, 0, token, identity, &serverError),
                "Host restart clears transient authentication throttling");
    BrickSuiteWebSocketClient postRestartLegacyClient;
    postRestartLegacyClient.configure(
        QUrl(QStringLiteral("wss://127.0.0.1:%1").arg(server.serverPort())),
        identity.fingerprint, token, false);
    postRestartLegacyClient.connectToHost();
    bool postRestartLegacySuccess = false;
    ok &= check(waitForResult(postRestartLegacyClient, &postRestartLegacySuccess, &resultMessage)
                    && postRestartLegacySuccess,
                "legacy Client authenticates after Host restart clears transient throttle");
    postRestartLegacyClient.disconnectFromHost();

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

    const QString replacementToken = BrickSuiteAuthentication::generateAccessToken(&credentialError);
    ok &= check(!replacementToken.isEmpty() && replacementToken != token,
                "rotated Host token is independently random");
    BrickSuiteWebSocketServer rotatedServer;
    ok &= check(rotatedServer.startWithIdentity(QHostAddress::LocalHost, 0, replacementToken,
                                                identity, &serverError),
                "Host starts with replacement token");
    BrickSuiteWebSocketClient restartedPairedClient;
    restartedPairedClient.configurePairedDevice(
        QUrl(QStringLiteral("wss://127.0.0.1:%1").arg(rotatedServer.serverPort())),
        identity.fingerprint, pairedDeviceId, pairedCredential, false);
    restartedPairedClient.connectToHost();
    bool restartedPairedSuccess = false;
    ok &= check(waitForResult(restartedPairedClient, &restartedPairedSuccess, &resultMessage)
                    && restartedPairedSuccess,
                "Host restart preserves paired-device registry and credential authentication");
    BrickSuiteWebSocketClient oldTokenClient;
    success = true;
    oldTokenClient.configure(
        QUrl(QStringLiteral("wss://127.0.0.1:%1").arg(rotatedServer.serverPort())),
        identity.fingerprint, token, false);
    oldTokenClient.connectToHost();
    ok &= check(waitForResult(oldTokenClient, &success, &resultMessage) && !success,
                "old token rejected after rotated credential is active");
    BrickSuiteWebSocketClient replacementTokenClient;
    success = false;
    replacementTokenClient.configure(
        QUrl(QStringLiteral("wss://127.0.0.1:%1").arg(rotatedServer.serverPort())),
        identity.fingerprint, replacementToken, false);
    replacementTokenClient.connectToHost();
    ok &= check(waitForResult(replacementTokenClient, &success, &resultMessage) && success
                    && rotatedServer.fingerprint() == identity.fingerprint,
                "replacement token authenticates without changing Host fingerprint");
    PairedDeviceAdministrationService rotatedAdministration(
        *rotatedServer.pairedDeviceRegistry(), rotatedServer);
    const QString fingerprintBeforeRevokeAll = rotatedServer.fingerprint();
    const auto revokeAll = rotatedAdministration.revokeAllDevices();
    ok &= check(revokeAll.success
                    && waitUntil([&] {
                        return restartedPairedClient.status().state
                            == BrickSuiteConnectionState::DeviceRevoked;
                    })
                    && replacementTokenClient.status().state
                        == BrickSuiteConnectionState::ConnectedAuthenticated
                    && rotatedServer.pairedDeviceRegistry()->devices().isEmpty()
                    && rotatedServer.fingerprint() == fingerprintBeforeRevokeAll,
                "revoke all disconnects paired devices while preserving legacy access and Host identity");
    replacementTokenClient.disconnectFromHost();
    rotatedServer.stop();

    QString cleanupError;
    ok &= check(CredentialStore::remove(
                    identityCredential,
                    &cleanupError), "test Host identity credential cleanup");
    CredentialStore::remove(hostTokenCredential, &cleanupError);
    if (!pairedDeviceId.isEmpty())
        CredentialStore::remove(BrickSuitePairingService::credentialReference(pairedDeviceId),
                                &cleanupError);
    if (!secondPairedDeviceId.isEmpty())
        CredentialStore::remove(BrickSuitePairingService::credentialReference(secondPairedDeviceId),
                                &cleanupError);
    QFile::remove(PairedDeviceRegistry::defaultPath());
    QFile::remove(BrickSuiteHostIdentity::certificatePath());
    return ok ? 0 : 1;
}
