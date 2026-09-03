#include "AutomaticBackupService.h"

#include "AutomaticBackupPolicy.h"
#include "DatabaseStatusService.h"
#include "../../database/DatabaseSchema.h"
#include "../../settings/UserSettings.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QPointer>
#include <QThread>
#include <QTimer>
#include <memory>

AutomaticBackupService::AutomaticBackupService(
    QObject* parent, const QString& sourceDatabasePath,
    const AutomaticBackupRetention::RemoveFile& removeFile,
    const HealthCheck& healthCheck)
    : QObject(parent)
    , m_timer(new QTimer(this))
    , m_sourceDatabasePath(sourceDatabasePath)
    , m_removeFile(removeFile)
    , m_healthCheck(healthCheck ? healthCheck : &AutomaticBackupService::validateSourceHealth)
{
    m_timer->setSingleShot(true);
    connect(m_timer, &QTimer::timeout, this, &AutomaticBackupService::checkDue);
}

AutomaticBackupService::~AutomaticBackupService()
{
    stop();
    if (m_worker) {
        m_worker->wait();
        delete m_worker;
        m_worker = nullptr;
    }
}

void AutomaticBackupService::start()
{
    if (m_started) return;
    m_started = true;
    QTimer::singleShot(0, this, [this]() {
        m_policyCheckPending = false;
        checkDue();
    });
}

void AutomaticBackupService::stop()
{
    m_started = false;
    m_timer->stop();
}

void AutomaticBackupService::reloadPolicy()
{
    m_policyCheckPending = true;
    m_timer->stop();
    if (m_started && !isRunning()) {
        m_policyCheckPending = false;
        checkDue();
    }
    emit stateChanged();
}

bool AutomaticBackupService::isRunning() const
{ return m_worker != nullptr; }

void AutomaticBackupService::checkDue()
{
    if (!m_started || isRunning()) return;
    UserSettings& settings = UserSettings::instance();
    const QDateTime now = QDateTime::currentDateTimeUtc();
    const int frequencyHours = settings.automaticBackupFrequencyHours();
    if (AutomaticBackupPolicy::isDue(settings.automaticBackupEnabled(),
                                        settings.automaticBackupRoot(),
                                        frequencyHours,
                                        settings.automaticBackupLastSuccessfulUtc(), now)
        && AutomaticBackupPolicy::retryEligible(frequencyHours,
                                                settings.automaticBackupLastAttemptUtc(), now)) {
        requestBackup(false);
        return;
    }
    scheduleNextCheck();
}

void AutomaticBackupService::scheduleNextCheck()
{
    if (!m_started || isRunning()) return;

    constexpr qint64 maximumPollMilliseconds = 60 * 60 * 1000;
    constexpr qint64 minimumPollMilliseconds = 1000;
    qint64 delayMilliseconds = maximumPollMilliseconds;

    UserSettings& settings = UserSettings::instance();
    if (settings.automaticBackupEnabled() && !settings.automaticBackupRoot().isEmpty()) {
        const QDateTime now = QDateTime::currentDateTimeUtc();
        const int frequencyHours = settings.automaticBackupFrequencyHours();
        QDateTime eligibleUtc = now;

        const QDateTime lastSuccessfulUtc = settings.automaticBackupLastSuccessfulUtc();
        if (lastSuccessfulUtc.isValid())
            eligibleUtc = lastSuccessfulUtc.addSecs(qint64(frequencyHours) * 60 * 60);

        const QDateTime lastAttemptUtc = settings.automaticBackupLastAttemptUtc();
        if (lastAttemptUtc.isValid()) {
            const int retryHours = qBound(1, frequencyHours,
                                          AutomaticBackupPolicy::MaximumRetryHours);
            const QDateTime retryUtc = lastAttemptUtc.addSecs(qint64(retryHours) * 60 * 60);
            if (retryUtc > eligibleUtc) eligibleUtc = retryUtc;
        }

        delayMilliseconds = qBound(minimumPollMilliseconds,
                                   now.msecsTo(eligibleUtc),
                                   maximumPollMilliseconds);
    }

    // Never wait more than an hour so clock changes and wake-from-sleep are noticed.
    m_timer->start(delayMilliseconds);
}

