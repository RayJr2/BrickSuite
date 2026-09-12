#include "../src/database/DatabaseManager.h"
#include "../src/database/DatabaseSchema.h"
#include "../src/network/HostDataEpoch.h"
#include "../src/services/database/DatabaseFileValidator.h"
#include "../src/services/database/DatabaseRestoreTransaction.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QDebug>

namespace {
bool require(bool condition, const QString& message)
{
    if (!condition) qCritical().noquote() << message;
    return condition;
}

bool createDatabase(const QString& path, const QString& name)
{
    const QString connection = QStringLiteral("restore-fixture-%1")
        .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    bool result = false;
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection);
        database.setDatabaseName(path);
        if (database.open() && DatabaseSchema::initialize(database)) {
            QSqlQuery query(database);
            query.prepare(QStringLiteral("INSERT INTO workspace(name, description, created_utc, modified_utc, is_active) VALUES(?, '', '2026-01-01T00:00:00.000Z', '2026-01-01T00:00:00.000Z', 1)"));
            query.addBindValue(name);
            result = query.exec();
        }
        database.close();
    }
    QSqlDatabase::removeDatabase(connection);
    return result;
}

bool hasWorkspace(const QString& path, const QString& name)
{
    const QString connection = QStringLiteral("restore-read-%1")
        .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    bool found = false;
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection);
        database.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY"));
        database.setDatabaseName(path);
        if (database.open()) {
            QSqlQuery query(database);
            query.prepare(QStringLiteral("SELECT 1 FROM workspace WHERE name = ?"));
            query.addBindValue(name);
            found = query.exec() && query.next();
        }
        database.close();
    }
    QSqlDatabase::removeDatabase(connection);
    return found;
}

bool executeSql(const QString& path, const QStringList& statements)
{
    const QString connection = QStringLiteral("restore-sql-%1")
        .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    bool success = true;
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection);
        database.setDatabaseName(path);
        success = database.open();
        for (const QString& statement : statements)
            success = success && QSqlQuery(database).exec(statement);
        database.close();
    }
    QSqlDatabase::removeDatabase(connection);
    return success;
}

