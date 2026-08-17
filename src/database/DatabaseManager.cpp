/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 *
 * This file is part of BrickSuite.
 *
 * BrickSuite is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, version 3 of the License.
 *
 * BrickSuite is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with BrickSuite. If not, see <https://www.gnu.org/licenses/>.
 */

// DatabaseManager.cpp

#include "DatabaseManager.h"
#include "DatabaseSchema.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlError>
#include <QStandardPaths>
#include <QUuid>
#include <qsqlquery.h>

DatabaseManager& DatabaseManager::instance()
{
    static DatabaseManager instance;
    return instance;
}

DatabaseManager::DatabaseManager() = default;

DatabaseManager::~DatabaseManager()
{
    close();
}

bool DatabaseManager::initialize()
{
    const QString dataPath =
        QStandardPaths::writableLocation(
            QStandardPaths::AppLocalDataLocation);

    QDir directory;

    if (!directory.mkpath(dataPath))
    {
        qCritical() << "Unable to create application data directory:"
                    << dataPath;

        return false;
    }

    const QString databasePath = this->databasePath();

    if (QSqlDatabase::contains()) {
        m_database = QSqlDatabase::database();
    } else {
        m_database = QSqlDatabase::addDatabase("QSQLITE");
    }

    m_database.setDatabaseName(databasePath);

    if (!m_database.open()) {
        qCritical() << "Unable to open BrickSuite database:" << m_database.lastError().text();

        return false;
    }

    QSqlQuery pragmaQuery(m_database);

    if (!pragmaQuery.exec("PRAGMA foreign_keys = ON")) {
        qCritical() << "Unable to enable SQLite foreign keys:" << pragmaQuery.lastError().text();

        return false;
    }

    qInfo() << "BrickSuite database:" << databasePath;

    if (!DatabaseSchema::initialize(m_database)) {
        qCritical() << "Unable to initialize BrickSuite database schema.";
        return false;
    }

    return true;
}

void DatabaseManager::close()
{
    if (m_database.isOpen())
        m_database.close();
}

QSqlDatabase DatabaseManager::database() const
{
    return m_database;
}

bool DatabaseManager::backupDatabase(const QString& backupPath, QString* errorMessage)
{
    auto setError = [errorMessage](const QString& message) {
        if (errorMessage)
            *errorMessage = message;
    };

    if (!m_database.isOpen()) {
        const QString message = "The BrickSuite database is not open.";
        setError(message);
        qWarning() << "Database backup rejected:" << message;

        return false;
    }

    const QString trimmedPath = backupPath.trimmed();

    qInfo() << "Database backup requested:" << trimmedPath;

    if (trimmedPath.isEmpty()) {
        setError("No backup file was selected.");

        return false;
    }

    const QFileInfo backupFileInfo(trimmedPath);

    QDir backupDirectory = backupFileInfo.dir();

    if (!backupDirectory.exists()) {
        if (!backupDirectory.mkpath(".")) {
            const QString message
                = QString("Unable to create the backup directory: %1")
                      .arg(backupDirectory.absolutePath());
            setError(QString("Unable to create the backup directory:\n%1")
                         .arg(backupDirectory.absolutePath()));
            qCritical() << "Database backup failed:" << message;

            return false;
        }
    }

    //
    // SQLite VACUUM INTO requires the destination
    // file not to already contain a database.
    //
    // Create the new snapshot as a temporary file first.
    // This means an existing backup isn't destroyed if
    // the SQLite backup operation fails.
    //
    const QString temporaryPath = trimmedPath + ".tmp";

    if (QFile::exists(temporaryPath)) {
        if (!QFile::remove(temporaryPath)) {
            setError(QString("Unable to remove an existing temporary "
                             "backup file:\n%1")
                         .arg(temporaryPath));

            return false;
        }
    }

    QString escapedTemporaryPath = temporaryPath;

    //
    // Escape single quotes for the SQLite string literal.
    //
    escapedTemporaryPath.replace("'", "''");

    QSqlQuery query(m_database);

    const QString sql = QString("VACUUM INTO '%1'").arg(escapedTemporaryPath);

    if (!query.exec(sql)) {
        QFile::remove(temporaryPath);

        const QString message = query.lastError().text();
        setError(
            QString("Unable to create the database backup.\n\n%1").arg(message));
        qCritical() << "Database backup VACUUM INTO failed:" << message;

        return false;
    }

    if (!QFile::exists(temporaryPath)) {
        setError("SQLite completed the backup operation, "
                 "but the backup file was not created.");
        qCritical() << "Database backup failed: SQLite reported success but "
                       "the temporary backup file was not created."
                    << "TemporaryPath:" << temporaryPath;

        return false;
    }

    //
    // Only replace an existing destination after
    // SQLite has successfully created the new snapshot.
    //
    if (QFile::exists(trimmedPath)) {
        if (!QFile::remove(trimmedPath)) {
            QFile::remove(temporaryPath);

            setError(QString("The new backup was created, but the "
                             "existing destination file could not be replaced:\n%1")
                         .arg(trimmedPath));

            return false;
        }
    }

    if (!QFile::rename(temporaryPath, trimmedPath)) {
        QFile::remove(temporaryPath);

        setError(QString("The database snapshot was created, "
                         "but it could not be renamed to:\n%1")
                     .arg(trimmedPath));

        return false;
    }

    qInfo() << "BrickSuite database backup created:" << trimmedPath;

    return true;
}

