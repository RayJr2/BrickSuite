#include "BrickSuiteWebSocketClient.h"

#include "BrickSuiteAuthentication.h"
#include "BrickSuiteHostIdentity.h"

#include <QDateTime>
#include <QDebug>
#include <QJsonArray>
#include <QRandomGenerator>
#include <QSslCertificate>
#include <QSslConfiguration>
#include <QSslError>
#include <QThread>
#include <QUuid>

BrickSuiteWebSocketClient::BrickSuiteWebSocketClient(QObject* parent)
    : QObject(parent)
{
    qRegisterMetaType<BrickSuiteConnectionStatus>();
    qRegisterMetaType<BrickSuiteProtocol::Error>();
    qRegisterMetaType<OperationalInvalidation>();
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
    m_deviceId.clear();
    m_pairing = false;
    m_pairingCode.clear();
    m_pairingFriendlyName.clear();
    m_requestedProtocolMinor = 2;
}

void BrickSuiteWebSocketClient::configurePairedDevice(
    const QUrl& endpoint, const QString& trustedFingerprint, const QString& deviceId,
    const QString& credential, bool reconnectAutomatically)
{
    m_endpoint = endpoint;
    m_trustedFingerprint = BrickSuiteHostIdentity::normalizedFingerprint(trustedFingerprint);
    m_deviceId = deviceId.trimmed().toLower();
    m_accessToken = credential;
    m_reconnectAutomatically = reconnectAutomatically;
    m_pairing = false;
    m_pairingCode.clear();
    m_pairingFriendlyName.clear();
    m_requestedProtocolMinor = 3;
}

void BrickSuiteWebSocketClient::beginPairing(
    const QUrl& endpoint, const QString& trustedFingerprint, const QString& code,
    const QString& friendlyName)
{
    m_endpoint = endpoint;
    m_trustedFingerprint = BrickSuiteHostIdentity::normalizedFingerprint(trustedFingerprint);
    m_accessToken.clear();
    m_deviceId.clear();
    m_pairingCode = code.trimmed().toUpper();
    m_pairingFriendlyName = friendlyName.trimmed();
    m_reconnectAutomatically = false;
    m_pairing = true;
    m_requestedProtocolMinor = 3;
}

void BrickSuiteWebSocketClient::setTrustedFingerprint(const QString& fingerprint)
{
    m_trustedFingerprint = BrickSuiteHostIdentity::normalizedFingerprint(fingerprint);
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
    m_dataEpoch.clear();
    m_dataEpochSupported = false;
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
    if (m_authenticated) {
        ++m_authenticatedSessionGeneration;
        m_authenticated = false;
        m_capabilities = {};
        emit authenticatedSessionLost();
    }
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

bool BrickSuiteWebSocketClient::supportsCapability(const QString& capability) const
{
    const QJsonArray values = m_capabilities.value(QStringLiteral("capabilities")).toArray();
    for (const QJsonValue& value : values)
        if (value.toString() == capability) return true;
    return false;
}

quint64 BrickSuiteWebSocketClient::authenticatedSessionGeneration() const
{
    return m_authenticatedSessionGeneration;
}

#ifdef BRICKSUITE_TESTING
void BrickSuiteWebSocketClient::sendProtocolEventForTesting(
    const OperationalInvalidation& invalidation)
{
    m_socket.sendTextMessage(QString::fromUtf8(BrickSuiteProtocol::serialize(
        BrickSuiteProtocol::event(OperationalInvalidation::Operation, invalidation.toPayload()))));
}
#endif

QString BrickSuiteWebSocketClient::enqueueRequest(
    const QString& operation, const QJsonObject& payload, QObject* context,
    Completion completion, Failure failure, int timeoutMs)
{
    if (m_pending.size() >= BrickSuiteProtocol::MaximumOutstandingRequests)
        return {};
    const auto message = BrickSuiteProtocol::request(operation, payload);
    auto versionedMessage = message;
    versionedMessage.protocolMinor = m_requestedProtocolMinor;
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
        qWarning() << "BrickSuite request timed out" << pending.operation << id.left(8);
        emit requestFailed(id, error);
        if (pending.context && pending.failure) pending.failure(error);
        emit testConnectionCompleted(false, error.message);
    });
    pending.timer->start(qBound(1, timeoutMs, BrickSuiteProtocol::RequestTimeoutMs));
    m_pending.insert(versionedMessage.requestId, pending);
    qDebug() << "BrickSuite request queued" << operation << message.requestId.left(8)
             << "pending" << m_pending.size();
    m_socket.sendTextMessage(QString::fromUtf8(BrickSuiteProtocol::serialize(versionedMessage)));
    return versionedMessage.requestId;
}