bool writeJournal(const QString& path, const QString& live, const QString& previous,
                  const QString& stage, const QString& oldEpoch, const QString& newEpoch,
                  const QString& state = QStringLiteral("prepared"))
{
    QSaveFile file(path);
    const QByteArray data = QJsonDocument(QJsonObject{
        {QStringLiteral("version"), 1}, {QStringLiteral("state"), state},
        {QStringLiteral("live"), live}, {QStringLiteral("previous"), previous},
        {QStringLiteral("stage"), stage}, {QStringLiteral("oldEpoch"), oldEpoch},
        {QStringLiteral("newEpoch"), newEpoch}}).toJson(QJsonDocument::Compact);
    return file.open(QIODevice::WriteOnly) && file.write(data) == data.size() && file.commit();
}
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir temporary;
    bool ok = require(temporary.isValid(), QStringLiteral("Temporary directory unavailable."));
    const QString state = QDir(temporary.path()).filePath(QStringLiteral("state"));
    const QString live = QDir(temporary.path()).filePath(QStringLiteral("BrickSuite.db"));
    const QString candidate = QDir(temporary.path()).filePath(QStringLiteral("candidate.db"));
    ok &= require(createDatabase(live, QStringLiteral("Database A")), QStringLiteral("DB A fixture failed."));
    ok &= require(createDatabase(candidate, QStringLiteral("Database B")), QStringLiteral("DB B fixture failed."));

    const auto bootstrap = HostDataEpoch::loadOrBootstrap(state);
    const auto reload = HostDataEpoch::loadExisting(state);
    ok &= require(bootstrap.success && bootstrap.bootstrapped && reload.success
                      && bootstrap.epoch == reload.epoch,
                  QStringLiteral("Epoch bootstrap/reload did not preserve identity."));
    ok &= require(DatabaseFileValidator::validate(live).valid,
                  QStringLiteral("Valid BrickSuite DB was rejected."));
    const int beforeConnections = QSqlDatabase::connectionNames().size();
    DatabaseFileValidator::validate(live);
    DatabaseFileValidator::validate(live);
    ok &= require(QSqlDatabase::connectionNames().size() == beforeConnections,
                  QStringLiteral("Validator leaked a Qt SQL connection."));

    const QString textFile = QDir(temporary.path()).filePath(QStringLiteral("not-sqlite.db"));
    { QFile file(textFile); ok &= require(file.open(QIODevice::WriteOnly)
          && file.write("not sqlite") == 10, QStringLiteral("Text fixture failed.")); }
    ok &= require(!DatabaseFileValidator::validate(textFile).valid,
                  QStringLiteral("Non-SQLite input was accepted."));
    const QString plainSqlite = QDir(temporary.path()).filePath(QStringLiteral("plain.db"));
    {
        const QString name = QStringLiteral("plain");
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), name);
        db.setDatabaseName(plainSqlite); db.open(); QSqlQuery(db).exec(QStringLiteral("CREATE TABLE x(id INTEGER)"));
        db.close(); db = {}; QSqlDatabase::removeDatabase(name);
    }
    ok &= require(DatabaseFileValidator::validate(plainSqlite).failure
                      == DatabaseFileValidator::Failure::NotBrickSuite,
                  QStringLiteral("Non-BrickSuite SQLite input was not classified."));
    const QString unsupported = QDir(temporary.path()).filePath(QStringLiteral("unsupported.db"));
    QFile::copy(live, unsupported);
    ok &= require(executeSql(unsupported, {QStringLiteral("UPDATE schema_version SET version=999")})
                      && DatabaseFileValidator::validate(unsupported).failure
                          == DatabaseFileValidator::Failure::UnsupportedSchema,
                  QStringLiteral("Unsupported schema was not rejected."));
    const QString foreignKeyInvalid = QDir(temporary.path()).filePath(QStringLiteral("fk-invalid.db"));
    QFile::copy(live, foreignKeyInvalid);
    ok &= require(executeSql(foreignKeyInvalid, {
                      QStringLiteral("PRAGMA foreign_keys=OFF"),
                      QStringLiteral("CREATE TABLE restore_parent(id INTEGER PRIMARY KEY)"),
                      QStringLiteral("CREATE TABLE restore_child(parent_id INTEGER REFERENCES restore_parent(id))"),
                      QStringLiteral("INSERT INTO restore_child(parent_id) VALUES(99)")})
                      && DatabaseFileValidator::validate(foreignKeyInvalid).failure
                          == DatabaseFileValidator::Failure::ForeignKeys,
                  QStringLiteral("Foreign-key-invalid database was not rejected."));

    const QString missing = QDir(temporary.path()).filePath(QStringLiteral("missing.db"));
    ok &= require(DatabaseFileValidator::validate(missing).failure
                      == DatabaseFileValidator::Failure::Missing,
                  QStringLiteral("Missing database was not classified."));
    ok &= require(DatabaseRestoreTransaction::isControlledRestoreArtifact(live, live, state),
                  QStringLiteral("Live database was not rejected as a controlled artifact."));

    auto expectRollback = [&](const DatabaseRestoreTransaction::FaultInjection& faults,
                              DatabaseRestoreTransaction::Failure failure,
                              const QString& label) {
        const QString safety = QDir(temporary.path()).filePath(label + QStringLiteral("-safety.db"));
        const QString epochBefore = HostDataEpoch::loadExisting(state).epoch;
        const auto result = DatabaseRestoreTransaction::execute(
            candidate, live, safety, state, state, &faults);
        return require(!result.success && result.failure == failure
                           && result.rollbackSucceeded
                           && HostDataEpoch::loadExisting(state).epoch == epochBefore
                           && hasWorkspace(live, QStringLiteral("Database A")),
                       label + QStringLiteral(" did not recover DB A/Epoch A."));
    };
    DatabaseRestoreTransaction::FaultInjection fault;
    fault.failSafetyBackup = true;
    const auto safetyFailure = DatabaseRestoreTransaction::execute(
        candidate, live, QDir(temporary.path()).filePath(QStringLiteral("failed-safety.db")),
        state, state, &fault);
    ok &= require(!safetyFailure.success
                      && safetyFailure.failure == DatabaseRestoreTransaction::Failure::SafetyBackup
                      && !safetyFailure.rollbackSucceeded
                      && hasWorkspace(live, QStringLiteral("Database A"))
                      && HostDataEpoch::loadExisting(state).epoch == bootstrap.epoch,
                  QStringLiteral("Safety-backup failure entered destructive replacement."));
    fault = {}; fault.failReplacement = true;
    ok &= expectRollback(fault, DatabaseRestoreTransaction::Failure::Replacement,
                         QStringLiteral("replacement-failure"));
    fault = {}; fault.failInstalledValidation = true;
    ok &= expectRollback(fault, DatabaseRestoreTransaction::Failure::InstalledValidation,
                         QStringLiteral("validation-failure"));
    fault = {}; fault.failEpochCommit = true;
    ok &= expectRollback(fault, DatabaseRestoreTransaction::Failure::EpochCommit,
                         QStringLiteral("epoch-failure"));

    fault = {}; fault.failReplacement = true; fault.failRollback = true;
    const auto rollbackFailure = DatabaseRestoreTransaction::execute(
        candidate, live,
        QDir(temporary.path()).filePath(QStringLiteral("rollback-failure-safety.db")),
        state, state, &fault);
    ok &= require(!rollbackFailure.success && !rollbackFailure.rollbackSucceeded
                      && rollbackFailure.failure == DatabaseRestoreTransaction::Failure::Rollback
                      && QFile::exists(DatabaseRestoreTransaction::journalPath(state)),
                  QStringLiteral("Rollback failure did not preserve recovery state."));
    const auto retryRecovery = DatabaseRestoreTransaction::recoverAtStartup(live, state, state);
    ok &= require(retryRecovery.success && retryRecovery.recoveredPreviousDatabase
                      && hasWorkspace(live, QStringLiteral("Database A")),
                  QStringLiteral("Startup did not recover a retained rollback failure."));

    const QString blockedEpochDirectory = QDir(temporary.path()).filePath(
        QStringLiteral("epoch-directory-blocker"));
    { QFile blocker(blockedEpochDirectory); ok &= require(blocker.open(QIODevice::WriteOnly),
          QStringLiteral("Epoch persistence blocker fixture failed.")); }
    QString persistenceError;
    ok &= require(!HostDataEpoch::persist(HostDataEpoch::create(), &persistenceError,
                                          blockedEpochDirectory),
                  QStringLiteral("Epoch persistence failure was not reported."));

    const QString safety1 = QDir(temporary.path()).filePath(QStringLiteral("safety1.db"));
    const auto first = DatabaseRestoreTransaction::execute(candidate, live, safety1, state, state);
    const auto epochB = HostDataEpoch::loadExisting(state);
    ok &= require(first.success && first.restartRequired && epochB.success
                      && epochB.epoch != bootstrap.epoch && hasWorkspace(live, QStringLiteral("Database B"))
                      && QFile::exists(safety1),
                  QStringLiteral("Successful Restore did not install DB B and advance once."));

    const QString safety2 = QDir(temporary.path()).filePath(QStringLiteral("safety2.db"));
    const auto second = DatabaseRestoreTransaction::execute(candidate, live, safety2, state, state);
    const auto epochC = HostDataEpoch::loadExisting(state);
    ok &= require(second.success && epochC.success && epochC.epoch != epochB.epoch,
                  QStringLiteral("Restoring the same backup did not create a new generation."));

    const QString oldEpoch = epochC.epoch;
    const auto rejected = DatabaseRestoreTransaction::execute(textFile, live,
        QDir(temporary.path()).filePath(QStringLiteral("unused.db")), state, state);
    ok &= require(!rejected.success && HostDataEpoch::loadExisting(state).epoch == oldEpoch
                      && hasWorkspace(live, QStringLiteral("Database B")),
                  QStringLiteral("Failed preflight changed the database or epoch."));

    // Simulate interruption after DB B installation but before Epoch B commit.
    const QString previous = live + QStringLiteral(".restore_previous");
    const QString stagePath = live + QStringLiteral(".restore_candidate");
    QFile::remove(previous);
    ok &= require(QFile::rename(live, previous) && QFile::copy(candidate, live),
                  QStringLiteral("Crash fixture replacement failed."));
    const QString pendingEpoch = HostDataEpoch::create();
    ok &= require(writeJournal(DatabaseRestoreTransaction::journalPath(state), live, previous,
                               stagePath, oldEpoch, pendingEpoch),
                  QStringLiteral("Crash fixture journal failed."));
    const auto recovered = DatabaseRestoreTransaction::recoverAtStartup(live, state, state);
    ok &= require(recovered.success && recovered.recoveredPreviousDatabase
                      && HostDataEpoch::loadExisting(state).epoch == oldEpoch,
                  QStringLiteral("Prepared Restore did not recover DB A/Epoch A."));

    // Simulate residue after Epoch B was committed: startup must keep DB B.
    QFile::copy(live, previous);
    const QString committedEpoch = HostDataEpoch::create();
    ok &= require(HostDataEpoch::persist(committedEpoch, nullptr, state)
                      && writeJournal(DatabaseRestoreTransaction::journalPath(state), live, previous,
                                      stagePath, oldEpoch, committedEpoch),
                  QStringLiteral("Committed crash fixture failed."));
    const auto completed = DatabaseRestoreTransaction::recoverAtStartup(live, state, state);
    ok &= require(completed.success && completed.completedCommittedRestore
                      && HostDataEpoch::loadExisting(state).epoch == committedEpoch,
                  QStringLiteral("Post-commit residue incorrectly rolled back."));

    const QString malformedState = QDir(temporary.path()).filePath(QStringLiteral("malformed"));
    QDir().mkpath(malformedState);
    { QFile file(QDir(malformedState).filePath(QStringLiteral("host-data-epoch.json")));
      ok &= require(file.open(QIODevice::WriteOnly) && file.write("{}") == 2,
                    QStringLiteral("Malformed epoch fixture failed.")); }
    ok &= require(!HostDataEpoch::loadOrBootstrap(malformedState).success,
                  QStringLiteral("Malformed persisted epoch was silently regenerated."));
    const QString missingState = QDir(temporary.path()).filePath(QStringLiteral("missing"));
    ok &= require(HostDataEpoch::loadOrBootstrap(missingState).success,
                  QStringLiteral("Missing-state fixture did not bootstrap."));
    QFile::remove(QDir(missingState).filePath(QStringLiteral("host-data-epoch.json")));
    ok &= require(!HostDataEpoch::loadOrBootstrap(missingState).success,
                  QStringLiteral("Unexpected epoch loss was silently regenerated."));
    return ok ? 0 : 1;
}
