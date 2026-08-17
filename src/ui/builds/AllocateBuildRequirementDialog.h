/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 *
 * This file is part of BrickSuite.
 *
 * BrickSuite is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, version 3 of the License.
 *
 * BrickSuite is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with BrickSuite. If not, see <https://www.gnu.org/licenses/>.
 */

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
    int m_quantityPulled = 0;
    int m_quantityRemaining = 0;

    bool m_isSpare = false;

    QLabel* m_partLabel = nullptr;
    QLabel* m_colorLabel = nullptr;
    QLabel* m_requiredLabel = nullptr;
    QLabel* m_pulledLabel = nullptr;
    QLabel* m_remainingLabel = nullptr;
    QLabel* m_summaryLabel = nullptr;

    QTableWidget* m_table = nullptr;

    QPushButton* m_saveButton = nullptr;
    QPushButton* m_cancelButton = nullptr;

    QList<AllocationRow> m_rows;
};