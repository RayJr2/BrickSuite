#pragma once

#include <QMainWindow>
class QCloseEvent;

class WorkspaceContext;
class QListWidget;
class QLineEdit;
class QTextEdit;
class QPushButton;
class QTabWidget;
class QWidget;

class StorageWidget;
class PartsCatalogWidget;
class MyInventoryWidget;
class BuildsWidget;
class SetsCatalogWidget;
class BackgroundPartColorImageCacheService;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(WorkspaceContext& workspaceContext, QWidget* parent = nullptr);

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void addWorkspace();
    void workspaceSelected();

private:
    void loadWorkspaces();
    QWidget* createWorkspaceTab();

    WorkspaceContext& m_workspaceContext;

    QTabWidget* m_tabWidget = nullptr;

    QListWidget* m_workspaceList = nullptr;
    QLineEdit* m_nameEdit = nullptr;
    QTextEdit* m_descriptionEdit = nullptr;
    QPushButton* m_addButton = nullptr;

    StorageWidget* m_storageWidget = nullptr;
    PartsCatalogWidget* m_partsCatalogWidget = nullptr;
    MyInventoryWidget* m_myInventoryWidget = nullptr;
    BuildsWidget* m_buildsWidget = nullptr;
    SetsCatalogWidget* m_setsCatalogWidget = nullptr;

    BackgroundPartColorImageCacheService* m_backgroundPartColorImageCacheService = nullptr;
};