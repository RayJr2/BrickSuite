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
class QLineEdit;
class QComboBox;
class QTextEdit;
class QPushButton;

class EditBuildDialog : public QDialog
{
    Q_OBJECT

public:
    explicit EditBuildDialog(int buildId, QWidget* parent = nullptr);

private:
    bool loadBuild();
    void saveBuild();

    int m_buildId = 0;

    QLabel* m_typeLabel = nullptr;
    QLabel* m_setNumberLabel = nullptr;
    QLabel* m_inventoryModeLabel = nullptr;

    QLineEdit* m_nameEdit = nullptr;
    QComboBox* m_statusCombo = nullptr;
    QComboBox* m_manufacturerCombo = nullptr;
    QTextEdit* m_notesEdit = nullptr;

    QPushButton* m_saveButton = nullptr;
};