bool DatabaseManager::verifyDatabaseBackup(const QString& backupPath, QString* errorMessage) const
{
    auto setError = [errorMessage](const QString& message) {
        if (errorMessage)
            *errorMessage = message;
    };

    const QString trimmedPath = backupPath.trimmed();

    if (trimmedPath.isEmpty()) {
        setError("No backup file was specified.");

        return false;
    }

    if (!QFile::exists(trimmedPath)) {
        setError(QString("The backup file does not exist:\n%1").arg(trimmedPath));
        qWarning() << "Database backup verification rejected: file does not exist:"
                   << trimmedPath;

        return false;
    }

    const QString connectionName = QString("BrickSuiteBackupVerify_%1")
                                       .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));

    bool verified = false;

    QString verificationError;

    {
        QSqlDatabase backupDatabase = QSqlDatabase::addDatabase("QSQLITE", connectionName);

        backupDatabase.setDatabaseName(trimmedPath);

        if (!backupDatabase.open()) {
            verificationError = QString("Unable to open the backup database.\n\n%1")
                                    .arg(backupDatabase.lastError().text());
        } else {
            //
            // SQLite integrity check.
            //
            QSqlQuery integrityQuery(backupDatabase);

            if (!integrityQuery.exec("PRAGMA integrity_check")) {
                verificationError = QString("Unable to run SQLite integrity check.\n\n%1")
                                        .arg(integrityQuery.lastError().text());
            } else if (!integrityQuery.next()) {
                verificationError = "SQLite integrity check returned no result.";
            } else if (integrityQuery.value(0).toString().compare("ok", Qt::CaseInsensitive) != 0) {
                verificationError = QString("SQLite integrity check failed.\n\n%1")
                                        .arg(integrityQuery.value(0).toString());
            } else {
                //
                // Check BrickSuite schema version.
                //
                QSqlQuery versionQuery(backupDatabase);

                if (!versionQuery.exec("SELECT version "
                                       "FROM schema_version "
                                       "LIMIT 1")) {
                    verificationError = QString("Unable to read the backup schema version.\n\n%1")
                                            .arg(versionQuery.lastError().text());
                } else if (!versionQuery.next()) {
                    verificationError = "The backup does not contain a schema version.";
                } else {
                    const int version = versionQuery.value(0).toInt();

                    if (version != DatabaseSchema::CurrentSchemaVersion) {
                        verificationError = QString("Backup schema version %1 "
                                                    "does not match BrickSuite "
                                                    "schema version %2.")
                                                .arg(version)
                                                .arg(DatabaseSchema::CurrentSchemaVersion);
                    } else {
                        verified = true;
                    }
                }
            }

            backupDatabase.close();
        }
    }

    //
    // All QSqlDatabase / QSqlQuery objects that used
    // the connection are now out of scope.
    //
    QSqlDatabase::removeDatabase(connectionName);

    if (!verified) {
        setError(verificationError);
        qWarning() << "Database backup verification failed:"
                   << trimmedPath
                   << verificationError;

        return false;
    }

    qInfo() << "Database backup verified:" << trimmedPath;

    return true;
}

QString DatabaseManager::databasePath() const
{
    const QString dataPath = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);

    return QDir(dataPath).filePath("BrickSuite.db");
}

