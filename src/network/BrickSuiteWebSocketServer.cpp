#include "BrickSuiteWebSocketServer.h"

#include "BrickSuiteAuthentication.h"
#include "../services/CredentialStore.h"

#include <QDateTime>
#include <QJsonArray>
#include <QPointer>
#include <QThread>
#include <QSslConfiguration>
#include <QWebSocket>
#include <QWebSocketServer>

BrickSuiteWebSocketServer::BrickSuiteWebSocketServer(QObject* parent)
    : QObject(parent)
    , m_registry(std::make_unique<PairedDeviceRegistry>())
    , m_pairing(std::make_unique<BrickSuitePairingService>(*m_registry))
{
    m_authenticationClock.start();
    QString registryError;
    if (!m_registry->load(&registryError))
        qWarning().noquote() << "Paired-device administration unavailable:" << registryError;
}

BrickSuiteWebSocketServer::~BrickSuiteWebSocketServer()
{
    stop();
}

BrickSuiteOperationDispatcher& BrickSuiteWebSocketServer::operationDispatcher()
{ return m_dispatcher; }

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
    m_authenticationThrottle.clear();
    m_authenticationClock.restart();
    if (accessToken.trimmed().isEmpty() || !identity.success
        || identity.certificate.isNull() || identity.privateKey.isNull()) {
        if (error) *error = QStringLiteral("A valid Host identity and access token are required.");
        return false;
    }
    QString registryError;
    if (!m_registry->load(&registryError)) {
        if (error) *error = registryError;
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
    if (m_pairing) m_pairing->cancel();
    if (!m_server) return;
    m_server->close();
    const auto sockets = m_sessions.keys();
    for (QWebSocket* socket : sockets) {
        const QString sessionId = QString::fromLatin1(m_sessions.value(socket).id.toBase64());
        if (!sessionId.isEmpty()) emit sessionDisconnected(sessionId);
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

int BrickSuiteWebSocketServer::authenticatedDeviceSessionCount(const QString& deviceId) const
{
    int count = 0;
    for (const Session& session : m_sessions)
        if (session.authenticated && session.deviceId.compare(deviceId, Qt::CaseInsensitive) == 0)
            ++count;
    return count;
}

int BrickSuiteWebSocketServer::legacyAuthenticatedClientCount() const
{
    int count = 0;
    for (const Session& session : m_sessions)
        if (session.authenticated && session.legacySharedToken) ++count;
    return count;
}

int BrickSuiteWebSocketServer::disconnectAuthenticatedDevice(const QString& deviceId)
{
    int count = 0;
    const auto sockets = m_sessions.keys();
    for (QWebSocket* socket : sockets) {
        const auto it = m_sessions.constFind(socket);
        if (it == m_sessions.cend() || !it->authenticated
            || it->deviceId.compare(deviceId, Qt::CaseInsensitive) != 0)
            continue;
        send(socket, BrickSuiteProtocol::event(QStringLiteral("system.deviceRevoked"), {}));
        QTimer::singleShot(0, socket, [socket] {
            socket->close(QWebSocketProtocol::CloseCodePolicyViolated,
                          QStringLiteral("Device authorization revoked."));
        });
        ++count;
    }
    return count;
}

int BrickSuiteWebSocketServer::disconnectAllAuthenticatedDevices()
{
    int count = 0;
    QStringList deviceIds;
    for (const Session& session : m_sessions)
        if (session.authenticated && !session.deviceId.isEmpty()
            && !deviceIds.contains(session.deviceId)) deviceIds.append(session.deviceId);
    for (const QString& deviceId : deviceIds) count += disconnectAuthenticatedDevice(deviceId);
    return count;
}

#ifdef BRICKSUITE_TESTING
QStringList BrickSuiteWebSocketServer::authenticatedDeviceIdsForTesting() const
{
    QStringList ids;
    for (const Session& session : m_sessions) {
        if (session.authenticated && !session.deviceId.isEmpty()) ids.append(session.deviceId);
    }
    return ids;
}

#endif

int BrickSuiteWebSocketServer::broadcastInvalidation(OperationalInvalidation invalidation)
{
    if (QThread::currentThread() != thread()) {
        qWarning() << "Invalidation broadcast must execute on the WebSocket Server thread.";
        return 0;
    }
    if (m_nextInvalidationSequence == 0
        || m_nextInvalidationSequence > OperationalInvalidation::MaximumJsonInteger)
        m_nextInvalidationSequence = 1;
    invalidation.sequence = m_nextInvalidationSequence++;
    QString error;
    if (!OperationalInvalidation::validate(invalidation, true, &error)) {
        qWarning().noquote() << "Invalidation broadcast rejected:" << error;
        return 0;
    }
    const auto message = BrickSuiteProtocol::event(OperationalInvalidation::Operation,
                                                    invalidation.toPayload());
    int recipients = 0;
    for (auto it = m_sessions.constBegin(); it != m_sessions.constEnd(); ++it) {
        if (!it->authenticated || !it->invalidationsReady || it->protocolMinor < 1 || !it.key())
            continue;
        send(it.key(), message);
        ++recipients;
    }
    qDebug() << "Host invalidation" << invalidation.sequence << "domains"
             << invalidation.domains.size() << "workspace"
             << invalidation.workspaceId.value_or(0) << "recipients" << recipients;
    return recipients;
}

void BrickSuiteWebSocketServer::setOperationalAdmissionOpen(bool open)
{
    m_operationalAdmissionOpen = open;
    emit statusChanged();
}

void BrickSuiteWebSocketServer::broadcastFullOperationalInvalidation()
{
    OperationalInvalidation value;
    // Host-wide domains form a valid protocol-1.2 event without inventing a
    // Workspace scope. Clients use maintenance recovery to reload their full
    // current Workspace projection when this event arrives.
    value.domains = {OperationalInvalidationDomain::Workspaces,
                     OperationalInvalidationDomain::PartReferenceCustomizations};
    broadcastInvalidation(value);
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
        session.backpressureTimer = new QTimer(socket);
        session.backpressureTimer->setSingleShot(true);
        connect(session.backpressureTimer, &QTimer::timeout, socket, [this, socket] {
            const auto it = m_sessions.constFind(socket);
            if (it == m_sessions.cend()) return;
            if (socket->bytesToWrite() >= HostOutboundPolicy::HighWaterBytes) {
                ++m_slowClientDisconnectCount;
                qWarning() << "BrickSuite Host disconnected a persistently slow client; buffered bytes"
                           << socket->bytesToWrite();
                socket->close(QWebSocketProtocol::CloseCodeGoingAway,
                              QStringLiteral("Client is not consuming Host responses."));
            } else {
                updateBackpressure(socket);
            }
        });
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
        connect(socket, &QWebSocket::bytesWritten, this,
                [this, socket](qint64) { updateBackpressure(socket); });
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
        if (parsed.message.type == BrickSuiteProtocol::MessageType::Event) {
            qWarning() << "Client-originated protocol event rejected.";
            socket->close(QWebSocketProtocol::CloseCodePolicyViolated,
                          QStringLiteral("Client-originated events are not permitted."));
            return;
        }
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
        session.protocolMinor = qMin(request.protocolMinor, BrickSuiteProtocol::Minor);
        send(socket, BrickSuiteProtocol::response(request, {
            {QStringLiteral("protocolMajor"), BrickSuiteProtocol::Major},
            {QStringLiteral("protocolMinor"), session.protocolMinor},
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
        const bool pairedProtocol = session.protocolMinor >= 3;
        const int expectedFields = pairedProtocol ? 4 : 3;
        if (request.payload.size() != expectedFields
            || !request.payload.value(QStringLiteral("sessionId")).isString()
            || !request.payload.value(QStringLiteral("clientNonce")).isString()
            || !request.payload.value(QStringLiteral("proof")).isString()
            || (pairedProtocol && !request.payload.value(QStringLiteral("deviceId")).isString())) {
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
        QString authenticationSecret = m_accessToken;
        std::optional<PairedDeviceRecord> pairedDevice;
        QString requestedDeviceId;
        if (pairedProtocol) {
            requestedDeviceId = request.payload.value(QStringLiteral("deviceId")).toString()
                                    .trimmed().toLower();
            pairedDevice = m_registry->find(requestedDeviceId);
            if (pairedDevice && pairedDevice->active) {
                const auto credential = CredentialStore::read(pairedDevice->credentialReference);
                if (credential.success && credential.found) authenticationSecret = credential.value;
                else authenticationSecret.clear();
            } else {
                authenticationSecret.clear();
            }
        }
        const QString peer = socket->peerAddress().toString();
        const QString throttleKey = pairedProtocol && pairedDevice
            ? QStringLiteral("device:") + pairedDevice->deviceId.toLower()
            : (pairedProtocol ? QStringLiteral("unknown-peer:") : QStringLiteral("legacy-peer:"))
                + peer;
        const qint64 nowMs = m_authenticationClock.elapsed();
        const auto throttle = m_authenticationThrottle.check(throttleKey, nowMs);
        if (throttle.expiredLockout)
            qInfo() << "BrickSuite authentication throttle expired.";
        if (!throttle.allowed) {
            session.challengeConsumed = true;
            reject(socket, request, QStringLiteral("AUTH_THROTTLED"),
                   QStringLiteral("Authentication is temporarily restricted after repeated failures. Try again shortly."),
                   true);
            socket->close(QWebSocketProtocol::CloseCodePolicyViolated,
                          QStringLiteral("Authentication temporarily throttled."));
            return;
        }
        const QByteArray expected = challengeUsable && !authenticationSecret.isEmpty()
            ? BrickSuiteAuthentication::hmacSha256(
                  authenticationSecret.toUtf8(), BrickSuiteAuthentication::authenticationInput(
                      session.challenge, clientNonce, session.id,
                      BrickSuiteProtocol::Major,
                      qMin(request.protocolMinor, BrickSuiteProtocol::Minor)), &hmacError)
            : QByteArray();
        if (!challengeUsable || expected.isEmpty()
            || !BrickSuiteAuthentication::constantTimeEquals(expected, proof)) {
            ++session.authenticationFailures;
            session.nextAuthenticationAllowedMs = QDateTime::currentMSecsSinceEpoch()
                + 1000 * session.authenticationFailures;
            const bool lockoutActivated = m_authenticationThrottle.recordFailure(throttleKey, nowMs);
            if (lockoutActivated)
                qWarning() << "BrickSuite authentication temporarily throttled after repeated failures.";
            else
                qDebug() << "BrickSuite Server authentication failed.";
            reject(socket, request, QStringLiteral("AUTH_FAILED"),
                   QStringLiteral("BrickSuite authentication failed."));
            if (session.authenticationFailures >= BrickSuiteProtocol::MaximumAuthenticationFailures)
                socket->close(QWebSocketProtocol::CloseCodePolicyViolated,
                              QStringLiteral("Authentication failed."));
            return;
        }
        if (m_authenticationThrottle.recordSuccess(throttleKey))
            qInfo() << "BrickSuite authentication succeeded after prior failures; throttle state reset.";
        session.authenticated = true;
        session.legacySharedToken = !pairedProtocol;
        if (pairedProtocol) {
            session.deviceId = pairedDevice->deviceId;
            QString lastSeenError;
            if (!m_registry->updateLastSeen(session.deviceId, QDateTime::currentDateTimeUtc(),
                                            &lastSeenError))
                qWarning().noquote() << "Unable to update paired-device last seen:" << lastSeenError;
        }
        session.authenticationTimer->stop();
        send(socket, BrickSuiteProtocol::response(request,
            {{QStringLiteral("authenticated"), true},
             {QStringLiteral("role"), QStringLiteral("FullBrickSuiteClient")},
             {QStringLiteral("authenticationKind"), pairedProtocol
                  ? QStringLiteral("PairedDevice") : QStringLiteral("LegacySharedToken")},
             {QStringLiteral("deviceId"), session.deviceId}}));
        qInfo() << "BrickSuite Server client authenticated in"
                << (QDateTime::currentMSecsSinceEpoch() - session.connectedMs)
                << "ms; authenticated clients:" << authenticatedClientCount();
        emit statusChanged();
        return;
    }
    if (request.operation == QStringLiteral("system.pair")) {
        if (session.protocolMinor < 3) {
            reject(socket, request, QStringLiteral("FORBIDDEN"),
                   QStringLiteral("Pairing requires BrickSuite Protocol 1.3."));
            return;
        }
        if (session.authenticated || request.payload.size() != 4
            || !request.payload.value(QStringLiteral("code")).isString()
            || !request.payload.value(QStringLiteral("friendlyName")).isString()
            || !request.payload.value(QStringLiteral("clientVersion")).isString()
            || !request.payload.value(QStringLiteral("platform")).isString()) {
            reject(socket, request, QStringLiteral("INVALID_REQUEST"),
                   QStringLiteral("The pairing request is invalid."));
            return;
        }
        const auto paired = m_pairing->pair(
            request.payload.value(QStringLiteral("code")).toString(),
            request.payload.value(QStringLiteral("friendlyName")).toString(),
            request.payload.value(QStringLiteral("clientVersion")).toString(),
            request.payload.value(QStringLiteral("platform")).toString());
        if (!paired.success) {
            reject(socket, request, paired.errorCode, paired.error,
                   paired.errorCode == QStringLiteral("PAIRING_RATE_LIMITED"));
            return;
        }
        send(socket, BrickSuiteProtocol::response(request, {
            {QStringLiteral("deviceId"), paired.deviceId},
            {QStringLiteral("credential"), paired.credential}}));
        QTimer::singleShot(0, socket, [socket] {
            socket->close(QWebSocketProtocol::CloseCodeNormal,
                          QStringLiteral("Pairing completed; reconnect to authenticate."));
        });
        return;
    }
    if (request.protocolMinor > session.protocolMinor) {
        reject(socket, request, QStringLiteral("FORBIDDEN"),
               QStringLiteral("The request exceeds the negotiated protocol version."));
        return;
    }
    const bool maintenanceSafe = request.operation == QStringLiteral("system.capabilities")
        || request.operation == QStringLiteral("system.ping")
        || request.operation == QStringLiteral("system.status");
    if (!m_operationalAdmissionOpen && !maintenanceSafe) {
        reject(socket, request, QStringLiteral("HOST_MAINTENANCE"),
               QStringLiteral("BrickSuite Host is temporarily in maintenance. Try again after it returns."),
               true);
        return;
    }
    std::shared_ptr<HostRequestAdmissionController::Lease> admissionLease;
    std::optional<HostRequestContext> requestContext;
    const auto admissionKind = m_dispatcher.admissionKind(request.operation);
    if (session.authenticated && admissionKind != BrickSuiteOperationDispatcher::AdmissionKind::None) {
        HostRequestContext context;
        context.sessionId = QString::fromLatin1(session.id.toBase64());
        context.requestId = request.requestId;
        context.protocolMinor = session.protocolMinor;
        context.authenticationKind = session.legacySharedToken
            ? HostRequestContext::AuthenticationKind::LegacySharedToken
            : HostRequestContext::AuthenticationKind::PairedDevice;
        context.pairedDeviceId = session.deviceId;
        const auto kind = admissionKind == BrickSuiteOperationDispatcher::AdmissionKind::Write
            ? HostRequestAdmissionController::WorkKind::Write
            : HostRequestAdmissionController::WorkKind::Read;
        auto admitted = m_admission.admit(context, kind);
        if (!admitted.accepted()) {
            if (admitted.rejection == HostRequestAdmissionController::Rejection::DuplicateRequestId)
                reject(socket, request, QStringLiteral("DUPLICATE_REQUEST"),
                       QStringLiteral("This request ID is already in flight for this session."));
            else
                reject(socket, request, QStringLiteral("HOST_BUSY"),
                       admitted.rejection == HostRequestAdmissionController::Rejection::OwnerLimit
                           ? QStringLiteral("This client has too many outstanding Host operations. Try again shortly.")
                           : QStringLiteral("The Host operation queue is full. Try again shortly."),
                       true);
            return;
        }
        admissionLease = std::move(admitted.lease);
        requestContext = context;
    }
    const QByteArray sessionId = session.id;
    QPointer<QWebSocket> guard(socket);
    auto completion =
        [this, guard, sessionId, operation = request.operation,
         admissionLease = std::move(admissionLease)](BrickSuiteProtocol::Message response) mutable {
            admissionLease.reset();
            if (!guard) return;
            auto it = m_sessions.find(guard.data());
            if (it == m_sessions.constEnd() || !it->authenticated || it->id != sessionId)
                return;
            if ((operation == QStringLiteral("system.capabilities")
                 || operation == QStringLiteral("system.status"))
                && response.type == BrickSuiteProtocol::MessageType::Response) {
                response.payload.insert(QStringLiteral("maintenance"),
                                        !m_operationalAdmissionOpen);
            }
            if (operation == QStringLiteral("system.capabilities")
                && response.type == BrickSuiteProtocol::MessageType::Response) {
                QJsonArray operations;
                for (const QString& name : m_dispatcher.operations(it->protocolMinor))
                    operations.append(name);
                QJsonArray capabilities = response.payload.value(
                    QStringLiteral("capabilities")).toArray();
                for (const QString& name : m_dispatcher.capabilities(it->protocolMinor))
                    if (!capabilities.contains(name)) capabilities.append(name);
                response.payload.insert(QStringLiteral("operations"), operations);
                response.payload.insert(QStringLiteral("capabilities"), capabilities);
                response.payload.insert(QStringLiteral("maintenance"),
                                        !m_operationalAdmissionOpen);
                it->invalidationsReady = true;
            }
            qDebug() << "BrickSuite Host sending response" << operation
                     << response.requestId.left(8) << BrickSuiteProtocol::typeName(response.type);
            send(guard.data(), response);
        };
    if (requestContext)
        m_dispatcher.dispatchAsync(request, session.authenticated, *requestContext,
                                   std::move(completion));
    else
        m_dispatcher.dispatchAsync(request, session.authenticated, std::move(completion));
}

bool BrickSuiteWebSocketServer::send(QWebSocket* socket,
                                     const BrickSuiteProtocol::Message& message)
{
    if (!socket || !m_sessions.contains(socket)) return false;
    bool replacedOversizedResponse = false;
    bool rejectedOversizedEvent = false;
    const QByteArray serialized = HostOutboundPolicy::serializeForSend(
        message, &replacedOversizedResponse, &rejectedOversizedEvent);
    if (rejectedOversizedEvent) {
        ++m_oversizedEventCount;
        if (m_oversizedEventCount == 1 || (m_oversizedEventCount & (m_oversizedEventCount - 1)) == 0)
            qWarning() << "BrickSuite Host rejected oversized outbound event; count"
                       << m_oversizedEventCount;
        return false;
    }
    if (replacedOversizedResponse) {
        ++m_oversizedResponseCount;
        if (m_oversizedResponseCount == 1
            || (m_oversizedResponseCount & (m_oversizedResponseCount - 1)) == 0)
            qWarning() << "BrickSuite Host replaced oversized response with RESULT_TOO_LARGE; count"
                       << m_oversizedResponseCount;
    }
    if (serialized.isEmpty()) return false;
    const qint64 buffered = socket->bytesToWrite();
    auto it = m_sessions.find(socket);
    if (it == m_sessions.end()) return false;
    it->maximumBufferedBytes = qMax(it->maximumBufferedBytes, buffered);
    const auto decision = HostOutboundPolicy::evaluate(buffered, serialized.size());
    if (decision == HostOutboundPolicy::Decision::Disconnect) {
        ++m_slowClientDisconnectCount;
        qWarning() << "BrickSuite Host disconnected a slow client at the outbound hard limit; buffered bytes"
                   << buffered;
        socket->close(QWebSocketProtocol::CloseCodeGoingAway,
                      QStringLiteral("Client outbound buffer limit reached."));
        return false;
    }
    if (decision == HostOutboundPolicy::Decision::EnterBackpressure
        && !it->underBackpressure) {
        it->underBackpressure = true;
        qWarning() << "BrickSuite Host client entered outbound backpressure; buffered bytes"
                   << buffered;
        it->backpressureTimer->start(HostOutboundPolicy::SustainedBackpressureMs);
    }
    const qint64 queuedBytes = socket->sendTextMessage(QString::fromUtf8(serialized));
    updateBackpressure(socket);
    if (message.operation.startsWith(QStringLiteral("inventory.")))
        qDebug() << "BrickSuite Host WebSocket sendTextMessage completed"
                 << message.operation << message.requestId.left(8)
                 << "queuedBytes" << queuedBytes
                 << "socketThreadCurrent" << (socket->thread() == QThread::currentThread());
    return queuedBytes >= 0;
}

void BrickSuiteWebSocketServer::updateBackpressure(QWebSocket* socket)
{
    auto it = m_sessions.find(socket);
    if (it == m_sessions.end()) return;
    const qint64 buffered = socket->bytesToWrite();
    it->maximumBufferedBytes = qMax(it->maximumBufferedBytes, buffered);
    if (buffered > HostOutboundPolicy::HardLimitBytes) {
        ++m_slowClientDisconnectCount;
        qWarning() << "BrickSuite Host disconnected a slow client after its outbound buffer exceeded the hard limit; buffered bytes"
                   << buffered;
        socket->close(QWebSocketProtocol::CloseCodeGoingAway,
                      QStringLiteral("Client outbound buffer limit reached."));
        return;
    }
    if (!it->underBackpressure && buffered >= HostOutboundPolicy::HighWaterBytes) {
        it->underBackpressure = true;
        qWarning() << "BrickSuite Host client entered outbound backpressure; buffered bytes"
                   << buffered;
        it->backpressureTimer->start(HostOutboundPolicy::SustainedBackpressureMs);
        return;
    }
    if (it->underBackpressure && buffered < HostOutboundPolicy::HighWaterBytes) {
        it->underBackpressure = false;
        it->backpressureTimer->stop();
        qInfo() << "BrickSuite Host client recovered from outbound backpressure; buffered bytes"
                << buffered;
    }
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
    const auto it = m_sessions.find(socket);
    const QString sessionId = it == m_sessions.end()
        ? QString() : QString::fromLatin1(it->id.toBase64());
    if (it != m_sessions.end()) m_sessions.erase(it);
    if (!sessionId.isEmpty()) emit sessionDisconnected(sessionId);
    socket->deleteLater();
    qInfo() << "BrickSuite Server client disconnected.";
    emit statusChanged();
}
