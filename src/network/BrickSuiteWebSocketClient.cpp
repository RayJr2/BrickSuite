#include "BrickSuiteWebSocketClient.h"

#include "BrickSuiteAuthentication.h"
#include "BrickSuiteHostIdentity.h"

#include <QDateTime>
#include <QJsonArray>
#include <QRandomGenerator>
#include <QSslCertificate>
#include <QSslConfiguration>
#include <QSslError>

BrickSuiteWebSocketClient::BrickSuiteWebSocketClient(QObject* parent)
    : QObject(parent)
{
    qRegisterMetaType<BrickSuiteConnectionStatus>();
    qRegisterMetaType<BrickSuiteProtocol::Error>();
    m_reconnectTimer.setSingleShot(true);
    m_socket.setMaxAllowedIncomingMessageSize(BrickSuiteProtocol::MaximumMessageBytes);
    m_socket.setMaxAllowedIncomingFrameSize(BrickSuiteProtocol::MaximumMessageBytes);
    connect(&m_reconnectTimer, &QTimer::timeout, this, [this]() {
        if (!m_explicitDisconnect) connectToHost();
    });
    connect(&m_socket, &QWebSocket::connected,
            this, &BrickSuiteWebSocketClient::handleConnected);
    connect(&m_socket, &QWebSocket::disconnected,
            this, &BrickSuiteWebSocketClient::handleDisconnected);
    connect(&m_socket, &QWebSocket::textMessageReceived,
            this, &BrickSuiteWebSocketClient::handleText);
    connect(&m_socket, &QWebSocket::sslErrors,
            this, &BrickSuiteWebSocketClient::handleSslErrors);
    connect(&m_socket, &QWebSocket::errorOccurred, this,
            [this](QAbstractSocket::SocketError) {
        if (m_status.state != BrickSuiteConnectionState::HostIdentityMismatch
            && m_status.state != BrickSuiteConnectionState::AuthenticationFailed)
            setStatus(BrickSuiteConnectionState::Error,
                      QStringLiteral("Unable to establish the secure Host connection."));
    });
}

BrickSuiteWebSocketClient::~BrickSuiteWebSocketClient()
{
    disconnectFromHost();
}

void BrickSuiteWebSocketClient::configure(const QUrl& endpoint,
                                           const QString& trustedFingerprint,
                                           const QString& accessToken,
                                           bool reconnectAutomatically)
{
    m_endpoint = endpoint;
    m_trustedFingerprint = BrickSuiteHostIdentity::normalizedFingerprint(trustedFingerprint);
    m_accessToken = accessToken;
    m_reconnectAutomatically = reconnectAutomatically;
}

void BrickSuiteWebSocketClient::connectToHost()
{
    if (!m_endpoint.isValid() || m_endpoint.scheme() != QStringLiteral("wss")
        || m_endpoint.host().isEmpty() || m_endpoint.port() <= 0) {
        setStatus(BrickSuiteConnectionState::Error,
                  QStringLiteral("Configure a valid wss:// Host endpoint and port."));
        emit testConnectionCompleted(false, m_status.message);
        return;
    }
    if (!m_endpoint.userInfo().isEmpty() || m_endpoint.hasQuery() || m_endpoint.hasFragment()) {
        setStatus(BrickSuiteConnectionState::Error,
                  QStringLiteral("The Host endpoint must not contain credentials, query, or fragment data."));
        emit testConnectionCompleted(false, m_status.message);
        return;
    }
    m_explicitDisconnect = false;
    m_reconnectTimer.stop();
    m_connectTimer.start();
    qInfo().noquote() << "Connecting to BrickSuite Host" << m_endpoint.host()
                      << "on port" << m_endpoint.port();
    setStatus(BrickSuiteConnectionState::Connecting,
              QStringLiteral("Connecting securely to BrickSuite Host..."));
    QSslConfiguration ssl = QSslConfiguration::defaultConfiguration();
    ssl.setProtocol(QSsl::TlsV1_2OrLater);
    ssl.setPeerVerifyMode(QSslSocket::VerifyPeer);
    m_socket.setSslConfiguration(ssl);
    m_socket.open(m_endpoint);
}

