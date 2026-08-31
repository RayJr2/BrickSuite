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

#include "../../import/InventoryImportTypes.h"

class QComboBox;
class QLabel;
class QPushButton;
class QTableWidget;
class QDialogButtonBox;

class InventoryImportPreviewDialog : public QDialog
{
    Q_OBJECT

public:
    explicit InventoryImportPreviewDialog(const InventoryImportPreview& preview,
                                          QWidget* parent = nullptr);

private:
    void populateSummary();
    void populateTable();
    void applyCompareFilter();

    void exportAppendCsv();
    void exportSubtractCsv();
    void exportDiffCsv(bool append);

    InventoryImportPreview m_preview;

    QLabel* m_scopeLabel = nullptr;
    QLabel* m_summaryLabel = nullptr;
    QComboBox* m_filterCombo = nullptr;
    QTableWidget* m_table = nullptr;
    QPushButton* m_exportAppendButton = nullptr;
    QPushButton* m_exportSubtractButton = nullptr;
    QDialogButtonBox* m_buttonBox = nullptr;
};