void AutomaticBackupService::requestBackup(bool bypassScheduledRetry)
{
    if (!m_started || isRunning()) return;
    UserSettings& settings = UserSettings::instance();
    const QString root = settings.automaticBackupRoot();
    if (root.isEmpty() || (!bypassScheduledRetry && !settings.automaticBackupEnabled())) {
        if (!bypassScheduledRetry) scheduleNextCheck();
        return;
    }
    if (!bypassScheduledRetry
        && !AutomaticBackupPolicy::retryEligible(settings.automaticBackupFrequencyHours(),
                                                  settings.automaticBackupLastAttemptUtc(),
                                                  QDateTime::currentDateTimeUtc())) {
        scheduleNextCheck();
        return;
    }
    startBackup(root, settings.automaticBackupRetentionCount(), bypassScheduledRetry);
}

void AutomaticBackupService::requestBackupNow(const QString& backupRoot, int retentionCount)
{
    if (!m_started || isRunning()) return;
    startBackup(backupRoot.trimmed(), retentionCount, true);
}

void AutomaticBackupService::startBackup(const QString& root, int retentionCount,
                                         bool bypassScheduledRetry)
{
    Q_UNUSED(bypassScheduledRetry)
    if (root.isEmpty() || isRunning()) return;
    UserSettings& settings = UserSettings::instance();
    const QDateTime attemptedUtc = QDateTime::currentDateTimeUtc();

    settings.setAutomaticBackupLastAttemptUtc(attemptedUtc);
    const QString directory = AutomaticBackupPolicy::versionDirectory(
        root, DatabaseSchema::CurrentSchemaVersion);
    const QString destination = AutomaticBackupPolicy::unusedBackupPath(
        directory, DatabaseSchema::CurrentSchemaVersion, attemptedUtc);
    const QString source = m_sourceDatabasePath.isEmpty()
                               ? DatabaseManager::instance().databasePath()
                               : m_sourceDatabasePath;
    auto result = std::make_shared<DatabaseManager::VerifiedBackupResult>();
    auto retention = std::make_shared<AutomaticBackupRetentionResult>();
    auto elapsed = std::make_shared<QElapsedTimer>();
    elapsed->start();

    qInfo() << "Automatic database backup started:" << destination;
    emit backupStarted(destination);
    m_timer->stop();
    m_worker = QThread::create([result, retention, source, destination,
                                directory, retentionCount, removeFile = m_removeFile,
                                healthCheck = m_healthCheck]() {
        const AutomaticBackupSourceHealthResult health = healthCheck(source);
        if (!health.healthy) {
            result->failure = health.failure;
            result->errorMessage = health.errorMessage;
            result->backupPath = destination;
            return;
        }

        QDir destinationDirectory(QFileInfo(destination).dir());
        if (!destinationDirectory.exists() && !destinationDirectory.mkpath(".")) {
            result->failure = DatabaseManager::BackupFailure::Destination;
            result->errorMessage = QString("Unable to create the configured backup directory: %1")
                                       .arg(destinationDirectory.absolutePath());
            result->backupPath = destination;
            return;
        }
        *result = DatabaseManager::createVerifiedBackup(source, destination);
        if (result->success) {
            *retention = AutomaticBackupRetention::clean(
                directory, DatabaseSchema::CurrentSchemaVersion, retentionCount,
                destination, removeFile);
        }
    });
    connect(m_worker, &QThread::finished, this,
            [this, result, retention, elapsed, attemptedUtc]() {
                QThread* completed = m_worker;
                m_worker = nullptr;
                completed->deleteLater();
                finishBackup(*result, *retention, attemptedUtc, elapsed->elapsed());
                if (m_policyCheckPending) {
                    m_policyCheckPending = false;
                    checkDue();
                } else {
                    scheduleNextCheck();
                }
            });
    m_worker->start();
}

