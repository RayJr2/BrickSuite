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

class QLabel;
class QSpinBox;
class QTextEdit;
class QDialogButtonBox;

class MarkLostInventoryDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MarkLostInventoryDialog(int inventoryRecordId, QWidget* parent = nullptr);

private slots:
    void markLost();

private:
    bool loadInventory();

    int m_inventoryRecordId = 0;

    int m_currentQuantity = 0;

    QLabel* m_partLabel = nullptr;
    QLabel* m_colorLabel = nullptr;
    QLabel* m_locationLabel = nullptr;
    QLabel* m_currentQuantityLabel = nullptr;

    QSpinBox* m_quantityLostSpin = nullptr;

    QTextEdit* m_notesEdit = nullptr;

    QDialogButtonBox* m_buttonBox = nullptr;
};
