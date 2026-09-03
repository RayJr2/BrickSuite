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

signals:
    void backupRequested();
    void restoreRequested();

private:
    void refresh();
    void runIntegrityCheck();
    void runForeignKeyCheck();
    void setBusy(bool busy, const QString& text = {});
    void applySnapshot(const DatabaseStatusSnapshot& snapshot);
    void showFailure(DatabaseDiagnosticOutcome outcome, const QString& details);
    bool event(QEvent* event) override;

    QString m_databasePath;
    QLabel* m_databaseValues = nullptr;
    QLabel* m_dataValues = nullptr;
    QLabel* m_detailsValues = nullptr;
    QLabel* m_resultLabel = nullptr;
    QProgressBar* m_progress = nullptr;
    QPushButton* m_refreshButton = nullptr;
    QPushButton* m_integrityButton = nullptr;
    QPushButton* m_foreignKeyButton = nullptr;
};
