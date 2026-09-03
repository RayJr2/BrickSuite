/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 * Copyright (C) 2026 RF StateSide, LLC
 */
#pragma once

#include <QDateTime>
#include <QString>
#include <QStringList>
#include <QVector>

enum class DatabaseDiagnosticOutcome
{
    Healthy,
    IntegrityProblems,
    ForeignKeyViolations,
    BusyOrLocked,
    DatabaseMissing,
    OpenFailure,
    QueryFailure
};

struct DatabaseStatusSnapshot
{
    DatabaseDiagnosticOutcome outcome = DatabaseDiagnosticOutcome::Healthy;
    QString errorMessage;
    QString databasePath;
    qint64 fileSize = 0;
    QDateTime lastModified;
    int schemaVersion = 0;
    qint64 partCount = 0;
    qint64 setCount = 0;
    qint64 minifigCount = 0;
    qint64 inventoryRecordCount = 0;
    qint64 inventoryPieceCount = 0;
    qint64 activeBuildCount = 0;
    qint64 archivedBuildCount = 0;
    qint64 activeManufacturerCount = 0;
    qint64 activeStorageLocationCount = 0;
    QString sqliteVersion;
    QString journalMode;
    bool foreignKeysEnabled = false;
    qint64 pageSize = 0;
    qint64 pageCount = 0;
    qint64 freelistPageCount = 0;
    bool walExists = false;
    qint64 walSize = 0;
    bool shmExists = false;
    qint64 shmSize = 0;
};

struct DatabaseIntegrityCheckResult
{
    DatabaseDiagnosticOutcome outcome = DatabaseDiagnosticOutcome::QueryFailure;
    QString errorMessage;
    qint64 issueCount = 0;
    QStringList representativeIssues;
    qint64 elapsedMilliseconds = 0;
};

struct DatabaseForeignKeyViolation
{
    QString table;
    qint64 rowId = 0;
    QString parentTable;
    int foreignKeyIndex = 0;
};

struct DatabaseForeignKeyCheckResult
{
    DatabaseDiagnosticOutcome outcome = DatabaseDiagnosticOutcome::QueryFailure;
    QString errorMessage;
    qint64 violationCount = 0;
    QVector<DatabaseForeignKeyViolation> representativeViolations;
    qint64 elapsedMilliseconds = 0;
};

class DatabaseStatusService
{
public:
    static DatabaseStatusSnapshot statusSnapshot(const QString& databasePath);
    static DatabaseIntegrityCheckResult interpretIntegrityResults(const QStringList& rows);
    static DatabaseIntegrityCheckResult runIntegrityCheck(const QString& databasePath);
    static DatabaseForeignKeyCheckResult runForeignKeyCheck(const QString& databasePath);
};
