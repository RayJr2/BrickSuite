#pragma once

#include <QDialog>
#include <QList>
#include <QModelIndex>

class WorkspaceContext;
class QLabel;
class QComboBox;
class QSpinBox;
class QDialogButtonBox;
class QCheckBox;
class RebrickableApiClient;
class QLineEdit;
class QCompleter;
class QStandardItemModel;
class QTimer;

class AddInventoryDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AddInventoryDialog(int partId,
                                WorkspaceContext& workspaceContext,
                                QWidget* parent = nullptr);

    explicit AddInventoryDialog(WorkspaceContext& workspaceContext, QWidget* parent = nullptr);

    bool inventoryWasAdded() const;

private slots:
    void addInventory();
    void showAllColorsToggled(bool checked);

private:
    void loadPart();
    void loadAllColors();
    void loadKnownColors();
    void applyKnownColors(int preferredColorId = 0);
    void loadStorageLocations();

    void initializeUi();
    void configureForSelectedPart();
    void clearPartSelection();
    void updatePartSearch();
    void selectSearchResult(const QModelIndex& index);
    void updateAddButtonState();

    int m_partId = 0;

    WorkspaceContext& m_workspaceContext;

    QLabel* m_partLabel = nullptr;

    QComboBox* m_colorCombo = nullptr;
    QComboBox* m_storageCombo = nullptr;
    QComboBox* m_conditionCombo = nullptr;
    QComboBox* m_ownershipCombo = nullptr;

    QSpinBox* m_quantitySpin = nullptr;

    QDialogButtonBox* m_buttonBox = nullptr;

    QCheckBox* m_showAllColorsCheck = nullptr;

    RebrickableApiClient* m_rebrickableApiClient = nullptr;

    QList<int> m_knownRebrickableColorIds;

    QString m_partNumber;

    QLineEdit* m_partSearchEdit = nullptr;
    QCompleter* m_partCompleter = nullptr;
    QStandardItemModel* m_partSearchModel = nullptr;
    QTimer* m_partSearchTimer = nullptr;

    QCheckBox* m_keepOpenCheck = nullptr;

    bool m_quickEntryMode = false;
    bool m_inventoryWasAdded = false;

    int m_quickEntryColorId = 0;
};
