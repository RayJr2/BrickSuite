#pragma once

#include <QDialog>
#include <QList>

class WorkspaceContext;
class QLabel;
class QComboBox;
class QSpinBox;
class QDialogButtonBox;
class QCheckBox;
class RebrickableApiClient;

class EditInventoryDialog : public QDialog
{
    Q_OBJECT

public:
    explicit EditInventoryDialog(int inventoryRecordId,
                                 WorkspaceContext& workspaceContext,
                                 QWidget* parent = nullptr);

private slots:
    void saveChanges();
    void showAllColorsToggled(bool checked);

private:
    bool loadInventoryRecord();

    void loadAllColors();
    void loadKnownColors();
    void applyKnownColors();

    void loadConditionOptions();
    void loadOwnershipOptions();

    int m_inventoryRecordId = 0;
    int m_partId = 0;

    WorkspaceContext& m_workspaceContext;

    QLabel* m_partLabel = nullptr;

    QComboBox* m_colorCombo = nullptr;
    QComboBox* m_conditionCombo = nullptr;
    QComboBox* m_ownershipCombo = nullptr;

    QSpinBox* m_quantitySpin = nullptr;

    QDialogButtonBox* m_buttonBox = nullptr;

    QCheckBox* m_showAllColorsCheck = nullptr;

    RebrickableApiClient* m_rebrickableApiClient = nullptr;

    QList<int> m_knownRebrickableColorIds;

    QString m_partNumber;

    int m_originalColorId = 0;

    int m_storageLocationId = 0;
};
