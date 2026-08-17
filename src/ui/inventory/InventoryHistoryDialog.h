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
#include <QHash>

class WorkspaceContext;
class QTableWidget;
class QLabel;

class InventoryHistoryDialog : public QDialog
{
    Q_OBJECT

public:
    explicit InventoryHistoryDialog(int partId,
                                    int colorId,
                                    WorkspaceContext& workspaceContext,
                                    QWidget* parent = nullptr);

private:
    void loadHeader();
    void loadHistory();
    void buildStoragePathCache();

    QString storagePathForId(int storageLocationId) const;

    int m_partId = 0;
    int m_colorId = 0;

    WorkspaceContext& m_workspaceContext;

    QLabel* m_titleLabel = nullptr;
    QTableWidget* m_table = nullptr;

    QHash<int, QString> m_storagePathById;
};