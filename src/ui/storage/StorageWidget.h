#pragma once

#include <QWidget>

class WorkspaceContext;
class QTreeWidget;
class QPushButton;

class StorageWidget : public QWidget
{
    Q_OBJECT

public:
    explicit StorageWidget(
        WorkspaceContext& workspaceContext,
        QWidget* parent = nullptr);

private slots:
    void workspaceChanged(int workspaceId);
    void addLocation();
    void editLocation();
    void deactivateLocation();
    void reactivateLocation();

private:
    void loadStorageTree();

    WorkspaceContext& m_workspaceContext;

    QTreeWidget* m_tree = nullptr;
    QPushButton* m_addButton = nullptr;
    QPushButton* m_editButton = nullptr;
    QPushButton* m_deactivateButton = nullptr;
    QPushButton* m_reactivateButton = nullptr;
};