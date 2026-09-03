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