void BrickSuiteWebSocketClient::setStatus(BrickSuiteConnectionState state,
                                          const QString& message)
{
    if (m_status.state == state && m_status.message == message) return;
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
#ifdef BRICKSUITE_TESTING
    ++m_transportDisconnectCount;
#endif
    failPending(QStringLiteral("TIMEOUT"), QStringLiteral("The Host connection was interrupted."), true);
    if (m_authenticated) {
        ++m_authenticatedSessionGeneration;
        m_authenticated = false;
        m_capabilities = {};
        emit authenticatedSessionLost();
    }
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
    if (parsed.valid && parsed.message.operation.startsWith(QStringLiteral("inventory.")))
        qDebug() << "BrickSuite Client raw WebSocket frame callback"
                 << parsed.message.operation << parsed.message.requestId.left(8)
                 << BrickSuiteProtocol::typeName(parsed.message.type)
                 << "socketThreadCurrent" << (m_socket.thread() == QThread::currentThread())
                 << "clientThreadCurrent" << (thread() == QThread::currentThread());
    if (!parsed.valid || parsed.message.type == BrickSuiteProtocol::MessageType::Request) {
        setStatus(BrickSuiteConnectionState::Error,
                  QStringLiteral("The Host returned an invalid protocol message."));
        m_socket.abort();
        return;
    }
    if (parsed.message.type == BrickSuiteProtocol::MessageType::Event) {
        if (!m_authenticated || parsed.message.operation != OperationalInvalidation::Operation) {
            setStatus(BrickSuiteConnectionState::Error,
                      QStringLiteral("The Host returned an invalid event message."));
            m_socket.abort();
            return;
        }
        if (!supportsCapability(OperationalInvalidation::Capability)) {
            qWarning() << "Host invalidation ignored because the capability was not negotiated.";
            return;
        }
        OperationalInvalidation invalidation;
        QString error;
        if (!OperationalInvalidation::fromPayload(parsed.message.payload, &invalidation, &error)) {
            qWarning().noquote() << "Malformed Host invalidation rejected:" << error;
            setStatus(BrickSuiteConnectionState::Error,
                      QStringLiteral("The Host returned an invalid event payload."));
            m_socket.abort();
            return;
        }
        const bool maintenanceEnded = m_authenticated
            && m_status.state == BrickSuiteConnectionState::HostMaintenance;
        if (maintenanceEnded)
            setStatus(BrickSuiteConnectionState::ConnectedAuthenticated,
                      QStringLiteral("Connected and authenticated to BrickSuite Host."));
        emit invalidationReceived(invalidation, m_authenticatedSessionGeneration);
        if (maintenanceEnded) emit hostMaintenanceEnded();
        return;
    }
    handleResponse(parsed.message);
}

