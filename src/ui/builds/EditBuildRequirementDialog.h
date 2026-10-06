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
class QCheckBox;
class QLineEdit;
class QComboBox;
class QPushButton;

class EditBuildRequirementDialog : public QDialog
{
    Q_OBJECT

public:
    explicit EditBuildRequirementDialog(int requirementId, QWidget* parent = nullptr);

private:
    void loadRequirement();
    void loadColors();
    void resetToOriginal();
    void saveRequirement();

    int m_requirementId = 0;
    int m_originalPartId = 0;
    int m_originalColorId = 0;

    QLabel* m_originalPartLabel = nullptr;
    QLabel* m_originalColorLabel = nullptr;

    QLineEdit* m_usePartEdit = nullptr;
    QComboBox* m_useColorCombo = nullptr;
    QPushButton* m_resetButton = nullptr;

    QSpinBox* m_quantitySpin = nullptr;
    QCheckBox* m_spareCheck = nullptr;
};
