#include "DatabaseStatusService.h"

#include <QElapsedTimer>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

namespace
{
constexpr int BusyTimeoutMilliseconds = 5000;
constexpr int MaximumReportedIssues = 25;

DatabaseDiagnosticOutcome classifyError(const QSqlError& error)
{
    const QString text = error.text().toLower();
    if (text.contains("locked") || text.contains("busy")
        || error.nativeErrorCode() == "5" || error.nativeErrorCode() == "6") {
        return DatabaseDiagnosticOutcome::BusyOrLocked;
    }
    return DatabaseDiagnosticOutcome::QueryFailure;
}

DatabaseDiagnosticOutcome classifyErrorText(const QString& text)
{
    const QString lower = text.toLower();
    return lower.contains("locked") || lower.contains("busy")
        ? DatabaseDiagnosticOutcome::BusyOrLocked
        : DatabaseDiagnosticOutcome::QueryFailure;
}

struct Connection
{
    QString name;
    QSqlDatabase database;

    ~Connection()
    {
        if (database.isValid()) database.close();
        database = QSqlDatabase();
        if (!name.isEmpty()) QSqlDatabase::removeDatabase(name);
    }
};

DatabaseDiagnosticOutcome openReadOnly(const QString& path, Connection& connection, QString& error)
{
    if (!QFileInfo::exists(path)) {
        error = QString("Database file does not exist: %1").arg(path);
        return DatabaseDiagnosticOutcome::DatabaseMissing;
    }

    connection.name = QString("database_status_%1").arg(
        QUuid::createUuid().toString(QUuid::WithoutBraces));
    connection.database = QSqlDatabase::addDatabase("QSQLITE", connection.name);
    connection.database.setDatabaseName(path);
    connection.database.setConnectOptions("QSQLITE_OPEN_READONLY");
    if (!connection.database.open()) {
        error = connection.database.lastError().text();
        const DatabaseDiagnosticOutcome classified = classifyError(connection.database.lastError());
        return classified == DatabaseDiagnosticOutcome::BusyOrLocked
            ? classified : DatabaseDiagnosticOutcome::OpenFailure;
    }

    QSqlQuery query(connection.database);
    if (!query.exec("PRAGMA foreign_keys=ON")
        || !query.exec("PRAGMA query_only=ON")
        || !query.exec(QString("PRAGMA busy_timeout=%1").arg(BusyTimeoutMilliseconds))) {
        error = query.lastError().text();
        return classifyError(query.lastError());
    }
    return DatabaseDiagnosticOutcome::Healthy;
}

bool scalar(QSqlDatabase& database, const QString& sql, QVariant& value, QString& error)
{
    QSqlQuery query(database);
    if (!query.exec(sql) || !query.next()) {
        error = query.lastError().text().isEmpty() ? QString("Query returned no result: %1").arg(sql)
                                                   : query.lastError().text();
        return false;
    }
    value = query.value(0);
    return true;
}
}

