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

class QComboBox;
class QLabel;
class QPushButton;
class QSpinBox;
class QTableWidget;

class DisassembleSetDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DisassembleSetDialog(int buildId, QWidget* parent = nullptr);

private:
    struct RowData
    {
        int requirementId = 0;
        int partId = 0;
        int colorId = 0;

        int sourceQuantity = 0;
        bool isSpare = false;

        QSpinBox* quantitySpin = nullptr;
        QComboBox* destinationCombo = nullptr;
    };

    bool loadBuild();
    bool loadStorageLocations();
    bool loadRequirements();

    void applyDefaultDestination();
    void updateSummary();
    void disassembleSet();

    QString storagePath(int storageLocationId) const;

    void populateLocationCombo(QComboBox* combo);

    int m_buildId = 0;
    int m_workspaceId = 0;

    QString m_buildName;
    QString m_setNumber;

    QString m_inventoryMode;
    QString m_disassemblyLabel;

    QLabel* m_buildLabel = nullptr;
    QLabel* m_summaryLabel = nullptr;
    QLabel* m_statusLabel = nullptr;

    QComboBox* m_defaultDestinationCombo = nullptr;
    QPushButton* m_applyDefaultButton = nullptr;

    QTableWidget* m_table = nullptr;

    QPushButton* m_disassembleButton = nullptr;
    QPushButton* m_cancelButton = nullptr;

    struct LocationChoice
    {
        int id = 0;
        QString path;
    };

    QList<LocationChoice> m_locations;
    QList<RowData> m_rows;
};
