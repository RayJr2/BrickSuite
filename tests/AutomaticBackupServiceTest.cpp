#include "../src/database/DatabaseManager.h"
#include "../src/database/DatabaseSchema.h"
#include "../src/services/database/AutomaticBackupPolicy.h"
#include "../src/services/database/AutomaticBackupService.h"
#include "../src/settings/UserSettings.h"

#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTextStream>
#include <QTimer>
#include <QUuid>

namespace {
bool require(bool condition, const QString& message)
{
    if (!condition) QTextStream(stderr) << "FAILED: " << message << '\n';
    return condition;
}

bool createDatabase(const QString& path, int version)
{
    const QString connection = "AutoBackupTest_" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    bool ok = false;
    {
        QSqlDatabase database = QSqlDatabase::addDatabase("QSQLITE", connection);
        database.setDatabaseName(path);
        if (database.open()) {
            QSqlQuery query(database);
            ok = query.exec("CREATE TABLE schema_version (version INTEGER NOT NULL)")
                 && query.prepare("INSERT INTO schema_version(version) VALUES (?)");
            if (ok) { query.addBindValue(version); ok = query.exec(); }
            if (ok) ok = query.exec("CREATE TABLE payload (id INTEGER PRIMARY KEY, value TEXT)");
            if (ok) ok = query.exec("INSERT INTO payload(value) VALUES ('preserved')");
            database.close();
        }
    }
    QSqlDatabase::removeDatabase(connection);
    return ok;
}

bool createForeignKeyViolationDatabase(const QString& path)
{
    if (!createDatabase(path, 29)) return false;
    const QString connection = "AutoBackupFkTest_"
                               + QUuid::createUuid().toString(QUuid::WithoutBraces);
    bool ok = false;
    {
        QSqlDatabase database = QSqlDatabase::addDatabase("QSQLITE", connection);
        database.setDatabaseName(path);
        if (database.open()) {
            QSqlQuery query(database);
            ok = query.exec("PRAGMA foreign_keys=OFF")
                 && query.exec("CREATE TABLE parent_record (id INTEGER PRIMARY KEY)")
                 && query.exec("CREATE TABLE child_record (id INTEGER PRIMARY KEY, "
                               "parent_id INTEGER REFERENCES parent_record(id))")
                 && query.exec("INSERT INTO child_record(id, parent_id) VALUES (1, 999)");
            database.close();
        }
    }
    QSqlDatabase::removeDatabase(connection);
    return ok;
}

bool waitFor(AutomaticBackupService& service, bool expectSuccess,
             DatabaseManager::BackupFailure* failure = nullptr)
{
    QEventLoop loop;
    bool succeeded = false;
    bool completed = false;
    bool terminalSawIdle = false;
    QObject::connect(&service, &AutomaticBackupService::backupSucceeded, &loop,
                     [&](const QString&) {
                         completed = true; succeeded = true;
                         terminalSawIdle = !service.isRunning(); loop.quit();
                     });
    QObject::connect(&service, &AutomaticBackupService::backupFailed, &loop,
                     [&](DatabaseManager::BackupFailure value, const QString&) {
                         completed = true;
                         terminalSawIdle = !service.isRunning();
                         if (failure) *failure = value;
                         loop.quit();
                     });
    QTimer timeout; timeout.setSingleShot(true);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    timeout.start(15000);
    service.start();
    service.requestBackupNow(UserSettings::instance().automaticBackupRoot(),
                             UserSettings::instance().automaticBackupRetentionCount());
    loop.exec();
    service.stop();
    return completed && terminalSawIdle && succeeded == expectSuccess && !service.isRunning();
}
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setOrganizationName("BrickSuiteAutomaticBackupTest");
    QCoreApplication::setApplicationName(QUuid::createUuid().toString(QUuid::WithoutBraces));
    QTemporaryDir temporary;
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temporary.path());
    QSettings().clear();
    bool ok = require(temporary.isValid(), "temporary directory");
    ok &= require(DatabaseSchema::CurrentSchemaVersion == 29, "schema remains 29");

    UserSettings& settings = UserSettings::instance();
    ok &= require(!settings.automaticBackupEnabled(), "default disabled");
    ok &= require(settings.automaticBackupFrequencyHours() == 24, "default 24-hour frequency");
    ok &= require(settings.automaticBackupRetentionCount() == 14, "default retention");
    settings.setAutomaticBackupFrequencyHours(0);
    settings.setAutomaticBackupRetentionCount(999);
    ok &= require(settings.automaticBackupFrequencyHours() == 24,
                  "invalid stored frequency normalized to 24 hours");
    for (const int hours : AutomaticBackupPolicy::supportedFrequencyHours()) {
        settings.setAutomaticBackupFrequencyHours(hours);
        ok &= require(settings.automaticBackupFrequencyHours() == hours,
                      QString("persist supported %1-hour frequency").arg(hours));
    }
    ok &= require(!QSettings().contains("AutomaticDatabaseBackup/FrequencyDays"),
                  "obsolete unshipped FrequencyDays key absent");
    ok &= require(settings.automaticBackupRetentionCount() == 365, "stored retention normalized");

    const QString source = QDir(temporary.path()).filePath("source.db");
    ok &= require(createDatabase(source, 29), "create source database");
    const QString root = QDir(temporary.path()).filePath("configured root with spaces");
    settings.setAutomaticBackupEnabled(true);
    settings.setAutomaticBackupRoot(root);
    settings.setAutomaticBackupLastAttemptUtc({});
    const QString manualMarker = QDir(temporary.path()).filePath("BrickSuite_Backup_manual.db");
    QFile marker(manualMarker);
    ok &= require(marker.open(QIODevice::WriteOnly), "create manual backup marker");
    marker.close();

    AutomaticBackupService service(nullptr, source);
    ok &= require(waitFor(service, true), "worker creates and verifies backup from read-only source");
    const QString successfulPath = settings.automaticBackupLastSuccessfulPath();
    ok &= require(QFileInfo::exists(successfulPath), "successful backup exists");
    ok &= require(successfulPath.startsWith(root), "configured root retained");
    ok &= require(successfulPath.contains("/v29/") || successfulPath.contains("\\v29\\"),
                  "v29 derived from root");
    ok &= require(settings.automaticBackupLastSuccessfulUtc().isValid(), "success timestamp stored");
    ok &= require(settings.automaticBackupLastFailureSummary().isEmpty(), "failure cleared");
    ok &= require(QFileInfo::exists(manualMarker), "automatic pipeline does not delete manual backup");

    const QDateTime healthGatePriorSuccess = settings.automaticBackupLastSuccessfulUtc();
    const QString healthGateRoot = QDir(temporary.path()).filePath("health gate root");
    const QString healthGateDirectory = AutomaticBackupPolicy::versionDirectory(healthGateRoot, 29);
    ok &= require(QDir().mkpath(healthGateDirectory), "create health-gate fixture directory");
    const QString existingBackup = QDir(healthGateDirectory).filePath(
        "BrickSuite_AutoBackup_v29_2020-01-01_000000.db");
    QFile existingBackupFile(existingBackup);
    ok &= require(existingBackupFile.open(QIODevice::WriteOnly), "create existing backup fixture");
    existingBackupFile.write("existing backup must remain untouched");
    existingBackupFile.close();
    settings.setAutomaticBackupRoot(healthGateRoot);

    auto verifyRejectedHealth = [&](const QString& label,
                                    const AutomaticBackupSourceHealthResult& health,
                                    DatabaseManager::BackupFailure expectedFailure,
                                    bool expectCorruptionWording) {
        int retentionRemovals = 0;
        AutomaticBackupService rejected(
            nullptr, source,
            [&](const QString&) { ++retentionRemovals; return true; },
            [health](const QString&) { return health; });
        DatabaseManager::BackupFailure failure = DatabaseManager::BackupFailure::None;
        const bool completed = waitFor(rejected, false, &failure);
        const QString summary = settings.automaticBackupLastFailureSummary();
        bool passed = require(completed && failure == expectedFailure, label + " rejected")
                      && require(settings.automaticBackupLastSuccessfulUtc()
                                     == healthGatePriorSuccess,
                                 label + " does not update LastSuccessful")
                      && require(retentionRemovals == 0, label + " does not run retention")
                      && require(QFileInfo::exists(existingBackup),
                                 label + " preserves existing backups")
                      && require(summary.size() <= 1000,
                                 label + " persists a bounded failure summary")
                      && require(summary.contains("Tools \u2192 Database Status & Integrity"),
                                 label + " includes database-status guidance");
        if (expectCorruptionWording) {
            passed &= require(summary.contains("failed health validation"),
                              label + " identifies confirmed health failure");
        } else {
            passed &= require(summary.contains("could not complete")
                                  && summary.contains("No corruption was confirmed"),
                              label + " is deferred rather than labeled corruption");
        }
        return passed;
    };

    AutomaticBackupSourceHealthResult integrityFailure;
    integrityFailure.confirmedDatabaseProblems = true;
    integrityFailure.failure = DatabaseManager::BackupFailure::SourceHealth;
    integrityFailure.errorMessage = "Automatic backup was skipped because the live database "
        "failed health validation: integrity_check reported 2 issue(s). "
        "Run Tools \u2192 Database Status & Integrity for details.";
    ok &= verifyRejectedHealth("integrity failure", integrityFailure,
                               DatabaseManager::BackupFailure::SourceHealth, true);

    AutomaticBackupSourceHealthResult foreignKeyFailure;
    foreignKeyFailure.confirmedDatabaseProblems = true;
    foreignKeyFailure.failure = DatabaseManager::BackupFailure::SourceHealth;
    foreignKeyFailure.errorMessage = "Automatic backup was skipped because the live database "
        "failed health validation: foreign_key_check reported 1 violation(s). "
        "Run Tools \u2192 Database Status & Integrity for details.";
    ok &= verifyRejectedHealth("foreign-key failure", foreignKeyFailure,
                               DatabaseManager::BackupFailure::SourceHealth, true);

    AutomaticBackupSourceHealthResult checkFailure;
    checkFailure.failure = DatabaseManager::BackupFailure::SourceHealthCheck;
    checkFailure.errorMessage = "Automatic backup was deferred because live database health "
        "validation could not complete: integrity_check: simulated query failure. "
        "No corruption was confirmed. Run Tools \u2192 Database Status & Integrity for details.";
    ok &= verifyRejectedHealth("health-check execution failure", checkFailure,
                               DatabaseManager::BackupFailure::SourceHealthCheck, false);

    const QString foreignKeySource = QDir(temporary.path()).filePath("foreign-key-source.db");
    ok &= require(createForeignKeyViolationDatabase(foreignKeySource),
                  "create source with a real foreign-key violation");
    AutomaticBackupService realForeignKeyFailure(nullptr, foreignKeySource);
    DatabaseManager::BackupFailure realForeignKeyFailureType = DatabaseManager::BackupFailure::None;
    ok &= require(waitFor(realForeignKeyFailure, false, &realForeignKeyFailureType)
                      && realForeignKeyFailureType == DatabaseManager::BackupFailure::SourceHealth
                      && settings.automaticBackupLastFailureSummary().contains(
                          "foreign_key_check reported 1 violation"),
                  "production health gate consumes and rejects foreign-key violations");

    const QString missingSource = QDir(temporary.path()).filePath("missing-source.db");
    AutomaticBackupService realCheckFailure(nullptr, missingSource);
    DatabaseManager::BackupFailure realCheckFailureType = DatabaseManager::BackupFailure::None;
    ok &= require(waitFor(realCheckFailure, false, &realCheckFailureType)
                      && realCheckFailureType == DatabaseManager::BackupFailure::SourceHealthCheck
                      && settings.automaticBackupLastFailureSummary().contains(
                          "No corruption was confirmed"),
                  "production pre-check execution failure is deferred, not called corruption");
    ok &= require(QFileInfo::exists(existingBackup),
                  "production health failures preserve existing backups");

    const QString manualAfterRejectedHealth = QDir(temporary.path()).filePath(
        "manual-backup-after-health-rejection.db");
    const DatabaseManager::VerifiedBackupResult manualResult =
        DatabaseManager::createVerifiedBackup(source, manualAfterRejectedHealth);
    ok &= require(manualResult.success && QFileInfo::exists(manualAfterRejectedHealth),
                  "manual backup implementation remains independent of automatic health gate");

    settings.setAutomaticBackupRoot(root);

    const QString committedRoot = QDir(temporary.path()).filePath("committed policy root");
    settings.setAutomaticBackupEnabled(false);
    settings.setAutomaticBackupRoot(committedRoot);
    settings.setAutomaticBackupFrequencyHours(1);
    settings.setAutomaticBackupLastSuccessfulUtc(QDateTime::currentDateTimeUtc().addSecs(-3601));
    settings.setAutomaticBackupLastAttemptUtc({});
    AutomaticBackupService committedPolicyService(nullptr, source);
    int committedStarts = 0;
    QEventLoop committedLoop;
    QObject::connect(&committedPolicyService, &AutomaticBackupService::backupStarted,
                     [&](const QString&) { ++committedStarts; });
    QObject::connect(&committedPolicyService, &AutomaticBackupService::backupSucceeded,
                     &committedLoop, &QEventLoop::quit);
    committedPolicyService.start();
    QCoreApplication::processEvents();
    settings.setAutomaticBackupEnabled(true); // Represents Settings OK committing the policy.
    committedPolicyService.reloadPolicy();
    committedPolicyService.reloadPolicy(); // Repeated Apply/reload must not duplicate the attempt.
    QTimer::singleShot(15000, &committedLoop, &QEventLoop::quit);
    committedLoop.exec();
    committedPolicyService.stop();
    ok &= require(committedStarts == 1 && QFileInfo::exists(
                      settings.automaticBackupLastSuccessfulPath()),
                  "committed due policy queues exactly one scheduled backup");

    settings.setAutomaticBackupLastSuccessfulUtc(QDateTime::currentDateTimeUtc());
    settings.setAutomaticBackupLastAttemptUtc({});
    AutomaticBackupService notDueService(nullptr, source);
    int notDueStarts = 0;
    QObject::connect(&notDueService, &AutomaticBackupService::backupStarted,
                     [&](const QString&) { ++notDueStarts; });
    notDueService.start();
    notDueService.reloadPolicy();
    QEventLoop notDueLoop;
    QTimer::singleShot(150, &notDueLoop, &QEventLoop::quit);
    notDueLoop.exec();
    notDueService.stop();
    ok &= require(notDueStarts == 0, "committed policy that is not due does not run");

    settings.setAutomaticBackupEnabled(false);
    AutomaticBackupService cancelledEditService(nullptr, source);
    int cancelledStarts = 0;
    QObject::connect(&cancelledEditService, &AutomaticBackupService::backupStarted,
                     [&](const QString&) { ++cancelledStarts; });
    cancelledEditService.start();
    QEventLoop cancelledLoop;
    QTimer::singleShot(100, &cancelledLoop, &QEventLoop::quit);
    cancelledLoop.exec();
    cancelledEditService.stop();
    ok &= require(!settings.automaticBackupEnabled() && cancelledStarts == 0,
                  "uncommitted enable edit does not persist or schedule");
    settings.setAutomaticBackupEnabled(true);

    const QString retentionRoot = QDir(temporary.path()).filePath("retention root");
    const QString retentionDirectory = AutomaticBackupPolicy::versionDirectory(retentionRoot, 29);
    ok &= require(QDir().mkpath(retentionDirectory), "create retention directory");
    const QString oldOne = QDir(retentionDirectory).filePath(
        "BrickSuite_AutoBackup_v29_2020-01-01_000000.db");
    const QString oldTwo = QDir(retentionDirectory).filePath(
        "BrickSuite_AutoBackup_v29_2020-01-02_000000.db");
    QFile oldOneFile(oldOne); QFile oldTwoFile(oldTwo);
    ok &= require(oldOneFile.open(QIODevice::WriteOnly) && oldTwoFile.open(QIODevice::WriteOnly),
                  "create service retention fixtures");
    oldOneFile.close(); oldTwoFile.close();
    AutomaticBackupService retentionService(nullptr, source);
    QEventLoop retentionLoop;
    bool retentionCompleted = false;
    QObject::connect(&retentionService, &AutomaticBackupService::backupSucceeded,
                     &retentionLoop, [&]() { retentionCompleted = true; retentionLoop.quit(); });
    retentionService.start();
    retentionService.requestBackupNow(retentionRoot, 2);
    QTimer::singleShot(15000, &retentionLoop, &QEventLoop::quit);
    retentionLoop.exec();
    retentionService.stop();
    const QStringList retained = QDir(retentionDirectory).entryList(
        {"BrickSuite_AutoBackup_v29_*.db"}, QDir::Files, QDir::Name);
    ok &= require(retentionCompleted && retained.size() == 2 && !QFileInfo::exists(oldOne)
                      && QFileInfo::exists(oldTwo),
                  "verified service backup runs retention and protects new file");

    const QString warningRoot = QDir(temporary.path()).filePath("retention warning root");
    const QString warningDirectory = AutomaticBackupPolicy::versionDirectory(warningRoot, 29);
    ok &= require(QDir().mkpath(warningDirectory), "create warning directory");
    const QString warningOld = QDir(warningDirectory).filePath(
        "BrickSuite_AutoBackup_v29_2020-01-01_000000.db");
    QFile warningOldFile(warningOld);
    ok &= require(warningOldFile.open(QIODevice::WriteOnly), "create warning fixture");
    warningOldFile.close();
    AutomaticBackupService warningService(nullptr, source, [](const QString&) { return false; });
    QEventLoop warningLoop;
    bool warningSawIdle = false;
    bool warningBackupSucceeded = false;
    QObject::connect(&warningService, &AutomaticBackupService::retentionWarning,
                     [&](const QString&) { warningSawIdle = !warningService.isRunning(); });
    QObject::connect(&warningService, &AutomaticBackupService::backupSucceeded,
                     [&](const QString&) { warningBackupSucceeded = true; warningLoop.quit(); });
    warningService.start();
    warningService.requestBackupNow(warningRoot, 1);
    QTimer::singleShot(15000, &warningLoop, &QEventLoop::quit);
    warningLoop.exec();
    warningService.stop();
    ok &= require(warningSawIdle && warningBackupSucceeded && QFileInfo::exists(warningOld),
                  "cleanup warning is terminal, retryable, and backup remains successful");
    ok &= require(!settings.automaticBackupLastFailureSummary().isEmpty(),
                  "cleanup warning updates live persistent status");

    settings.setAutomaticBackupRoot(root);
    settings.setAutomaticBackupEnabled(false);
    AutomaticBackupService explicitWhileDisabled(nullptr, source);
    ok &= require(waitFor(explicitWhileDisabled, true), "explicit request bypasses schedule policy");
    settings.setAutomaticBackupEnabled(true);

    const QDateTime priorSuccess = settings.automaticBackupLastSuccessfulUtc();
    const QString badSource = QDir(temporary.path()).filePath("old-schema.db");
    ok &= require(createDatabase(badSource, 28), "create verification-failure database");
    settings.setAutomaticBackupLastAttemptUtc({});
    AutomaticBackupService failing(nullptr, badSource);
    DatabaseManager::BackupFailure failure = DatabaseManager::BackupFailure::None;
    ok &= require(waitFor(failing, false, &failure), "verification failure returned");
    ok &= require(failure == DatabaseManager::BackupFailure::Verification, "failure classified");
    ok &= require(settings.automaticBackupLastSuccessfulUtc() == priorSuccess,
                  "failure does not advance success");
    ok &= require(!settings.automaticBackupLastFailureSummary().isEmpty(), "failure persisted");

    QDir versionDir(AutomaticBackupPolicy::versionDirectory(root, 29));
    const QStringList candidates = versionDir.entryList(
        {"BrickSuite_AutoBackup_v*.db"}, QDir::Files);
    ok &= require(candidates.size() == 2, "unverified output cannot masquerade as backup");

    const QString impossibleRoot = source + "/child";
    settings.setAutomaticBackupRoot(impossibleRoot);
    settings.setAutomaticBackupLastAttemptUtc({});
    AutomaticBackupService unavailable(nullptr, source);
    ok &= require(waitFor(unavailable, false), "unavailable explicit root fails");
    ok &= require(settings.automaticBackupRoot() == impossibleRoot,
                  "unavailable explicit root is not replaced");

    QSettings().clear();
    if (ok) QTextStream(stdout) << "AutomaticBackupServiceTest passed\n";
    return ok ? 0 : 1;
}
