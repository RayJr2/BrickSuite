#pragma once

#include "DatabaseRestoreTransaction.h"

#include <QObject>

class AutomaticBackupService;
class BrickSuiteNetworkManager;
class QThread;

class DatabaseRestoreCoordinator : public QObject
{
    Q_OBJECT
public:
    DatabaseRestoreCoordinator(BrickSuiteNetworkManager& network,
                               AutomaticBackupService& automaticBackups,
                               QObject* parent = nullptr);
    ~DatabaseRestoreCoordinator() override;

    bool isRunning() const { return m_running; }
    void start(const QString& candidatePath);

signals:
    void phaseChanged(const QString& message);
    void localQuiesceRequired();
    void finished(const DatabaseRestoreTransaction::Result& result);

private:
    void runPreflight();
    void waitForAutomaticBackup();
    void enterMaintenance();
    void runReplacement();

    BrickSuiteNetworkManager& m_network;
    AutomaticBackupService& m_automaticBackups;
    QString m_candidatePath;
    QString m_stagedCandidatePath;
    bool m_running = false;
    bool m_destructiveStarted = false;
    QThread* m_worker = nullptr;
};

Q_DECLARE_METATYPE(DatabaseRestoreTransaction::Result)