DatabaseStatusSnapshot DatabaseStatusService::statusSnapshot(const QString& databasePath)
{
    DatabaseStatusSnapshot result;
    result.databasePath = databasePath;
    const QFileInfo file(databasePath);
    result.fileSize = file.size();
    result.lastModified = file.lastModified();
    result.walExists = QFileInfo::exists(databasePath + "-wal");
    result.walSize = QFileInfo(databasePath + "-wal").size();
    result.shmExists = QFileInfo::exists(databasePath + "-shm");
    result.shmSize = QFileInfo(databasePath + "-shm").size();

    Connection connection;
    result.outcome = openReadOnly(databasePath, connection, result.errorMessage);
    if (result.outcome != DatabaseDiagnosticOutcome::Healthy) return result;

    QVariant value;
    if (!scalar(connection.database, "SELECT version FROM schema_version LIMIT 1", value,
                result.errorMessage)) {
        result.outcome = classifyErrorText(result.errorMessage);
        return result;
    }
    result.schemaVersion = value.toInt();

    struct Field { const char* sql; qint64* target; } fields[] = {
        {"SELECT COUNT(*) FROM part", &result.partCount},
        {"SELECT COUNT(*) FROM set_catalog", &result.setCount},
        {"SELECT COUNT(*) FROM minifig_catalog WHERE is_active=1", &result.minifigCount},
        {"SELECT COUNT(*) FROM inventory_record", &result.inventoryRecordCount},
        {"SELECT COALESCE(SUM(quantity),0) FROM inventory_record", &result.inventoryPieceCount},
        {"SELECT COUNT(*) FROM build WHERE is_active=1", &result.activeBuildCount},
        {"SELECT COUNT(*) FROM build WHERE is_active=0", &result.archivedBuildCount},
        {"SELECT COUNT(*) FROM manufacturer WHERE is_active=1", &result.activeManufacturerCount},
        {"SELECT COUNT(*) FROM storage_location WHERE is_active=1", &result.activeStorageLocationCount},
        {"PRAGMA page_size", &result.pageSize}, {"PRAGMA page_count", &result.pageCount},
        {"PRAGMA freelist_count", &result.freelistPageCount}
    };
    for (const Field& field : fields) {
        QVariant value;
        if (!scalar(connection.database, field.sql, value, result.errorMessage)) {
            result.outcome = classifyErrorText(result.errorMessage);
            return result;
        }
        *field.target = value.toLongLong();
    }

    if (!scalar(connection.database, "SELECT sqlite_version()", value, result.errorMessage)) {
        result.outcome = classifyErrorText(result.errorMessage); return result;
    }
    result.sqliteVersion = value.toString();
    if (!scalar(connection.database, "PRAGMA journal_mode", value, result.errorMessage)) {
        result.outcome = classifyErrorText(result.errorMessage); return result;
    }
    result.journalMode = value.toString();
    if (!scalar(connection.database, "PRAGMA foreign_keys", value, result.errorMessage)) {
        result.outcome = classifyErrorText(result.errorMessage); return result;
    }
    result.foreignKeysEnabled = value.toBool();
    return result;
}

DatabaseIntegrityCheckResult DatabaseStatusService::runIntegrityCheck(const QString& databasePath)
{
    DatabaseIntegrityCheckResult result;
    QElapsedTimer timer; timer.start();
    Connection connection;
    result.outcome = openReadOnly(databasePath, connection, result.errorMessage);
    if (result.outcome != DatabaseDiagnosticOutcome::Healthy) {
        result.elapsedMilliseconds = timer.elapsed(); return result;
    }
    QSqlQuery query(connection.database);
    if (!query.exec("PRAGMA integrity_check")) {
        result.outcome = classifyError(query.lastError()); result.errorMessage = query.lastError().text();
        result.elapsedMilliseconds = timer.elapsed(); return result;
    }
    QStringList rows;
    while (query.next()) {
        rows.append(query.value(0).toString());
    }
    result = interpretIntegrityResults(rows);
    result.elapsedMilliseconds = timer.elapsed();
    return result;
}

DatabaseIntegrityCheckResult DatabaseStatusService::interpretIntegrityResults(const QStringList& rows)
{
    DatabaseIntegrityCheckResult result;
    if (rows.size() == 1 && rows.first().compare("ok", Qt::CaseInsensitive) == 0) {
        result.outcome = DatabaseDiagnosticOutcome::Healthy;
        return result;
    }
    result.outcome = DatabaseDiagnosticOutcome::IntegrityProblems;
    result.issueCount = rows.size();
    result.representativeIssues = rows.mid(0, MaximumReportedIssues);
    return result;
}

DatabaseForeignKeyCheckResult DatabaseStatusService::runForeignKeyCheck(const QString& databasePath)
{
    DatabaseForeignKeyCheckResult result;
    QElapsedTimer timer; timer.start();
    Connection connection;
    result.outcome = openReadOnly(databasePath, connection, result.errorMessage);
    if (result.outcome != DatabaseDiagnosticOutcome::Healthy) {
        result.elapsedMilliseconds = timer.elapsed(); return result;
    }
    QSqlQuery query(connection.database);
    if (!query.exec("PRAGMA foreign_key_check")) {
        result.outcome = classifyError(query.lastError()); result.errorMessage = query.lastError().text();
        result.elapsedMilliseconds = timer.elapsed(); return result;
    }
    while (query.next()) {
        ++result.violationCount;
        if (result.representativeViolations.size() < MaximumReportedIssues) {
            result.representativeViolations.append({query.value(0).toString(), query.value(1).toLongLong(),
                                                     query.value(2).toString(), query.value(3).toInt()});
        }
    }
    result.outcome = result.violationCount == 0 ? DatabaseDiagnosticOutcome::Healthy
                                                : DatabaseDiagnosticOutcome::ForeignKeyViolations;
    result.elapsedMilliseconds = timer.elapsed();
    return result;
}
