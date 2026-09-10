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
#include <QPointer>
#include <QString>

class WorkspaceContext;
class SessionStorageSelectionService;
class QLabel;
class QTableWidget;
class RemoteReadApplicationServices;
class RemoteInventoryMutationApplicationService;
class RemoteInventoryMutationDialog;

class LostInventoryDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LostInventoryDialog(WorkspaceContext& workspaceContext,
                                 SessionStorageSelectionService& sessionStorageSelectionService,
                                 QWidget* parent = nullptr);
    LostInventoryDialog(WorkspaceContext& workspaceContext,
        SessionStorageSelectionService& sessionStorageSelectionService,
        RemoteReadApplicationServices& remoteReads,
        RemoteInventoryMutationApplicationService& remoteMutations,
        const QHash<int, QString>& storagePaths, QWidget* parent = nullptr);
    void refreshRemoteStoragePaths(const QHash<int, QString>& storagePaths);

private:
    void loadLostInventory();
    void initializeUi();
    void loadRemoteLostInventory();

    WorkspaceContext& m_workspaceContext;
    SessionStorageSelectionService& m_sessionStorageSelectionService;

    QTableWidget* m_table = nullptr;
    QLabel* m_statusLabel = nullptr;
    RemoteReadApplicationServices* m_remoteReads = nullptr;
    RemoteInventoryMutationApplicationService* m_remoteMutations = nullptr;
    QHash<int, QString> m_remoteStoragePaths;
    QPointer<RemoteInventoryMutationDialog> m_activeRemoteMutationDialog;
};
