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

#include <QWidget>

class WorkspaceContext;
class QTreeWidget;
class QPushButton;

class StorageWidget : public QWidget
{
    Q_OBJECT

public:
    explicit StorageWidget(
        WorkspaceContext& workspaceContext,
        QWidget* parent = nullptr);

private slots:
    void workspaceChanged(int workspaceId);
    void addLocation();
    void editLocation();
    void deactivateLocation();
    void reactivateLocation();

private:
    void loadStorageTree();

    WorkspaceContext& m_workspaceContext;

    QTreeWidget* m_tree = nullptr;
    QPushButton* m_addButton = nullptr;
    QPushButton* m_editButton = nullptr;
    QPushButton* m_deactivateButton = nullptr;
    QPushButton* m_reactivateButton = nullptr;
};