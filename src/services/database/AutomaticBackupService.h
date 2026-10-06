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

#pragma once

#include "../../database/DatabaseManager.h"
#include "AutomaticBackupRetention.h"

#include <QObject>
#include <QString>
#include <functional>

class QThread;
class QTimer;

struct AutomaticBackupSourceHealthResult
{
    bool healthy = false;
    bool confirmedDatabaseProblems = false;
    DatabaseManager::BackupFailure failure = DatabaseManager::BackupFailure::SourceHealthCheck;
    QString errorMessage;
};

class AutomaticBackupService : public QObject
{
    Q_OBJECT

public:
    using HealthCheck = std::function<AutomaticBackupSourceHealthResult(const QString&)>;

    explicit AutomaticBackupService(QObject* parent = nullptr,
                                    const QString& sourceDatabasePath = QString(),
                                    const AutomaticBackupRetention::RemoveFile& removeFile = {},
                                    const HealthCheck& healthCheck = {});
    ~AutomaticBackupService() override;

    void start();
    void stop();
    void reloadPolicy();
    bool isRunning() const;
    void requestBackupNow(const QString& backupRoot, int retentionCount);

public slots:
    void requestBackup(bool bypassScheduledRetry = false);

signals:
    void backupStarted(const QString& destination);
    void backupSucceeded(const QString& destination);
    void backupFailed(DatabaseManager::BackupFailure failure, const QString& message);
    void retentionWarning(const QString& message);
    void stateChanged();

private:
    void checkDue();
    void startBackup(const QString& root, int retentionCount, bool bypassScheduledRetry);
    void scheduleNextCheck();
    void finishBackup(const DatabaseManager::VerifiedBackupResult& result,
                      const AutomaticBackupRetentionResult& retention,
                      const QDateTime& attemptedUtc, qint64 elapsedMilliseconds);
    static AutomaticBackupSourceHealthResult validateSourceHealth(const QString& databasePath);

    QTimer* m_timer = nullptr;
    QThread* m_worker = nullptr;
    bool m_started = false;
    bool m_policyCheckPending = false;
    QString m_sourceDatabasePath;
    AutomaticBackupRetention::RemoveFile m_removeFile;
    HealthCheck m_healthCheck;
};