AutomaticBackupSourceHealthResult AutomaticBackupService::validateSourceHealth(
    const QString& databasePath)
{
    // DatabaseStatusService owns a fresh read-only connection for each full result set.
    // This method runs on the automatic-backup worker, never on the GUI connection.
    const DatabaseIntegrityCheckResult integrity =
        DatabaseStatusService::runIntegrityCheck(databasePath);
    const DatabaseForeignKeyCheckResult foreignKeys =
        DatabaseStatusService::runForeignKeyCheck(databasePath);

    AutomaticBackupSourceHealthResult result;
    if (integrity.outcome == DatabaseDiagnosticOutcome::IntegrityProblems
        || foreignKeys.outcome == DatabaseDiagnosticOutcome::ForeignKeyViolations) {
        result.confirmedDatabaseProblems = true;
        result.failure = DatabaseManager::BackupFailure::SourceHealth;
        QStringList findings;
        if (integrity.outcome == DatabaseDiagnosticOutcome::IntegrityProblems) {
            findings.append(QString("integrity_check reported %1 issue(s)")
                                .arg(integrity.issueCount));
        }
        if (foreignKeys.outcome == DatabaseDiagnosticOutcome::ForeignKeyViolations) {
            findings.append(QString("foreign_key_check reported %1 violation(s)")
                                .arg(foreignKeys.violationCount));
        }
        result.errorMessage = QString(
            "Automatic backup was skipped because the live database failed health validation: "
            "%1. Run Tools \u2192 Database Status & Integrity for details.")
                                  .arg(findings.join("; "));
        return result;
    }

    if (integrity.outcome != DatabaseDiagnosticOutcome::Healthy
        || foreignKeys.outcome != DatabaseDiagnosticOutcome::Healthy) {
        const bool busy = integrity.outcome == DatabaseDiagnosticOutcome::BusyOrLocked
                          || foreignKeys.outcome == DatabaseDiagnosticOutcome::BusyOrLocked;
        result.failure = busy ? DatabaseManager::BackupFailure::Busy
                              : DatabaseManager::BackupFailure::SourceHealthCheck;
        QStringList errors;
        if (integrity.outcome != DatabaseDiagnosticOutcome::Healthy)
            errors.append("integrity_check: " + integrity.errorMessage);
        if (foreignKeys.outcome != DatabaseDiagnosticOutcome::Healthy)
            errors.append("foreign_key_check: " + foreignKeys.errorMessage);
        result.errorMessage = QString(
            "Automatic backup was deferred because live database health validation could not "
            "complete: %1. No corruption was confirmed. Run Tools \u2192 Database Status & "
            "Integrity for details.")
                                  .arg(errors.join("; "));
        return result;
    }

    result.healthy = true;
    result.failure = DatabaseManager::BackupFailure::None;
    return result;
}

void AutomaticBackupService::finishBackup(const DatabaseManager::VerifiedBackupResult& result,
                                          const AutomaticBackupRetentionResult& retention,
                                          const QDateTime& attemptedUtc,
                                          qint64 elapsedMilliseconds)
{
    UserSettings& settings = UserSettings::instance();
    if (result.success) {
        settings.setAutomaticBackupLastSuccessfulUtc(attemptedUtc);
        settings.setAutomaticBackupLastSuccessfulPath(result.backupPath);
        if (retention.success) {
            settings.clearAutomaticBackupFailure();
        } else {
            settings.setAutomaticBackupLastFailureUtc(QDateTime::currentDateTimeUtc());
            settings.setAutomaticBackupLastFailureSummary(retention.errorMessage);
        }
        qInfo() << "Automatic database backup completed and verified:"
                << result.backupPath << "DurationMs:" << elapsedMilliseconds;
        if (!retention.success) {
            qWarning() << "Automatic database backup retention cleanup failed:"
                       << retention.errorMessage;
            emit retentionWarning(retention.errorMessage);
        }
        emit backupSucceeded(result.backupPath);
    } else {
        settings.setAutomaticBackupLastFailureUtc(QDateTime::currentDateTimeUtc());
        settings.setAutomaticBackupLastFailureSummary(result.errorMessage);
        qWarning() << "Automatic database backup failed:" << result.errorMessage;
        emit backupFailed(result.failure, result.errorMessage);
    }
    emit stateChanged();
}
