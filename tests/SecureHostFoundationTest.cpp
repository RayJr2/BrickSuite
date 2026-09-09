#include "../src/network/BrickSuiteAuthentication.h"
#include "../src/network/BrickSuiteHostIdentity.h"
#include "../src/network/BrickSuiteProtocol.h"
#include "../src/network/BrickSuiteOperationDispatcher.h"
#include "../src/network/BrickSuiteWebSocketClient.h"
#include "../src/network/BrickSuiteWebSocketServer.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QHostAddress>
#include <QJsonDocument>
#include <QTimer>
#include <QStandardPaths>
#include <QFile>

#include "../src/services/CredentialStore.h"

#include <iostream>

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
    client.disconnectFromHost();

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

    server.stop();
    ok &= check(!server.isListening(), "server shutdown stops listener");

    QString cleanupError;
    ok &= check(CredentialStore::remove(
                    QStringLiteral("BrickSuiteHostTlsIdentity.Test.SecureHostFoundationTest"),
                    &cleanupError), "test Host identity credential cleanup");
    QFile::remove(BrickSuiteHostIdentity::certificatePath());
    return ok ? 0 : 1;
}
