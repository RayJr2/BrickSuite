#pragma once

#include <QDialog>
#include <QList>

class QLabel;
class QPushButton;
class QSpinBox;
class QTableWidget;

class AllocateBuildRequirementDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AllocateBuildRequirementDialog(int workspaceId,
                                            int buildId,
                                            int requirementId,
                                            QWidget* parent = nullptr);

private:
    struct AllocationRow
    {
        int inventoryRecordId = 0;
        int storageLocationId = 0;

        int quantityOwned = 0;
        int otherBuildsAllocated = 0;
        int currentBuildAllocated = 0;

        QSpinBox* allocationSpin = nullptr;
    };

    bool loadRequirement();

    void loadInventory();

    void updateSummary();

    void saveAllocations();

    QString storageLocationName(int storageLocationId) const;

    int m_workspaceId = 0;
    int m_buildId = 0;
    int m_requirementId = 0;

    int m_partId = 0;
    int m_colorId = 0;

    int m_quantityRequired = 0;

    bool m_isSpare = false;

    QLabel* m_partLabel = nullptr;
    QLabel* m_colorLabel = nullptr;
    QLabel* m_requiredLabel = nullptr;
    QLabel* m_summaryLabel = nullptr;

    QTableWidget* m_table = nullptr;

    QPushButton* m_saveButton = nullptr;
    QPushButton* m_cancelButton = nullptr;

    QList<AllocationRow> m_rows;
};