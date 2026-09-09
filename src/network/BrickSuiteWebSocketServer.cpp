#include "BrickSuiteWebSocketServer.h"

#include "BrickSuiteAuthentication.h"

#include <QDateTime>
#include <QSslConfiguration>
#include <QWebSocket>
#include <QWebSocketServer>

BrickSuiteWebSocketServer::BrickSuiteWebSocketServer(QObject* parent)
    : QObject(parent)
{
}

BrickSuiteWebSocketServer::~BrickSuiteWebSocketServer()
{
    stop();
}

bool BrickSuiteWebSocketServer::start(const QHostAddress& address, quint16 port,
                                      const QString& accessToken, QString* error)
{
    stop();
    if (accessToken.trimmed().isEmpty()) {
        if (error) *error = QStringLiteral("Generate a BrickSuite Server access token first.");
        return false;
    }
    m_identity = BrickSuiteHostIdentity::loadOrCreate();
    if (!m_identity.success) {
        if (error) *error = m_identity.error;
        return false;
    }
    return startWithIdentity(address, port, accessToken, m_identity, error);
}

bool BrickSuiteWebSocketServer::startWithIdentity(
    const QHostAddress& address, quint16 port, const QString& accessToken,
    const BrickSuiteHostIdentity::Result& identity, QString* error)
{
    stop();
    if (accessToken.trimmed().isEmpty() || !identity.success
        || identity.certificate.isNull() || identity.privateKey.isNull()) {
        if (error) *error = QStringLiteral("A valid Host identity and access token are required.");
        return false;
    }
    m_identity = identity;
    m_server = new QWebSocketServer(QStringLiteral("BrickSuite"),
                                    QWebSocketServer::SecureMode, this);
    QSslConfiguration ssl = QSslConfiguration::defaultConfiguration();
    ssl.setProtocol(QSsl::TlsV1_2OrLater);
    ssl.setLocalCertificate(m_identity.certificate);
    ssl.setPrivateKey(m_identity.privateKey);
    ssl.setPeerVerifyMode(QSslSocket::VerifyNone); // Clients authenticate at the application layer.
    m_server->setSslConfiguration(ssl);
    connect(m_server, &QWebSocketServer::newConnection,
            this, &BrickSuiteWebSocketServer::acceptConnection);
    if (!m_server->listen(address, port)) {
        if (error) *error = QStringLiteral("Unable to listen on the configured address and port.");
        m_server->deleteLater();
        m_server = nullptr;
        return false;
    }
    m_accessToken = accessToken;
    qInfo().noquote() << "BrickSuite Server listening securely on"
                      << m_server->serverAddress().toString() << m_server->serverPort();
    emit statusChanged();
    return true;
}

void BrickSuiteWebSocketServer::stop()
{
    if (!m_server) return;
    m_server->close();
    const auto sockets = m_sessions.keys();
    for (QWebSocket* socket : sockets) {
        socket->close(QWebSocketProtocol::CloseCodeGoingAway,
                      QStringLiteral("BrickSuite is shutting down."));
        socket->deleteLater();
    }
    m_sessions.clear();
    m_server->deleteLater();
    m_server = nullptr;
    m_accessToken.clear();
    emit statusChanged();
}

bool BrickSuiteWebSocketServer::isListening() const { return m_server && m_server->isListening(); }
quint16 BrickSuiteWebSocketServer::serverPort() const { return m_server ? m_server->serverPort() : 0; }
QString BrickSuiteWebSocketServer::fingerprint() const { return m_identity.fingerprint; }

int BrickSuiteWebSocketServer::authenticatedClientCount() const
{
    int count = 0;
    for (const Session& session : m_sessions) count += session.authenticated ? 1 : 0;
    return count;
}

