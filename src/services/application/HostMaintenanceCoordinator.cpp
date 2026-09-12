#include "HostMaintenanceCoordinator.h"

#include "HostReadExecutor.h"
#include "HostWriteExecutor.h"
#include "HostOperationalGate.h"
#include "../../network/BrickSuiteWebSocketServer.h"

#include <QDebug>

HostMaintenanceCoordinator::HostMaintenanceCoordinator(
    BrickSuiteWebSocketServer& server, HostReadExecutor& reads,
    HostWriteExecutor& writes, QObject* parent)
    : QObject(parent), m_server(server), m_reads(reads), m_writes(writes)
{
    m_timeout.setSingleShot(true);
    connect(&m_timeout, &QTimer::timeout, this, [this] {
        if (m_state == State::EnteringMaintenance)
            reopenAfterFailedEntry(QStringLiteral(
                "Maintenance could not start because Host operations did not finish in time."));
    });
    const auto activity = [this] { emit countersChanged(); evaluateDrain(); };
    connect(&m_reads, &HostReadExecutor::activityChanged, this, activity);
    connect(&m_writes, &HostWriteExecutor::activityChanged, this, activity);
}

QString HostMaintenanceCoordinator::stateText() const
{
    switch (m_state) {
    case State::Normal: return QStringLiteral("Normal");
    case State::EnteringMaintenance: return QStringLiteral("Entering Maintenance");
    case State::Maintenance: return QStringLiteral("Maintenance");
    case State::LeavingMaintenance: return QStringLiteral("Leaving Maintenance");
    case State::ShuttingDown: return QStringLiteral("Shutting Down");
    }
    return {};
}

int HostMaintenanceCoordinator::queuedReads() const { return m_reads.queuedReadCount(); }
int HostMaintenanceCoordinator::activeReads() const { return m_reads.activeReadCount(); }
int HostMaintenanceCoordinator::queuedWrites() const { return m_writes.queuedMutationCount(); }
int HostMaintenanceCoordinator::activeWrites() const { return m_writes.activeMutationCount(); }

bool HostMaintenanceCoordinator::requestEnterMaintenance(int timeoutMs)
{
    if (m_state != State::Normal || timeoutMs <= 0) return false;
    setState(State::EnteringMaintenance);
    HostOperationalGate::setLocalWritesAllowed(false);
    m_server.setOperationalAdmissionOpen(false);
    m_reads.stopAccepting();
    m_writes.stopAccepting();
    m_timeout.start(timeoutMs);
    evaluateDrain();
    return true;
}

void HostMaintenanceCoordinator::evaluateDrain()
{
    if (m_state != State::EnteringMaintenance || !m_reads.isIdle() || !m_writes.isIdle())
        return;
    m_timeout.stop();
    setState(State::Maintenance);
    qInfo() << "BrickSuite Host entered maintenance.";
}

void HostMaintenanceCoordinator::reopenAfterFailedEntry(const QString& message)
{
    m_reads.startAccepting();
    m_writes.startAccepting();
    m_server.setOperationalAdmissionOpen(true);
    HostOperationalGate::setLocalWritesAllowed(true);
    setState(State::Normal);
    qInfo().noquote() << "Host maintenance entry failed due to drain timeout:" << message;
    emit maintenanceEntryFailed(message);
}

bool HostMaintenanceCoordinator::leaveMaintenance()
{
    if (m_state != State::Maintenance) return false;
    setState(State::LeavingMaintenance);
    m_reads.startAccepting();
    m_writes.startAccepting();
    m_server.setOperationalAdmissionOpen(true);
    HostOperationalGate::setLocalWritesAllowed(true);
    setState(State::Normal);
    qInfo() << "BrickSuite Host returned to normal operation.";
    m_server.broadcastFullOperationalInvalidation();
    return true;
}

void HostMaintenanceCoordinator::beginShutdown()
{
    if (m_state == State::ShuttingDown) return;
    m_timeout.stop();
    m_server.setOperationalAdmissionOpen(false);
    HostOperationalGate::setLocalWritesAllowed(false);
    m_reads.stopAccepting();
    m_writes.stopAccepting();
    setState(State::ShuttingDown);
}

void HostMaintenanceCoordinator::setState(State state)
{
    if (m_state == state) return;
    m_state = state;
    emit stateChanged(state);
    emit countersChanged();
}