void BrickSuiteWebSocketClient::handleResponse(const BrickSuiteProtocol::Message& message)
{
    auto it = m_pending.find(message.requestId);
    if (it == m_pending.end()) {
        qWarning() << "Unmatched BrickSuite response" << message.operation
                   << message.requestId.left(8) << "pending" << m_pending.size();
        return; // Unsolicited/stale response.
    }
    Pending pending = std::move(it.value());
    const QString operation = pending.operation;
    pending.timer->stop();
    pending.timer->deleteLater();
    m_pending.erase(it);
    qDebug() << "BrickSuite response matched" << operation << message.requestId.left(8)
             << "pending" << m_pending.size();
    if (message.type == BrickSuiteProtocol::MessageType::Error) {
        if (message.error.code == QStringLiteral("AUTH_FAILED")) {
            setStatus(BrickSuiteConnectionState::AuthenticationFailed,
                      QStringLiteral("BrickSuite Host authentication failed."));
            m_explicitDisconnect = true;
        } else if (message.error.code == QStringLiteral("INCOMPATIBLE_PROTOCOL")) {
            setStatus(BrickSuiteConnectionState::IncompatibleProtocol, message.error.message);
            m_explicitDisconnect = true;
        } else if (message.error.code == QStringLiteral("HOST_MAINTENANCE")) {
            setStatus(BrickSuiteConnectionState::HostMaintenance,
                      QStringLiteral("Host Maintenance — shared operations are temporarily unavailable."));
        }
        emit requestFailed(message.requestId, message.error);
        if (operation == QStringLiteral("system.pair")) emit pairingFailed(message.error.message);
        if (pending.context && pending.failure) pending.failure(message.error);
        emit testConnectionCompleted(false, message.error.message);
        return;
    }
    emit requestCompleted(message.requestId, message.payload);
    if (pending.context && pending.completion) pending.completion(message.payload);
    if (operation == QStringLiteral("system.hello")) {
        if (m_pairing) sendPairing();
        else sendAuthentication(message);
    } else if (operation == QStringLiteral("system.pair")) {
        const QString deviceId = message.payload.value(QStringLiteral("deviceId")).toString();
        const QString credential = message.payload.value(QStringLiteral("credential")).toString();
        if (deviceId.isEmpty() || credential.isEmpty()) {
            setStatus(BrickSuiteConnectionState::Error,
                      QStringLiteral("The Host returned an invalid pairing result."));
            m_socket.abort();
            return;
        }
        m_pairing = false;
        m_pairingCode.clear();
        m_pairingFriendlyName.clear();
        m_explicitDisconnect = true;
        m_socket.close(QWebSocketProtocol::CloseCodeNormal,
                       QStringLiteral("Pairing completed; reconnecting to authenticate."));
        emit pairingCompleted(deviceId, credential);
    } else if (operation == QStringLiteral("system.authenticate")) {
        setStatus(BrickSuiteConnectionState::Authenticating,
                  QStringLiteral("Authentication succeeded; loading capabilities..."));
        sendRequest(QStringLiteral("system.capabilities"));
    } else if (operation == QStringLiteral("system.capabilities")) {
        const bool epochPresent = message.payload.contains(QStringLiteral("dataEpoch"));
        const QString epoch = message.payload.value(QStringLiteral("dataEpoch")).toString();
        if (epochPresent && (epoch.isEmpty() || QUuid(epoch).isNull()
            || QUuid(epoch).toString(QUuid::WithoutBraces).compare(epoch, Qt::CaseInsensitive) != 0)) {
            setStatus(BrickSuiteConnectionState::Error,
                      QStringLiteral("The Host returned an invalid data epoch."));
            m_socket.abort();
            return;
        }
        m_capabilities = message.payload;
        m_dataEpochSupported = epochPresent;
        m_dataEpoch = epoch.toLower();
        m_reconnectAttempt = 0;
        const QSslCertificate certificate = m_socket.sslConfiguration().peerCertificate();
        m_presentedFingerprint = BrickSuiteHostIdentity::fingerprint(certificate);
        ++m_authenticatedSessionGeneration;
        m_authenticated = true;
        setStatus(BrickSuiteConnectionState::ConnectedAuthenticated,
                  QStringLiteral("Connected and authenticated to BrickSuite %1 — protocol %2.%3")
                      .arg(message.payload.value(QStringLiteral("brickSuiteVersion")).toString())
                      .arg(message.payload.value(QStringLiteral("protocolMajor")).toInt())
                      .arg(message.payload.value(QStringLiteral("protocolMinor")).toInt()));
        qInfo() << "BrickSuite Host secure connection and authentication completed.";
        qDebug() << "BrickSuite Host secure connection and authentication completed in"
                 << m_connectTimer.elapsed() << "ms.";
        emit authenticatedSessionEstablished(m_presentedFingerprint);
        emit authenticatedSessionEstablishedWithEpoch(m_presentedFingerprint,
                                                       m_dataEpoch,
                                                       m_dataEpochSupported);
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
    QJsonObject payload{
        {QStringLiteral("sessionId"), QString::fromLatin1(sessionId.toBase64())},
        {QStringLiteral("clientNonce"), QString::fromLatin1(nonce.toBase64())},
        {QStringLiteral("proof"), QString::fromLatin1(proof.toBase64())}
    };
    if (m_requestedProtocolMinor >= 3) payload.insert(QStringLiteral("deviceId"), m_deviceId);
    sendRequest(QStringLiteral("system.authenticate"), payload);
}

void BrickSuiteWebSocketClient::sendPairing()
{
    setStatus(BrickSuiteConnectionState::Authenticating,
              QStringLiteral("Pairing this device with BrickSuite Host..."));
    sendRequest(QStringLiteral("system.pair"), {
        {QStringLiteral("code"), m_pairingCode},
        {QStringLiteral("friendlyName"), m_pairingFriendlyName},
        {QStringLiteral("clientVersion"), QStringLiteral(BRICKSUITE_VERSION)},
#if defined(Q_OS_WIN)
        {QStringLiteral("platform"), QStringLiteral("Windows")}
#elif defined(Q_OS_MACOS) || defined(Q_OS_MAC)
        {QStringLiteral("platform"), QStringLiteral("macOS")}
#else
        {QStringLiteral("platform"), QStringLiteral("Linux")}
#endif
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
        if (pending.operation == QStringLiteral("system.pair")) emit pairingFailed(message);
        if (pending.context && pending.failure) pending.failure(error);
    }
}

void BrickSuiteWebSocketClient::scheduleReconnect()
{
#ifdef BRICKSUITE_TESTING
    ++m_reconnectScheduleCount;
#endif
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
