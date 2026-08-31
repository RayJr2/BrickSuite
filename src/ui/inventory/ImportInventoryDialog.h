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

class WorkspaceContext;
class QLineEdit;
class QComboBox;
class QPushButton;
class QDialogButtonBox;
class QLabel;

class ImportInventoryDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ImportInventoryDialog(WorkspaceContext& workspaceContext, QWidget* parent = nullptr);

private slots:
    void browseForFile();
    void importFile();

private:
    void loadStorageLocations();
    void suggestStorageFromFileName(const QString& filePath);
    InventoryImportSource selectedImportSource() const;
    InventoryImportSource detectImportSource(const QString& filePath) const;
    void refreshImportFormatState();
    static QString normalizedStorageKey(const QString& text);

    WorkspaceContext& m_workspaceContext;

    QLineEdit* m_fileEdit = nullptr;
    QPushButton* m_browseButton = nullptr;
    QComboBox* m_formatCombo = nullptr;
    QLabel* m_formatStatusLabel = nullptr;

    QComboBox* m_operationCombo = nullptr;
    QComboBox* m_storageCombo = nullptr;
    QComboBox* m_conditionCombo = nullptr;
    QComboBox* m_ownershipCombo = nullptr;

    QDialogButtonBox* m_buttonBox = nullptr;
};