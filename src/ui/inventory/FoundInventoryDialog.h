#pragma once

#include <QDialog>

class QComboBox;
class QLabel;
class QSpinBox;
class QTextEdit;
class QDialogButtonBox;

class FoundInventoryDialog : public QDialog
{
    Q_OBJECT

public:
    explicit FoundInventoryDialog(int workspaceId,
                                  int partId,
                                  int colorId,
                                  QWidget* parent = nullptr);

private slots:
    void returnFoundInventory();

private:
    bool loadLostInventory();
    void loadStorageLocations();

    QString storagePathForId(int storageLocationId) const;

    int m_workspaceId = 0;
    int m_partId = 0;
    int m_colorId = 0;

    int m_outstandingQuantity = 0;
    int m_lastStorageLocationId = 0;

    QLabel* m_partLabel = nullptr;
    QLabel* m_colorLabel = nullptr;
    QLabel* m_outstandingLabel = nullptr;

    QSpinBox* m_quantityFoundSpin = nullptr;

    QComboBox* m_storageCombo = nullptr;
    QComboBox* m_conditionCombo = nullptr;
    QComboBox* m_ownershipCombo = nullptr;

    QTextEdit* m_notesEdit = nullptr;

    QDialogButtonBox* m_buttonBox = nullptr;
};
