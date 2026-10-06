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
class QLineEdit;
class QTextEdit;
class QCheckBox;

class CollectionItemDialog : public QDialog
{
    Q_OBJECT
public:
    explicit CollectionItemDialog(int collectionItemId, QWidget* parent = nullptr);

signals:
    void itemChanged();

private:
    void save();
    int m_itemId = 0;
    QCheckBox* m_allowPartsSourceCheck = nullptr;
    QComboBox* m_stateCombo = nullptr;
    QComboBox* m_conditionCombo = nullptr;
    QComboBox* m_completenessCombo = nullptr;
    QComboBox* m_locationCombo = nullptr;
    QLineEdit* m_nicknameEdit = nullptr;
    QTextEdit* m_notesEdit = nullptr;
};
