#pragma once

#include <QObject>
#include <QTimer>

class BrickSuiteWebSocketServer;
class HostReadExecutor;
class HostWriteExecutor;

// Application-owned, runtime-only maintenance boundary. It closes admission
// before waiting asynchronously for already-admitted serialized work to drain.
class HostMaintenanceCoordinator : public QObject
{
    Q_OBJECT
public:
    enum class State { Normal, EnteringMaintenance, Maintenance,
                       LeavingMaintenance, ShuttingDown };
    Q_ENUM(State)

    static constexpr int DefaultDrainTimeoutMs = 30000;

    HostMaintenanceCoordinator(BrickSuiteWebSocketServer& server,
                               HostReadExecutor& reads,
                               HostWriteExecutor& writes,
                               QObject* parent = nullptr);

    State state() const { return m_state; }
    QString stateText() const;
    int queuedReads() const;
    int activeReads() const;
    int queuedWrites() const;
    int activeWrites() const;
    bool localOperationalWritesAllowed() const { return m_state == State::Normal; }

    bool requestEnterMaintenance(int timeoutMs = DefaultDrainTimeoutMs);
    bool leaveMaintenance();
    void beginShutdown();

signals:
    void stateChanged(HostMaintenanceCoordinator::State state);
    void countersChanged();
    void maintenanceEntryFailed(const QString& message);

private:
    void setState(State state);
    void evaluateDrain();
    void reopenAfterFailedEntry(const QString& message);

    BrickSuiteWebSocketServer& m_server;
    HostReadExecutor& m_reads;
    HostWriteExecutor& m_writes;
    State m_state = State::Normal;
    QTimer m_timeout;
};
