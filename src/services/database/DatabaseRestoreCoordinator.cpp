#include "DatabaseRestoreCoordinator.h"

#include "AutomaticBackupService.h"
#include "../../database/DatabaseManager.h"
#include "../../network/BrickSuiteNetworkManager.h"
#include "../../network/HostDataEpoch.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QPointer>
#include <QThread>
#include <QUuid>

DatabaseRestoreCoordinator::DatabaseRestoreCoordinator(
    BrickSuiteNetworkManager& network, AutomaticBackupService& automaticBackups, QObject* parent)
    : QObject(parent), m_network(network), m_automaticBackups(automaticBackups)
{
    qRegisterMetaType<DatabaseRestoreTransaction::Result>();
}

DatabaseRestoreCoordinator::~DatabaseRestoreCoordinator()
{
    if (m_worker) {
        m_worker->wait();
        delete m_worker;
    }
}

void DatabaseRestoreCoordinator::start(const QString& candidatePath)
{
    if (m_running) return;
    m_running = true;
    m_destructiveStarted = false;
    m_candidatePath = candidatePath;
    runPreflight();
}

void DatabaseRestoreCoordinator::runPreflight()
{
    emit phaseChanged(QStringLiteral("Validating and staging the selected Restore database..."));
    const QString livePath = DatabaseManager::instance().databasePath();
    m_stagedCandidatePath = livePath + QStringLiteral(".restore_preflight_")
        + QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString source = m_candidatePath;
    const QString staged = m_stagedCandidatePath;
    auto result = std::make_shared<DatabaseFileValidator::Result>();
    m_worker = QThread::create([result, source, staged] {
        *result = DatabaseFileValidator::validate(source);
        if (result->valid && (!QFile::copy(source, staged)
            || !DatabaseFileValidator::validate(staged).valid)) {
            result->valid = false;
            result->message = QStringLiteral(
                "The selected database could not be staged and verified in the BrickSuite data folder.");
            QFile::remove(staged);
        }
    });
    connect(m_worker, &QThread::finished, this, [this, result] {
        QThread* completed = m_worker;
        m_worker = nullptr;
        completed->deleteLater();
        if (!result->valid) {
            DatabaseRestoreTransaction::Result failure;
            failure.failure = DatabaseRestoreTransaction::Failure::Candidate;
            failure.error = result->message;
            m_running = false;
            emit finished(failure);
            return;
        }
        m_candidatePath = m_stagedCandidatePath;
        emit phaseChanged(QStringLiteral("Entering Host Maintenance and waiting for database work..."));
        m_automaticBackups.stop();
        waitForAutomaticBackup();
    });
    m_worker->start();
}

void DatabaseRestoreCoordinator::waitForAutomaticBackup()
{
    if (!m_automaticBackups.isRunning()) {
        enterMaintenance();
        return;
    }
    auto connection = std::make_shared<QMetaObject::Connection>();
    *connection = connect(&m_automaticBackups, &AutomaticBackupService::stateChanged, this,
                          [this, connection] {
        if (m_automaticBackups.isRunning()) return;
        disconnect(*connection);
        enterMaintenance();
    });
}

void DatabaseRestoreCoordinator::enterMaintenance()
{
    m_network.quiesceForDatabaseRestore([this](bool success, const QString& error) {
        if (!success) {
            DatabaseRestoreTransaction::Result result;
            result.failure = DatabaseRestoreTransaction::Failure::Replacement;
            result.error = error;
            QFile::remove(m_stagedCandidatePath);
            m_running = false;
            emit finished(result);
            return;
        }
        m_destructiveStarted = true;
        emit phaseChanged(QStringLiteral("Stopping local database services..."));
        emit localQuiesceRequired();
        DatabaseManager::instance().close();
        runReplacement();
    });
}

void DatabaseRestoreCoordinator::runReplacement()
{
    emit phaseChanged(QStringLiteral("Creating and verifying the pre-Restore safety backup..."));
    const QString livePath = DatabaseManager::instance().databasePath();
    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd_HHmmss"));
    const QString safetyPath = QFileInfo(livePath).dir().filePath(
        QStringLiteral("BrickSuite_PreRestore_%1_%2.db")
            .arg(stamp, QUuid::createUuid().toString(QUuid::WithoutBraces).left(8)));
    auto result = std::make_shared<DatabaseRestoreTransaction::Result>();
    const QString candidate = m_candidatePath;
    const QString stateDirectory = HostDataEpoch::storageDirectory();
    QPointer<DatabaseRestoreCoordinator> guard(this);
    m_worker = QThread::create([result, candidate, livePath, safetyPath, stateDirectory, guard] {
        *result = DatabaseRestoreTransaction::execute(candidate, livePath, safetyPath,
            stateDirectory, stateDirectory, nullptr, [guard](const QString& phase) {
                if (!guard) return;
                QMetaObject::invokeMethod(guard, [guard, phase] {
                    if (guard) emit guard->phaseChanged(phase);
                }, Qt::QueuedConnection);
            }, true);
    });
    connect(m_worker, &QThread::finished, this, [this, result] {
        QThread* completed = m_worker;
        m_worker = nullptr;
        completed->deleteLater();
        QFile::remove(m_stagedCandidatePath);
        if (m_destructiveStarted) result->restartRequired = true;
        m_running = false;
        emit phaseChanged(result->success
            ? QStringLiteral("Restore finalized; restart required.")
            : QStringLiteral("Restore did not complete."));
        emit finished(*result);
    });
    m_worker->start();
}