void BrickSuiteWebSocketClient::disconnectFromHost()
{
    m_explicitDisconnect = true;
    m_reconnectTimer.stop();
    failPending(QStringLiteral("TIMEOUT"), QStringLiteral("The Host connection closed."), true);
    m_socket.close(QWebSocketProtocol::CloseCodeNormal, QStringLiteral("Client disconnect."));
    setStatus(BrickSuiteConnectionState::Disconnected, QStringLiteral("Disconnected."));
}

BrickSuiteConnectionStatus BrickSuiteWebSocketClient::status() const { return m_status; }
QString BrickSuiteWebSocketClient::presentedFingerprint() const { return m_presentedFingerprint; }
QJsonObject BrickSuiteWebSocketClient::capabilities() const { return m_capabilities; }

QString BrickSuiteWebSocketClient::sendRequest(const QString& operation,
                                                const QJsonObject& payload)
{
    return enqueueRequest(operation, payload, nullptr, {}, {},
                          BrickSuiteProtocol::RequestTimeoutMs);
}

QString BrickSuiteWebSocketClient::sendRequest(
    const QString& operation, const QJsonObject& payload, QObject* context,
    Completion completion, Failure failure, int timeoutMs)
{
    if (!context) return {};
    return enqueueRequest(operation, payload, context, std::move(completion),
                          std::move(failure), timeoutMs);
}

bool BrickSuiteWebSocketClient::supportsOperation(const QString& operation) const
{
    const QJsonArray operations = m_capabilities.value(QStringLiteral("operations")).toArray();
    for (const QJsonValue& value : operations)
        if (value.toString() == operation) return true;
    return false;
}

QString BrickSuiteWebSocketClient::enqueueRequest(
    const QString& operation, const QJsonObject& payload, QObject* context,
    Completion completion, Failure failure, int timeoutMs)
{
    if (m_pending.size() >= BrickSuiteProtocol::MaximumOutstandingRequests)
        return {};
    const auto message = BrickSuiteProtocol::request(operation, payload);
    Pending pending;
    pending.operation = operation;
    pending.context = context;
    pending.completion = std::move(completion);
    pending.failure = std::move(failure);
    pending.timer = new QTimer(this);
    pending.timer->setSingleShot(true);
    connect(pending.timer, &QTimer::timeout, this, [this, id = message.requestId]() {
        auto it = m_pending.find(id);
        if (it == m_pending.end()) return;
        Pending pending = std::move(it.value());
        pending.timer->deleteLater();
        m_pending.erase(it);
        const BrickSuiteProtocol::Error error{QStringLiteral("TIMEOUT"),
            QStringLiteral("The Host request timed out."), true};
        emit requestFailed(id, error);
        if (pending.context && pending.failure) pending.failure(error);
        emit testConnectionCompleted(false, error.message);
    });
    pending.timer->start(qBound(1, timeoutMs, BrickSuiteProtocol::RequestTimeoutMs));
    m_pending.insert(message.requestId, pending);
    m_socket.sendTextMessage(QString::fromUtf8(BrickSuiteProtocol::serialize(message)));
    return message.requestId;
}

void BrickSuiteWebSocketClient::setStatus(BrickSuiteConnectionState state,
                                          const QString& message)
{
    m_status = {state, message};
    emit statusChanged(m_status);
}

void BrickSuiteWebSocketClient::handleConnected()
{
    setStatus(BrickSuiteConnectionState::VerifyingHost,
              QStringLiteral("Secure transport established; verifying Host identity..."));
    sendHello();
}

void BrickSuiteWebSocketClient::handleDisconnected()
{
    failPending(QStringLiteral("TIMEOUT"), QStringLiteral("The Host connection was interrupted."), true);
    if (!m_explicitDisconnect && m_reconnectAutomatically
        && m_status.state != BrickSuiteConnectionState::HostIdentityMismatch
        && m_status.state != BrickSuiteConnectionState::AuthenticationFailed
        && m_status.state != BrickSuiteConnectionState::IncompatibleProtocol) {
        scheduleReconnect();
    } else if (m_status.state != BrickSuiteConnectionState::HostIdentityMismatch
               && m_status.state != BrickSuiteConnectionState::AuthenticationFailed
               && m_status.state != BrickSuiteConnectionState::IncompatibleProtocol) {
        setStatus(BrickSuiteConnectionState::Disconnected, QStringLiteral("Disconnected."));
    }
}

