#pragma once

#include <QDialog>

class WorkspaceContext;
class QLabel;
class QComboBox;
class QSpinBox;
class QDialogButtonBox;

class AddInventoryDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AddInventoryDialog(int partId,
                                WorkspaceContext& workspaceContext,
                                QWidget* parent = nullptr);

private slots:
    void addInventory();

private:
    void loadPart();
    void loadColors();
    void loadStorageLocations();

    int m_partId = 0;

    WorkspaceContext& m_workspaceContext;

    QLabel* m_partLabel = nullptr;

    QComboBox* m_colorCombo = nullptr;
    QComboBox* m_storageCombo = nullptr;
    QComboBox* m_conditionCombo = nullptr;
    QComboBox* m_ownershipCombo = nullptr;

    QSpinBox* m_quantitySpin = nullptr;

    QDialogButtonBox* m_buttonBox = nullptr;
};
