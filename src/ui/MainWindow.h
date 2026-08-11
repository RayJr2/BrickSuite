#pragma once

#include <QMainWindow>

class QListWidget;
class QLineEdit;
class QTextEdit;
class QPushButton;
class WorkspaceContext;
class StorageWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(WorkspaceContext& workspaceContext, QWidget* parent = nullptr);

private slots:
    void addWorkspace();
    void workspaceSelected();

private:
    void loadWorkspaces();

    WorkspaceContext& m_workspaceContext;

    QListWidget* m_workspaceList = nullptr;
    QLineEdit* m_nameEdit = nullptr;
    QTextEdit* m_descriptionEdit = nullptr;
    QPushButton* m_addButton = nullptr;

    StorageWidget* m_storageWidget = nullptr;
};