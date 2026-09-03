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

class QComboBox;
class QLabel;
class QSpinBox;
class QTextEdit;
class QDialogButtonBox;
class SessionStorageSelectionService;

class FoundInventoryDialog : public QDialog
{
    Q_OBJECT

public:
    explicit FoundInventoryDialog(int workspaceId,
                                  int partId,
                                  int colorId,
                                  SessionStorageSelectionService& sessionStorageSelectionService,
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
    SessionStorageSelectionService& m_sessionStorageSelectionService;

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