void BrickSuiteWebSocketClient::handleSslErrors(const QList<QSslError>& errors)
{
    const QSslCertificate certificate = m_socket.sslConfiguration().peerCertificate();
    m_presentedFingerprint = BrickSuiteHostIdentity::fingerprint(certificate);
    const QString presented = BrickSuiteHostIdentity::normalizedFingerprint(m_presentedFingerprint);
    if (m_trustedFingerprint.isEmpty()) {
        setStatus(BrickSuiteConnectionState::HostIdentityMismatch,
                  QStringLiteral("The Host certificate has not been trusted yet."));
        emit trustRequired(m_presentedFingerprint);
        m_socket.abort();
        return;
    }
    if (presented.isEmpty() || presented != m_trustedFingerprint) {
        setStatus(BrickSuiteConnectionState::HostIdentityMismatch,
                  QStringLiteral("The Host certificate fingerprint does not match the trusted Host."));
        emit testConnectionCompleted(false, m_status.message);
        m_socket.abort();
        return;
    }
    QList<QSslError> permitted;
    for (const QSslError& error : errors) {
        switch (error.error()) {
        case QSslError::SelfSignedCertificate:
        case QSslError::SelfSignedCertificateInChain:
        case QSslError::UnableToGetLocalIssuerCertificate:
        case QSslError::UnableToVerifyFirstCertificate:
        case QSslError::HostNameMismatch:
            permitted.append(error);
            break;
        default:
            setStatus(BrickSuiteConnectionState::Error,
                      QStringLiteral("The Host TLS certificate failed security validation."));
            emit testConnectionCompleted(false, m_status.message);
            m_socket.abort();
            return;
        }
    }
    // Pinning supplies Host identity for the deliberately self-signed certificate. Hostname
    // mismatch is accounted for because one persistent Host identity may be reached through
    // LAN, public-IP, and dynamic-DNS endpoints. No error is ignored unless the exact DER
    // certificate SHA-256 fingerprint matched above.
    m_socket.ignoreSslErrors(permitted);
}

void BrickSuiteWebSocketClient::handleText(const QString& text)
{
    const auto parsed = BrickSuiteProtocol::parse(text.toUtf8());
    if (!parsed.valid || parsed.message.type == BrickSuiteProtocol::MessageType::Request) {
        setStatus(BrickSuiteConnectionState::Error,
                  QStringLiteral("The Host returned an invalid protocol message."));
        m_socket.abort();
        return;
    }
    handleResponse(parsed.message);
}

void BrickSuiteWebSocketClient::handleResponse(const BrickSuiteProtocol::Message& message)
{
    auto it = m_pending.find(message.requestId);
    if (it == m_pending.end()) return; // Unsolicited/stale response.
    Pending pending = std::move(it.value());
    const QString operation = pending.operation;
    pending.timer->stop();
    pending.timer->deleteLater();
    m_pending.erase(it);
    if (message.type == BrickSuiteProtocol::MessageType::Error) {
        if (message.error.code == QStringLiteral("AUTH_FAILED")) {
            setStatus(BrickSuiteConnectionState::AuthenticationFailed,
                      QStringLiteral("BrickSuite Host authentication failed."));
            m_explicitDisconnect = true;
        } else if (message.error.code == QStringLiteral("INCOMPATIBLE_PROTOCOL")) {
            setStatus(BrickSuiteConnectionState::IncompatibleProtocol, message.error.message);
            m_explicitDisconnect = true;
        }
        emit requestFailed(message.requestId, message.error);
        if (pending.context && pending.failure) pending.failure(message.error);
        emit testConnectionCompleted(false, message.error.message);
        return;
    }
    emit requestCompleted(message.requestId, message.payload);
    if (pending.context && pending.completion) pending.completion(message.payload);
    if (operation == QStringLiteral("system.hello")) {
        sendAuthentication(message);
    } else if (operation == QStringLiteral("system.authenticate")) {
        setStatus(BrickSuiteConnectionState::Authenticating,
                  QStringLiteral("Authentication succeeded; loading capabilities..."));
        sendRequest(QStringLiteral("system.capabilities"));
    } else if (operation == QStringLiteral("system.capabilities")) {
        m_capabilities = message.payload;
        m_reconnectAttempt = 0;
        setStatus(BrickSuiteConnectionState::ConnectedAuthenticated,
                  QStringLiteral("Connected and authenticated to BrickSuite %1 — protocol %2.%3")
                      .arg(message.payload.value(QStringLiteral("brickSuiteVersion")).toString())
                      .arg(message.payload.value(QStringLiteral("protocolMajor")).toInt())
                      .arg(message.payload.value(QStringLiteral("protocolMinor")).toInt()));
        qInfo() << "BrickSuite Host secure connection and authentication completed in"
                << m_connectTimer.elapsed() << "ms.";
        emit testConnectionCompleted(true, m_status.message);
    }
}

