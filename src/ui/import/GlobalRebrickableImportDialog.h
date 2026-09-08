#pragma once

#include "../../import/global/RebrickableImportTypes.h"

#include <QDialog>

class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;

class GlobalRebrickableImportDialog : public QDialog
{
    Q_OBJECT

public:
    explicit GlobalRebrickableImportDialog(QWidget* parent = nullptr);

private:
    void selectFolder();
    void rescan();
    void displayPlan(const RebrickableImportPlan& plan);

    QLineEdit* m_directoryEdit = nullptr;
    QTableWidget* m_table = nullptr;
    QLabel* m_progressLabel = nullptr;
    QLabel* m_summaryLabel = nullptr;
    QPushButton* m_browseButton = nullptr;
    QPushButton* m_rescanButton = nullptr;
    QPushButton* m_importButton = nullptr;
    bool m_scanRunning = false;
};