bool DatabaseManager::restoreDatabase(const QString& backupPath, QString* errorMessage)
{
    auto setError = [errorMessage](const QString& message) {
        if (errorMessage)
            *errorMessage = message;
    };

    const QString trimmedBackupPath = backupPath.trimmed();

    qInfo() << "Database restore requested:" << trimmedBackupPath;

    if (trimmedBackupPath.isEmpty()) {
        setError("No backup file was selected.");

        return false;
    }

    if (!QFile::exists(trimmedBackupPath)) {
        setError(QString("The selected backup file does not exist:\n%1").arg(trimmedBackupPath));
        qWarning() << "Database restore rejected: selected backup does not exist:"
                   << trimmedBackupPath;

        return false;
    }

    //
    // Step 1:
    // Verify selected backup before touching
    // the live database.
    //
    QString verificationError;

    if (!verifyDatabaseBackup(trimmedBackupPath, &verificationError)) {
        setError(QString("The selected database backup is not valid.\n\n%1").arg(verificationError));
        qWarning() << "Database restore rejected during verification:"
                   << verificationError;

        return false;
    }

    const QString liveDatabasePath = databasePath();

    const QFileInfo liveInfo(liveDatabasePath);

    const QString safetyBackupPath = liveInfo.dir().filePath(
        QString("BrickSuite_PreRestore_%1.db")
            .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd_HHmmss")));

    //
    // Step 2:
    // Create safety backup of current live DB.
    //
    QString backupError;

    if (!backupDatabase(safetyBackupPath, &backupError)) {
        setError(QString("Unable to create the automatic pre-restore "
                         "safety backup.\n\n%1")
                     .arg(backupError));
        qCritical() << "Database restore stopped: unable to create safety backup:"
                    << backupError;

        return false;
    }

    //
    // Step 3:
    // Verify safety backup too.
    //
    QString safetyVerificationError;

    if (!verifyDatabaseBackup(safetyBackupPath, &safetyVerificationError)) {
        setError(QString("The automatic pre-restore backup was created "
                         "but failed verification.\n\n%1")
                     .arg(safetyVerificationError));

        return false;
    }

    //
    // Step 4:
    // Close the live database before replacing it.
    //
    close();

    //
    // Step 5:
    // Replace the live DB with the selected backup.
    //
    const QString temporaryRestorePath = liveDatabasePath + ".restore_tmp";

    if (QFile::exists(temporaryRestorePath)) {
        QFile::remove(temporaryRestorePath);
    }

    if (!QFile::copy(trimmedBackupPath, temporaryRestorePath)) {
        //
        // Reopen original DB before returning.
        //
        initialize();

        setError(QString("Unable to copy the selected backup "
                         "into the BrickSuite data folder.\n\n%1")
                     .arg(trimmedBackupPath));
        qCritical() << "Database restore failed while copying selected backup:"
                    << trimmedBackupPath;

        return false;
    }

    //
    // Keep the existing database until the copied
    // restore file is safely in place.
    //
    const QString oldDatabasePath = liveDatabasePath + ".old";

    if (QFile::exists(oldDatabasePath)) {
        QFile::remove(oldDatabasePath);
    }

    if (QFile::exists(liveDatabasePath)) {
        if (!QFile::rename(liveDatabasePath, oldDatabasePath)) {
            QFile::remove(temporaryRestorePath);

            initialize();

            setError("Unable to move the current BrickSuite "
                     "database out of the way.");
            qCritical() << "Database restore failed moving current live database aside."
                        << "LiveDatabase:" << liveDatabasePath
                        << "PreviousDatabase:" << oldDatabasePath;

            return false;
        }
    }

    if (!QFile::rename(temporaryRestorePath, liveDatabasePath)) {
        //
        // Put the original DB back.
        //
        if (QFile::exists(oldDatabasePath)) {
            QFile::rename(oldDatabasePath, liveDatabasePath);
        }

        initialize();

        setError("Unable to install the selected backup "
                 "as the live BrickSuite database.");
        qCritical() << "Database restore failed installing selected backup."
                    << "SelectedBackup:" << trimmedBackupPath
                    << "LiveDatabase:" << liveDatabasePath;

        return false;
    }

    //
    // Step 6:
    // Reopen and initialize restored DB.
    //
    if (!initialize()) {
        //
        // Step 7:
        // Automatic rollback to original live DB.
        //
        close();

        QFile::remove(liveDatabasePath);

        if (QFile::exists(oldDatabasePath)) {
            QFile::rename(oldDatabasePath, liveDatabasePath);
        }

        if (!initialize()) {
            setError(QString("Restore failed, and BrickSuite could not "
                             "reopen either the restored database or "
                             "the original database.\n\n"
                             "A verified pre-restore backup exists at:\n%1")
                         .arg(safetyBackupPath));

            return false;
        }

        setError(QString("The selected backup could not be opened as "
                         "the live BrickSuite database.\n\n"
                         "The original database was restored.\n\n"
                         "Safety backup:\n%1")
                     .arg(safetyBackupPath));

        return false;
    }

    //
    // Step 8:
    // Success. Remove temporary original copy.
    //
    if (QFile::exists(oldDatabasePath)) {
        QFile::remove(oldDatabasePath);
    }

    qInfo() << "BrickSuite database restored from:" << trimmedBackupPath;

    qInfo() << "Pre-restore safety backup:" << safetyBackupPath;

    return true;
}