void BrickSuiteWebSocketServer::acceptConnection()
{
    while (m_server && m_server->hasPendingConnections()) {
        QWebSocket* socket = m_server->nextPendingConnection();
        if (!socket) continue;
        if (m_sessions.size() >= BrickSuiteProtocol::MaximumClients) {
            socket->close(QWebSocketProtocol::CloseCodePolicyViolated,
                          QStringLiteral("Server connection limit reached."));
            socket->deleteLater();
            continue;
        }
        Session session;
        QString randomError;
        session.id = BrickSuiteAuthentication::secureRandom(24, &randomError);
        session.connectedMs = QDateTime::currentMSecsSinceEpoch();
        session.authenticationTimer = new QTimer(socket);
        session.authenticationTimer->setSingleShot(true);
        connect(session.authenticationTimer, &QTimer::timeout, socket, [socket]() {
            socket->close(QWebSocketProtocol::CloseCodePolicyViolated,
                          QStringLiteral("Authentication timed out."));
        });
        session.authenticationTimer->start(BrickSuiteProtocol::AuthenticationTimeoutMs);
        m_sessions.insert(socket, session);
        socket->setMaxAllowedIncomingMessageSize(BrickSuiteProtocol::MaximumMessageBytes);
        socket->setMaxAllowedIncomingFrameSize(BrickSuiteProtocol::MaximumMessageBytes);
        connect(socket, &QWebSocket::textMessageReceived, this,
                [this, socket](const QString& text) { receiveText(socket, text); });
        connect(socket, &QWebSocket::binaryMessageReceived, socket, [socket](const QByteArray&) {
            socket->close(QWebSocketProtocol::CloseCodeDatatypeNotSupported,
                          QStringLiteral("Binary protocol messages are not supported."));
        });
        connect(socket, &QWebSocket::disconnected, this,
                [this, socket]() { closeSession(socket); });
        qInfo() << "BrickSuite Server client connected.";
        emit statusChanged();
    }
}

void BrickSuiteWebSocketServer::receiveText(QWebSocket* socket, const QString& text)
{
    const QByteArray bytes = text.toUtf8();
    const auto parsed = BrickSuiteProtocol::parse(bytes);
    if (!parsed.valid) {
        BrickSuiteProtocol::Message placeholder;
        placeholder.requestId = QStringLiteral("invalid");
        placeholder.operation = QStringLiteral("system.invalid");
        reject(socket, placeholder, parsed.error.code, parsed.error.message);
        if (parsed.error.code == QStringLiteral("PAYLOAD_TOO_LARGE"))
            socket->close(QWebSocketProtocol::CloseCodeTooMuchData,
                          QStringLiteral("Message too large."));
        return;
    }
    if (parsed.message.type != BrickSuiteProtocol::MessageType::Request) {
        reject(socket, parsed.message, QStringLiteral("INVALID_REQUEST"),
               QStringLiteral("The Host accepts request messages only."));
        return;
    }
    dispatch(socket, parsed.message);
}

