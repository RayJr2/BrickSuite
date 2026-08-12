#pragma once

#include <QDialog>
#include <QHash>

class WorkspaceContext;
class QTableWidget;
class QLabel;

class InventoryHistoryDialog : public QDialog
{
    Q_OBJECT

public:
    explicit InventoryHistoryDialog(int partId,
                                    int colorId,
                                    WorkspaceContext& workspaceContext,
                                    QWidget* parent = nullptr);

private:
    void loadHeader();
    void loadHistory();
    void buildStoragePathCache();

    QString storagePathForId(int storageLocationId) const;

    int m_partId = 0;
    int m_colorId = 0;

    WorkspaceContext& m_workspaceContext;

    QLabel* m_titleLabel = nullptr;
    QTableWidget* m_table = nullptr;

    QHash<int, QString> m_storagePathById;
};