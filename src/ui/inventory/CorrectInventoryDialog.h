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
class QLineEdit;
class QSpinBox;
class QDialogButtonBox;
class QCompleter;
class QStandardItemModel;
class QTimer;
class QModelIndex;

class CorrectInventoryDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CorrectInventoryDialog(int inventoryRecordId,
                                    WorkspaceContext& workspaceContext,
                                    QWidget* parent = nullptr);

private slots:
    void updatePartSearch();
    void applySelectedPart(const QModelIndex& index);
    void resolveEnteredPart();
    void saveCorrection();

private:
    bool loadInventoryRecord();
    void setResolvedPart(int partId, const QString& displayText);
    void updateSaveButtonState();

    int m_inventoryRecordId = 0;
    int m_originalPartId = 0;
    int m_replacementPartId = 0;
    int m_currentQuantity = 0;

    WorkspaceContext& m_workspaceContext;

    QLabel* m_currentPartLabel = nullptr;
    QLabel* m_contextLabel = nullptr;
    QLabel* m_resolvedLabel = nullptr;
    QLineEdit* m_partSearchEdit = nullptr;
    QSpinBox* m_quantitySpin = nullptr;
    QLineEdit* m_notesEdit = nullptr;
    QDialogButtonBox* m_buttonBox = nullptr;
    QCompleter* m_completer = nullptr;
    QStandardItemModel* m_searchModel = nullptr;
    QTimer* m_searchTimer = nullptr;
};
