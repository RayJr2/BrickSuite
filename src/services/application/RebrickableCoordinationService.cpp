#include "RebrickableCoordinationService.h"

#include "../../api/rebrickable/RebrickableService.h"
#include "../../network/BrickSuiteOperationDispatcher.h"
#include "../../network/BrickSuiteProtocol.h"
#include "../../network/BrickSuiteWebSocketClient.h"
#include "../../network/BrickSuiteWebSocketServer.h"
#include "../../network/HostRequestContext.h"

#include <QJsonObject>
#include <QDebug>
#include <QtMath>

const QString RebrickableCoordinationService::Capability = QStringLiteral("apiCoordination.rebrickable");
const QString RebrickableCoordinationService::RegisterOperation = QStringLiteral("apiCoordination.rebrickable.register");
const QString RebrickableCoordinationService::StatusOperation = QStringLiteral("apiCoordination.rebrickable.status");
const QString RebrickableCoordinationService::ChangedEvent = QStringLiteral("apiCoordination.rebrickable.changed");

RebrickableCoordinationService::RebrickableCoordinationService(
    BrickSuiteWebSocketServer& server, BrickSuiteWebSocketClient& client, QObject* parent)
    : QObject(parent), m_server(server), m_client(client)
{
    qRegisterMetaType<State>();
    m_fallbackTimer.setSingleShot(true);
    m_fallbackTimer.setInterval(FallbackGraceMs);
    connect(&m_fallbackTimer, &QTimer::timeout, this, [this] {
        if (!m_remoteActive) return;
        m_state = {};
        RebrickableService::setCoordinationMinimumIntervalMs(0);
        emit stateChanged(m_state);
        qInfo() << "Rebrickable coordination unavailable; using the local request interval.";
    });
    connect(&m_client, &BrickSuiteWebSocketClient::authenticatedSessionEstablished,
            this, [this](const QString&) {
        if (!m_remoteActive || !m_client.supportsCapability(Capability)) return;
        // A restarted Host begins a fresh transient revision sequence. Retain
        // the safer interval while allowing its first state to replace ours.
        m_state.revision = 0;
        const QJsonObject request{{QStringLiteral("provider"), QStringLiteral("Rebrickable")},
                                  {QStringLiteral("participating"), m_remoteParticipating}};
        m_client.sendRequest(RegisterOperation, request, this,
            [this](const QJsonObject& payload) {
                if (m_remoteParticipating) applyRemoteState(payload);
                else { m_state = {}; RebrickableService::setCoordinationMinimumIntervalMs(0); emit stateChanged(m_state); }
            });
    });
    connect(&m_client, &BrickSuiteWebSocketClient::authenticatedSessionLost,
            this, &RebrickableCoordinationService::beginFallback);
    connect(&m_client, &BrickSuiteWebSocketClient::rebrickableCoordinationReceived,
            this, &RebrickableCoordinationService::applyRemoteState);
    registerOperations();
}

void RebrickableCoordinationService::configureHost(bool active, bool participating)
{
    m_hostActive = active;
    m_hostParticipating = active && participating;
    if (active) recalculateHost();
}

void RebrickableCoordinationService::configureRemote(bool active, bool participating)
{
    m_remoteActive = active;
    m_remoteParticipating = active && participating;
    if (!active) { m_fallbackTimer.stop(); m_state = {}; RebrickableService::setCoordinationMinimumIntervalMs(0); }
    if (active && m_client.supportsCapability(Capability)) {
        m_client.sendRequest(RegisterOperation,
            {{QStringLiteral("provider"), QStringLiteral("Rebrickable")},
             {QStringLiteral("participating"), m_remoteParticipating}}, this,
            [this](const QJsonObject& payload) {
                if (m_remoteParticipating) applyRemoteState(payload);
                else { m_state = {}; RebrickableService::setCoordinationMinimumIntervalMs(0); emit stateChanged(m_state); }
            });
    }
}

void RebrickableCoordinationService::registerOperations()
{
    auto& dispatcher = m_server.operationDispatcher();
    dispatcher.registerAsyncOperation(RegisterOperation, true,
        [this](const BrickSuiteProtocol::Message& request,
               BrickSuiteOperationDispatcher::Completion completion) {
            const HostRequestContext* context = HostRequestContext::current();
            if (!context || context->authenticationKind != HostRequestContext::AuthenticationKind::PairedDevice
                || context->pairedDeviceId.isEmpty()
                || request.payload.size() != 2
                || request.payload.value(QStringLiteral("provider")).toString() != QStringLiteral("Rebrickable")
                || !request.payload.value(QStringLiteral("participating")).isBool()) {
                completion(BrickSuiteProtocol::errorResponse(request, QStringLiteral("INVALID_REQUEST"),
                    QStringLiteral("The Rebrickable coordination registration is invalid.")));
                return;
            }
            registerParticipant(context->sessionId, context->pairedDeviceId,
                                request.payload.value(QStringLiteral("participating")).toBool());
            completion(BrickSuiteProtocol::response(request, statePayload()));
        }, 4, Capability);
    dispatcher.registerAsyncOperation(StatusOperation, true,
        [this](const BrickSuiteProtocol::Message& request,
               BrickSuiteOperationDispatcher::Completion completion) {
            if (!request.payload.isEmpty()) {
                completion(BrickSuiteProtocol::errorResponse(request, QStringLiteral("INVALID_REQUEST"),
                    QStringLiteral("The Rebrickable coordination status request must be empty.")));
                return;
            }
            completion(BrickSuiteProtocol::response(request, statePayload()));
        }, 4, Capability);
}

