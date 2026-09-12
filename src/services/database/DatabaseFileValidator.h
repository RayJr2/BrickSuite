#pragma once

#include "../../database/DatabaseSchema.h"

#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QString>
#include <QStringList>
#include <QUuid>

class DatabaseFileValidator
{
public:
    enum class Failure {
        None,
        Missing,
        Open,
        NotBrickSuite,
        UnsupportedSchema,
        Integrity,
        ForeignKeys,
        Query
    };

    struct Result {
        bool valid = false;
        Failure failure = Failure::None;
        int schemaVersion = 0;
        int issueCount = 0;
        QString message;
        QStringList diagnostics;
    };

    static Result validate(const QString& path)
    {
        constexpr int maximumDiagnostics = 20;
        auto appendBounded = [maximumDiagnostics](QStringList& diagnostics,
                                                   const QString& value) {
            if (diagnostics.size() < maximumDiagnostics)
                diagnostics.append(value.left(500));
        };

        Result result;
        const QString trimmedPath = path.trimmed();
        const QString candidate = QFileInfo(trimmedPath).absoluteFilePath();
        if (trimmedPath.isEmpty() || !QFileInfo::exists(candidate)) {
            result.failure = Failure::Missing;
            result.message = QStringLiteral("The database file does not exist.");
            return result;
        }

        const QString connectionName = QStringLiteral("BrickSuiteFileValidation_%1")
            .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
        {
            QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                                               connectionName);
            database.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY"));
            database.setDatabaseName(candidate);
            if (!database.open()) {
                result.failure = Failure::Open;
                result.message = QStringLiteral("The database could not be opened read-only: %1")
                                     .arg(database.lastError().text().left(500));
            } else {
                QSqlQuery version(database);
                if (!version.exec(QStringLiteral("SELECT version FROM schema_version LIMIT 1"))) {
                    result.failure = Failure::NotBrickSuite;
                    result.message = QStringLiteral("The file is not a recognizable BrickSuite database.");
                } else if (!version.next() || version.value(0).isNull()) {
                    result.failure = Failure::NotBrickSuite;
                    result.message = QStringLiteral("The BrickSuite schema version is missing.");
                } else {
                    bool integer = false;
                    result.schemaVersion = version.value(0).toInt(&integer);
                    if (!integer || result.schemaVersion != DatabaseSchema::CurrentSchemaVersion) {
                        result.failure = Failure::UnsupportedSchema;
                        result.message = QStringLiteral("Database schema version %1 is not supported; this BrickSuite build requires version %2.")
                                             .arg(result.schemaVersion)
                                             .arg(DatabaseSchema::CurrentSchemaVersion);
                    }
                }

                if (result.failure == Failure::None) {
                    QSqlQuery integrity(database);
                    if (!integrity.exec(QStringLiteral("PRAGMA integrity_check"))) {
                        result.failure = Failure::Query;
                        result.message = QStringLiteral("SQLite integrity_check could not run: %1")
                                             .arg(integrity.lastError().text().left(500));
                    } else {
                        while (integrity.next()) {
                            ++result.issueCount;
                            appendBounded(result.diagnostics, integrity.value(0).toString());
                        }
                        const bool healthy = result.issueCount == 1
                            && result.diagnostics.value(0).compare(QStringLiteral("ok"),
                                                                  Qt::CaseInsensitive) == 0;
                        if (!healthy) {
                            result.failure = Failure::Integrity;
                            result.message = result.issueCount == 0
                                ? QStringLiteral("SQLite integrity_check returned no result.")
                                : QStringLiteral("SQLite integrity_check reported %1 issue(s).")
                                      .arg(result.issueCount);
                        }
                    }
                }

                if (result.failure == Failure::None) {
                    result.issueCount = 0;
                    result.diagnostics.clear();
                    QSqlQuery foreignKeys(database);
                    if (!foreignKeys.exec(QStringLiteral("PRAGMA foreign_key_check"))) {
                        result.failure = Failure::Query;
                        result.message = QStringLiteral("SQLite foreign_key_check could not run: %1")
                                             .arg(foreignKeys.lastError().text().left(500));
                    } else {
                        while (foreignKeys.next()) {
                            ++result.issueCount;
                            appendBounded(result.diagnostics,
                                          QStringLiteral("table=%1 rowid=%2 parent=%3 constraint=%4")
                                              .arg(foreignKeys.value(0).toString(),
                                                   foreignKeys.value(1).toString(),
                                                   foreignKeys.value(2).toString(),
                                                   foreignKeys.value(3).toString()));
                        }
                        if (result.issueCount > 0) {
                            result.failure = Failure::ForeignKeys;
                            result.message = QStringLiteral("SQLite foreign_key_check reported %1 violation(s).")
                                                 .arg(result.issueCount);
                        }
                    }
                }
                database.close();
            }
        }
        QSqlDatabase::removeDatabase(connectionName);

        if (result.failure == Failure::None) {
            result.valid = true;
            result.message = QStringLiteral("The BrickSuite database is valid.");
        }
        return result;
    }
};