void BrickSuiteWebSocketServer::dispatch(QWebSocket* socket,
                                         const BrickSuiteProtocol::Message& request)
{
    auto it = m_sessions.find(socket);
    if (it == m_sessions.end()) return;
    Session& session = it.value();
    if (request.protocolMajor != BrickSuiteProtocol::Major) {
        reject(socket, request, QStringLiteral("INCOMPATIBLE_PROTOCOL"),
               QStringLiteral("The BrickSuite protocol major version is incompatible."));
        return;
    }
    if (request.operation == QStringLiteral("system.hello")) {
        if (QDateTime::currentMSecsSinceEpoch() < session.nextAuthenticationAllowedMs) {
            reject(socket, request, QStringLiteral("RATE_LIMITED"),
                   QStringLiteral("Wait before retrying authentication."), true);
            return;
        }
        if (!request.payload.isEmpty()) {
            reject(socket, request, QStringLiteral("INVALID_REQUEST"),
                   QStringLiteral("system.hello does not accept payload fields."));
            return;
        }
        QString randomError;
        session.challenge = BrickSuiteAuthentication::secureRandom(32, &randomError);
        session.challengeCreatedMs = QDateTime::currentMSecsSinceEpoch();
        session.challengeConsumed = false;
        send(socket, BrickSuiteProtocol::response(request, {
            {QStringLiteral("protocolMajor"), BrickSuiteProtocol::Major},
            {QStringLiteral("protocolMinor"), qMin(request.protocolMinor, BrickSuiteProtocol::Minor)},
            {QStringLiteral("authentication"), QStringLiteral("HMAC-SHA-256")},
            {QStringLiteral("sessionId"), QString::fromLatin1(session.id.toBase64())},
            {QStringLiteral("challenge"), QString::fromLatin1(session.challenge.toBase64())},
            {QStringLiteral("expiresSeconds"), 30}
        }));
        return;
    }
    if (request.operation == QStringLiteral("system.authenticate")) {
        if (session.authenticated) {
            reject(socket, request, QStringLiteral("INVALID_REQUEST"),
                   QStringLiteral("This connection is already authenticated."));
            socket->close(QWebSocketProtocol::CloseCodePolicyViolated,
                          QStringLiteral("Authentication replay rejected."));
            return;
        }
        if (request.payload.size() != 3
            || !request.payload.value(QStringLiteral("sessionId")).isString()
            || !request.payload.value(QStringLiteral("clientNonce")).isString()
            || !request.payload.value(QStringLiteral("proof")).isString()) {
            reject(socket, request, QStringLiteral("INVALID_REQUEST"),
                   QStringLiteral("The authentication payload is invalid."));
            return;
        }
        const qint64 age = QDateTime::currentMSecsSinceEpoch() - session.challengeCreatedMs;
        const QByteArray clientNonce = QByteArray::fromBase64(
            request.payload.value(QStringLiteral("clientNonce")).toString().toLatin1());
        const QByteArray proof = QByteArray::fromBase64(
            request.payload.value(QStringLiteral("proof")).toString().toLatin1());
        const QByteArray challengeId = QByteArray::fromBase64(
            request.payload.value(QStringLiteral("sessionId")).toString().toLatin1());
        const bool challengeUsable = !session.challenge.isEmpty() && !session.challengeConsumed
            && age >= 0 && age <= BrickSuiteProtocol::AuthenticationTimeoutMs
            && challengeId == session.id && clientNonce.size() == 32 && proof.size() == 32;
        session.challengeConsumed = true;
        QString hmacError;
        const QByteArray expected = challengeUsable
            ? BrickSuiteAuthentication::hmacSha256(
                  m_accessToken.toUtf8(), BrickSuiteAuthentication::authenticationInput(
                      session.challenge, clientNonce, session.id,
                      BrickSuiteProtocol::Major,
                      qMin(request.protocolMinor, BrickSuiteProtocol::Minor)), &hmacError)
            : QByteArray();
        if (!challengeUsable || expected.isEmpty()
            || !BrickSuiteAuthentication::constantTimeEquals(expected, proof)) {
            ++session.authenticationFailures;
            session.nextAuthenticationAllowedMs = QDateTime::currentMSecsSinceEpoch()
                + 1000 * session.authenticationFailures;
            qWarning() << "BrickSuite Server authentication failed.";
            reject(socket, request, QStringLiteral("AUTH_FAILED"),
                   QStringLiteral("BrickSuite authentication failed."));
            if (session.authenticationFailures >= BrickSuiteProtocol::MaximumAuthenticationFailures)
                socket->close(QWebSocketProtocol::CloseCodePolicyViolated,
                              QStringLiteral("Authentication failed."));
            return;
        }
        session.authenticated = true;
        session.authenticationTimer->stop();
        send(socket, BrickSuiteProtocol::response(request,
            {{QStringLiteral("authenticated"), true},
             {QStringLiteral("role"), QStringLiteral("FullBrickSuiteClient")}}));
        qInfo() << "BrickSuite Server client authenticated in"
                << (QDateTime::currentMSecsSinceEpoch() - session.connectedMs)
                << "ms; authenticated clients:" << authenticatedClientCount();
        emit statusChanged();
        return;
    }
    send(socket, m_dispatcher.dispatch(request, session.authenticated));
}

void BrickSuiteWebSocketServer::send(QWebSocket* socket,
                                     const BrickSuiteProtocol::Message& message)
{
    socket->sendTextMessage(QString::fromUtf8(BrickSuiteProtocol::serialize(message)));
}

void BrickSuiteWebSocketServer::reject(QWebSocket* socket,
                                       const BrickSuiteProtocol::Message& request,
                                       const QString& code, const QString& message,
                                       bool retryable)
{
    send(socket, BrickSuiteProtocol::errorResponse(request, code, message, retryable));
}

void BrickSuiteWebSocketServer::closeSession(QWebSocket* socket)
{
    m_sessions.remove(socket);
    socket->deleteLater();
    qInfo() << "BrickSuite Server client disconnected.";
    emit statusChanged();
}
