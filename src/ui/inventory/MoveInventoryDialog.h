#pragma once

#include <QDialog>

class WorkspaceContext;
class QLabel;
class QComboBox;
class QSpinBox;
class QDialogButtonBox;

class MoveInventoryDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MoveInventoryDialog(int inventoryRecordId,
                                 WorkspaceContext& workspaceContext,
                                 QWidget* parent = nullptr);

private slots:
    void moveInventory();

private:
    bool loadInventoryRecord();
    void loadStorageLocations();

    int m_inventoryRecordId = 0;
    int m_sourceStorageLocationId = 0;
    int m_availableQuantity = 0;

    WorkspaceContext& m_workspaceContext;

    QLabel* m_partLabel = nullptr;
    QLabel* m_currentStorageLabel = nullptr;
    QLabel* m_availableLabel = nullptr;

    QComboBox* m_destinationCombo = nullptr;
    QSpinBox* m_quantitySpin = nullptr;

    QDialogButtonBox* m_buttonBox = nullptr;
};