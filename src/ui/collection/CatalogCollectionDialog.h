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

#include "../../models/CollectionItem.h"

#include <QDialog>

class QComboBox;
class QLineEdit;
class QTextEdit;

class CatalogCollectionDialog : public QDialog
{
    Q_OBJECT
public:
    CatalogCollectionDialog(int workspaceId, CollectionItemType type, int catalogId,
                            const QString& reference, const QString& name,
                            QWidget* parent = nullptr, int sourceBuildId = 0);

    int createdCollectionItemId() const;

private:
    void createItem();

    int m_workspaceId = 0;
    CollectionItemType m_type = CollectionItemType::Invalid;
    int m_catalogId = 0;
    int m_createdItemId = 0;
    int m_sourceBuildId = 0;
    QComboBox* m_stateCombo = nullptr;
    QComboBox* m_conditionCombo = nullptr;
    QComboBox* m_completenessCombo = nullptr;
    QComboBox* m_locationCombo = nullptr;
    QLineEdit* m_nicknameEdit = nullptr;
    QTextEdit* m_notesEdit = nullptr;
};
