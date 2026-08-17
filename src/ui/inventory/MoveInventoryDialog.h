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