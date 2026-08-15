#pragma once

#include <QDialog>

class WorkspaceContext;
class QLabel;
class QTableWidget;

class LostInventoryDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LostInventoryDialog(WorkspaceContext& workspaceContext, QWidget* parent = nullptr);

private:
    void loadLostInventory();

    WorkspaceContext& m_workspaceContext;

    QTableWidget* m_table = nullptr;
    QLabel* m_statusLabel = nullptr;
};