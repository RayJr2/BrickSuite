#pragma once

#include <QDialog>

class QPlainTextEdit;
class QPushButton;
class QTimer;
class QCloseEvent;

class LogViewerDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LogViewerDialog(QWidget* parent = nullptr);

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void refreshLog();
    void clearLog();
    void openLogFolder();

    QPlainTextEdit* m_logEdit = nullptr;
    QPushButton* m_refreshButton = nullptr;
    QPushButton* m_clearButton = nullptr;
    QPushButton* m_openFolderButton = nullptr;
    QTimer* m_refreshTimer = nullptr;
};
