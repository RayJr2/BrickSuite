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

// DatabaseManager.h

#pragma once

#include <QSqlDatabase>
#include <QString>

class DatabaseManager
{
public:
    enum class BackupFailure {
        None,
        Busy,
        Source,
        SourceHealth,
        SourceHealthCheck,
        Destination,
        Snapshot,
        Verification
    };

    struct VerifiedBackupResult {
        bool success = false;
        BackupFailure failure = BackupFailure::None;
        QString errorMessage;
        QString backupPath;
    };

    static DatabaseManager& instance();

    bool initialize();
    void close();

    QString databasePath() const;
    QSqlDatabase database() const;

    bool backupDatabase(const QString& backupPath, QString* errorMessage = nullptr);
    bool verifyDatabaseBackup(const QString& backupPath, QString* errorMessage = nullptr) const;
    bool restoreDatabase(const QString& backupPath, QString* errorMessage = nullptr);
    static VerifiedBackupResult createVerifiedBackup(const QString& sourceDatabasePath,
                                                      const QString& backupPath);

private:
    DatabaseManager();
    ~DatabaseManager();

    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;

    static bool createSnapshot(QSqlDatabase database, const QString& backupPath,
                               QString* errorMessage, BackupFailure* failure = nullptr);
    static bool verifyBackup(const QString& backupPath, QString* errorMessage);
    bool backupDatabaseUnlocked(const QString& backupPath, QString* errorMessage = nullptr);

    QSqlDatabase m_database;
};