void RebrickableCoordinationService::registerParticipant(
    const QString& sessionId, const QString& rawDeviceId, bool participating)
{
    const QString deviceId = rawDeviceId.trimmed().toLower();
    if (auto* timer = m_removalTimers.take(deviceId)) { timer->stop(); timer->deleteLater(); }
    removeSession(sessionId);
    if (participating) {
        m_sessionDevices.insert(sessionId, deviceId);
        m_deviceSessions[deviceId].insert(sessionId);
    }
    recalculateHost();
}

void RebrickableCoordinationService::sessionDisconnected(const QString& sessionId)
{
    const QString deviceId = m_sessionDevices.take(sessionId);
    if (deviceId.isEmpty()) return;
    m_deviceSessions[deviceId].remove(sessionId);
    if (!m_deviceSessions.value(deviceId).isEmpty()) return;
    auto* timer = new QTimer(this);
    timer->setSingleShot(true); timer->setInterval(RemovalGraceMs);
    m_removalTimers.insert(deviceId, timer);
    connect(timer, &QTimer::timeout, this, [this, deviceId] {
        m_removalTimers.remove(deviceId);
        m_deviceSessions.remove(deviceId);
        recalculateHost();
    });
    timer->start();
}

void RebrickableCoordinationService::removeSession(const QString& sessionId)
{
    const QString old = m_sessionDevices.take(sessionId);
    if (!old.isEmpty()) m_deviceSessions[old].remove(sessionId);
}

void RebrickableCoordinationService::recalculateHost()
{
    if (!m_hostActive) return;
    int devices = 0;
    for (auto it = m_deviceSessions.cbegin(); it != m_deviceSessions.cend(); ++it)
        if (!it.value().isEmpty() || m_removalTimers.contains(it.key())) ++devices;
    const int count = devices + (m_hostParticipating ? 1 : 0);
    const int interval = coordinatedIntervalForParticipants(count);
    if (m_state.coordinated && m_state.participantCount == count
        && m_state.coordinatedIntervalMs == interval) return;
    m_state.coordinated = count > 0;
    m_state.participantCount = count;
    m_state.coordinatedIntervalMs = interval;
    ++m_state.revision;
    RebrickableService::setCoordinationMinimumIntervalMs(interval);
    emit stateChanged(m_state);
    publishHostState();
    qInfo() << "Rebrickable coordination updated; participants" << count
            << "minimum interval" << interval << "ms.";
}

QJsonObject RebrickableCoordinationService::statePayload() const
{
    return {{QStringLiteral("provider"), QStringLiteral("Rebrickable")},
            {QStringLiteral("participantCount"), m_state.participantCount},
            {QStringLiteral("coordinatedIntervalMs"), m_state.coordinatedIntervalMs},
            {QStringLiteral("revision"), double(m_state.revision)}};
}

bool RebrickableCoordinationService::decodeState(const QJsonObject& p, State* state)
{
    if (!state || p.size() != 4 || p.value(QStringLiteral("provider")).toString() != QStringLiteral("Rebrickable")
        || !p.value(QStringLiteral("participantCount")).isDouble()
        || !p.value(QStringLiteral("coordinatedIntervalMs")).isDouble()
        || !p.value(QStringLiteral("revision")).isDouble()) return false;
    const int count = p.value(QStringLiteral("participantCount")).toInt(-1);
    const int interval = p.value(QStringLiteral("coordinatedIntervalMs")).toInt(-1);
    const double revision = p.value(QStringLiteral("revision")).toDouble(-1);
    if (count < 0 || interval < 0 || revision < 0 || revision != qFloor(revision)) return false;
    *state = {true, count, interval, quint64(revision)};
    return true;
}

void RebrickableCoordinationService::applyRemoteState(const QJsonObject& payload)
{
    State next;
    if (!m_remoteActive || !decodeState(payload, &next)) return;
    if (m_state.coordinated && next.revision <= m_state.revision) return;
    m_fallbackTimer.stop();
    m_state = next;
    RebrickableService::setCoordinationMinimumIntervalMs(next.coordinatedIntervalMs);
    emit stateChanged(m_state);
    qInfo() << "Rebrickable Host coordination active; participants" << next.participantCount
            << "minimum interval" << next.coordinatedIntervalMs << "ms.";
}

void RebrickableCoordinationService::publishHostState()
{
    if (!m_hostActive) return;
    m_server.broadcastEventToDevices(ChangedEvent, statePayload(), m_deviceSessions.keys(), 4);
}

void RebrickableCoordinationService::beginFallback()
{
    if (!m_remoteActive || !m_state.coordinated) return;
    if (!m_fallbackTimer.isActive()) {
        m_fallbackTimer.start();
        qInfo() << "Rebrickable Host coordination lost; retaining the safer interval briefly.";
    }
}

#ifdef BRICKSUITE_TESTING
void RebrickableCoordinationService::registerParticipantForTesting(const QString& s,const QString& d,bool p){registerParticipant(s,d,p);}
void RebrickableCoordinationService::expireRemovalGraceForTesting(const QString& d){if(auto*t=m_removalTimers.value(d)){t->stop();m_removalTimers.remove(d);m_deviceSessions.remove(d);t->deleteLater();recalculateHost();}}
void RebrickableCoordinationService::expireFallbackGraceForTesting(){m_fallbackTimer.stop();if(m_remoteActive){m_state={};RebrickableService::setCoordinationMinimumIntervalMs(0);emit stateChanged(m_state);}}
#endif