void BrickSuiteWebSocketClient::sendHello()
{
    sendRequest(QStringLiteral("system.hello"));
}

void BrickSuiteWebSocketClient::sendAuthentication(const BrickSuiteProtocol::Message& hello)
{
    if (hello.payload.value(QStringLiteral("protocolMajor")).toInt(-1) != BrickSuiteProtocol::Major) {
        setStatus(BrickSuiteConnectionState::IncompatibleProtocol,
                  QStringLiteral("The Host uses an incompatible BrickSuite protocol."));
        m_socket.abort();
        return;
    }
    const QByteArray sessionId = QByteArray::fromBase64(
        hello.payload.value(QStringLiteral("sessionId")).toString().toLatin1());
    const QByteArray challenge = QByteArray::fromBase64(
        hello.payload.value(QStringLiteral("challenge")).toString().toLatin1());
    QString randomError;
    const QByteArray nonce = BrickSuiteAuthentication::secureRandom(32, &randomError);
    const int minor = hello.payload.value(QStringLiteral("protocolMinor")).toInt();
    QString hmacError;
    const QByteArray proof = BrickSuiteAuthentication::hmacSha256(
        m_accessToken.toUtf8(), BrickSuiteAuthentication::authenticationInput(
            challenge, nonce, sessionId, BrickSuiteProtocol::Major, minor), &hmacError);
    if (sessionId.isEmpty() || challenge.size() != 32 || nonce.size() != 32 || proof.size() != 32) {
        setStatus(BrickSuiteConnectionState::Error,
                  QStringLiteral("Unable to prepare secure Host authentication."));
        m_socket.abort();
        return;
    }
    setStatus(BrickSuiteConnectionState::Authenticating,
              QStringLiteral("Authenticating with BrickSuite Host..."));
    sendRequest(QStringLiteral("system.authenticate"), {
        {QStringLiteral("sessionId"), QString::fromLatin1(sessionId.toBase64())},
        {QStringLiteral("clientNonce"), QString::fromLatin1(nonce.toBase64())},
        {QStringLiteral("proof"), QString::fromLatin1(proof.toBase64())}
    });
}

void BrickSuiteWebSocketClient::failPending(const QString& code,
                                            const QString& message, bool retryable)
{
    const auto ids = m_pending.keys();
    for (const QString& id : ids) {
        Pending pending = m_pending.take(id);
        pending.timer->stop();
        pending.timer->deleteLater();
        const BrickSuiteProtocol::Error error{code, message, retryable};
        emit requestFailed(id, error);
        if (pending.context && pending.failure) pending.failure(error);
    }
}

void BrickSuiteWebSocketClient::scheduleReconnect()
{
    static constexpr int delays[] = {1000, 2000, 4000, 8000, 15000, 30000, 60000};
    const int index = qMin(m_reconnectAttempt, 6);
    const int base = delays[index];
    ++m_reconnectAttempt;
    const int jitter = QRandomGenerator::global()->bounded(qMax(1, base / 5)) - base / 10;
    m_reconnectTimer.start(qMax(500, base + jitter));
    qInfo() << "BrickSuite Host reconnect scheduled; attempt" << m_reconnectAttempt;
    setStatus(BrickSuiteConnectionState::Reconnecting,
              QStringLiteral("Host disconnected; reconnecting shortly..."));
}
