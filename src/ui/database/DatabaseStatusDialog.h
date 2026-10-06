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

#include "../../services/database/DatabaseStatusService.h"

#include <QDialog>

class QLabel;
class QProgressBar;
class QPushButton;
class QWidget;

class DatabaseStatusDialog : public QDialog
{
    Q_OBJECT
public:
    explicit DatabaseStatusDialog(const QString& databasePath, QWidget* parent = nullptr);
    void presentIntegrityResult(const DatabaseIntegrityCheckResult& result);
    void presentForeignKeyResult(const DatabaseForeignKeyCheckResult& result);
    QString diagnosticSummary() const;

signals:
    void backupRequested();
    void restoreRequested();
    void applicationLogRequested();

private:
    void refresh();
    void runIntegrityCheck();
    void runForeignKeyCheck();
    void setBusy(bool busy, const QString& text = {});
    void applySnapshot(const DatabaseStatusSnapshot& snapshot);
    void showFailure(DatabaseDiagnosticOutcome outcome, const QString& details);
    void showRecoveryPanel(DatabaseDiagnosticOutcome outcome);
    void hideRecoveryPanel();
    void openDatabaseFolder();
    void copyDiagnosticSummary();
    QString m_databasePath;
    QLabel* m_databaseValues = nullptr;
    QLabel* m_dataValues = nullptr;
    QLabel* m_detailsValues = nullptr;
    QLabel* m_resultLabel = nullptr;
    QProgressBar* m_progress = nullptr;
    QPushButton* m_refreshButton = nullptr;
    QPushButton* m_integrityButton = nullptr;
    QPushButton* m_foreignKeyButton = nullptr;
    QWidget* m_recoveryPanel = nullptr;
    QLabel* m_recoveryHeading = nullptr;
    QLabel* m_recoveryText = nullptr;
    DatabaseStatusSnapshot m_snapshot;
    DatabaseDiagnosticOutcome m_lastOutcome = DatabaseDiagnosticOutcome::Healthy;
    QString m_lastCheckType;
    qint64 m_lastIssueCount = 0;
    qint64 m_lastElapsedMilliseconds = 0;
    QStringList m_lastDiagnosticRows;
    QDateTime m_lastCheckTime;
};
