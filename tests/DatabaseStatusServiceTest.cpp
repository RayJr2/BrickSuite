#include "../src/services/database/DatabaseStatusService.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTextStream>

namespace
{
bool require(bool condition, const QString& message)
{
    if (!condition) QTextStream(stderr) << "FAILED: " << message << '\n';
    return condition;
}

bool exec(QSqlDatabase& db, const QString& sql)
{
    QSqlQuery query(db);
    if (query.exec(sql)) return true;
    QTextStream(stderr) << "SQL failed: " << query.lastError().text() << '\n';
    return false;
}
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir directory;
    if (!require(directory.isValid(), "temporary directory")) return 1;
    const QString path = directory.filePath("status.db");
    const QString connectionName = "database_status_fixture";
    {
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", connectionName);
        db.setDatabaseName(path);
        if (!require(db.open(), "open fixture")) return 1;
        const QStringList schema = {
            "CREATE TABLE schema_version(version INTEGER)", "INSERT INTO schema_version VALUES(29)",
            "CREATE TABLE part(id INTEGER PRIMARY KEY)", "INSERT INTO part VALUES(1)",
            "CREATE TABLE set_catalog(id INTEGER PRIMARY KEY)", "INSERT INTO set_catalog VALUES(1)",
            "CREATE TABLE minifig_catalog(id INTEGER PRIMARY KEY,is_active INTEGER)", "INSERT INTO minifig_catalog VALUES(1,1)",
            "CREATE TABLE inventory_record(id INTEGER PRIMARY KEY,quantity INTEGER)", "INSERT INTO inventory_record VALUES(1,7)",
            "CREATE TABLE build(id INTEGER PRIMARY KEY,is_active INTEGER)", "INSERT INTO build VALUES(1,1)", "INSERT INTO build VALUES(2,0)",
            "CREATE TABLE manufacturer(id INTEGER PRIMARY KEY,is_active INTEGER)", "INSERT INTO manufacturer VALUES(1,1)",
            "CREATE TABLE storage_location(id INTEGER PRIMARY KEY,is_active INTEGER)", "INSERT INTO storage_location VALUES(1,1)",
            "CREATE TABLE parent(id INTEGER PRIMARY KEY)",
            "CREATE TABLE child(id INTEGER PRIMARY KEY,parent_id INTEGER REFERENCES parent(id))"
        };
        for (const QString& statement : schema) if (!exec(db, statement)) return 1;
        db.close();
    }
    QSqlDatabase::removeDatabase(connectionName);

    const auto snapshot = DatabaseStatusService::statusSnapshot(path);
    if (!require(snapshot.outcome == DatabaseDiagnosticOutcome::Healthy, snapshot.errorMessage)
        || !require(snapshot.schemaVersion == 29, "schema version")
        || !require(snapshot.partCount == 1 && snapshot.setCount == 1 && snapshot.minifigCount == 1, "catalog counts")
        || !require(snapshot.inventoryRecordCount == 1 && snapshot.inventoryPieceCount == 7, "inventory counts")
        || !require(snapshot.activeBuildCount == 1 && snapshot.archivedBuildCount == 1, "build counts")
        || !require(snapshot.activeManufacturerCount == 1 && snapshot.activeStorageLocationCount == 1, "reference counts")
        || !require(snapshot.fileSize > 0 && QFileInfo::exists(snapshot.databasePath), "file status")) return 1;

    const auto integrity = DatabaseStatusService::runIntegrityCheck(path);
    if (!require(integrity.outcome == DatabaseDiagnosticOutcome::Healthy, "healthy integrity")) return 1;
    const auto interpreted = DatabaseStatusService::interpretIntegrityResults(
        {"ok", "additional diagnostic", "another diagnostic"});
    if (!require(interpreted.outcome == DatabaseDiagnosticOutcome::IntegrityProblems
                 && interpreted.issueCount == 3 && interpreted.representativeIssues.size() == 3,
                 "complete integrity result interpretation")) return 1;
    const auto healthyFk = DatabaseStatusService::runForeignKeyCheck(path);
    if (!require(healthyFk.outcome == DatabaseDiagnosticOutcome::Healthy, "healthy foreign keys")) return 1;

    {
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", connectionName);
        db.setDatabaseName(path);
        if (!db.open() || !exec(db, "PRAGMA foreign_keys=OFF") || !exec(db, "INSERT INTO child VALUES(1,999)")) return 1;
        db.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
    const auto brokenFk = DatabaseStatusService::runForeignKeyCheck(path);
    if (!require(brokenFk.outcome == DatabaseDiagnosticOutcome::ForeignKeyViolations
                 && brokenFk.violationCount == 1 && brokenFk.representativeViolations.size() == 1,
                 "foreign-key violation")) return 1;

    {
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", connectionName);
        db.setDatabaseName(path);
        if (!db.open() || !exec(db, "BEGIN EXCLUSIVE")) return 1;
        const auto busy = DatabaseStatusService::statusSnapshot(path);
        if (!require(busy.outcome == DatabaseDiagnosticOutcome::BusyOrLocked,
                     QString("busy/locked outcome (actual %1: %2)")
                         .arg(static_cast<int>(busy.outcome)).arg(busy.errorMessage))) return 1;
        if (!exec(db, "ROLLBACK")) return 1;
        db.close();
    }
    QSqlDatabase::removeDatabase(connectionName);

    const QString missing = directory.filePath("missing.db");
    const auto missingResult = DatabaseStatusService::runIntegrityCheck(missing);
    if (!require(missingResult.outcome == DatabaseDiagnosticOutcome::DatabaseMissing, "missing outcome")
        || !require(!QFileInfo::exists(missing), "missing file was not created")) return 1;

    const QString incomplete = directory.filePath("incomplete.db");
    {
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", connectionName);
        db.setDatabaseName(incomplete); if (!db.open()) return 1; db.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
    const auto queryFailure = DatabaseStatusService::statusSnapshot(incomplete);
    if (!require(queryFailure.outcome == DatabaseDiagnosticOutcome::QueryFailure,
                 "query failure is distinct from integrity problems")) return 1;

    for (const QString& name : QSqlDatabase::connectionNames())
        if (!require(!name.startsWith("database_status_"), "diagnostic connection cleanup")) return 1;

    {
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", connectionName);
        db.setDatabaseName(path);
        if (!db.open()) return 1;
        QSqlQuery query(db);
        if (!query.exec("SELECT version FROM schema_version") || !query.next()
            || !require(query.value(0).toInt() == 29, "diagnostics preserved schema and data")) return 1;
        db.close();
    }
    QSqlDatabase::removeDatabase(connectionName);

    QTextStream(stdout) << "DatabaseStatusServiceTest passed\n";
    return 0;
}
