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
#include <QElapsedTimer>
#include <QPointer>
#include <optional>
#include "../../services/application/dto/RemoteReadDtos.h"
#include "../../services/application/dto/RemoteStorageMutationDtos.h"
#include "StorageLocationDialog.h"
#include <functional>

class WorkspaceContext;
class QTreeWidget;
class QPushButton;
class QLabel;
class QMessageBox;
class RemoteReadApplicationServices;
class RemoteStorageMutationApplicationService;

class StorageWidget : public QWidget
{
    Q_OBJECT

public:
    explicit StorageWidget(
        WorkspaceContext& workspaceContext,
        RemoteReadApplicationServices* remoteReads = nullptr,
        RemoteStorageMutationApplicationService* remoteMutations = nullptr,
        QWidget* parent = nullptr);
    void refresh();
    void setRemoteSessionConnected(bool connected);

signals:
    void storageLocationsChanged();
    void hostStorageMutationCommitted(int workspaceId, int storageLocationId);
    void remoteRefreshFinished(bool succeeded);

private slots:
    void workspaceChanged(int workspaceId);
    void addLocation();
    void editLocation();
    void deactivateLocation();
    void reactivateLocation();

private:
    void loadStorageTree();
    void loadRemoteStorageTree();
    void setMutationControlsEnabled(bool enabled);
    void updateRemoteActionState();
    void remoteAddLocation();
    void remoteEditLocation();
    void remoteSetActive(bool active);
    void fetchRemoteTypes(std::function<void(bool)> completion);
    void openRemoteDialog(StorageLocationDialog::Mode mode,
                          const std::optional<RemoteReadDto::StorageDetail>& detail);
    void submitRemoteDialog();
    void submitRemoteSetActive(const RemoteReadDto::StorageDetail& detail,bool active);
    QList<StorageLocationDialog::Choice> remoteParentChoices(qint64 excludedId=0)const;

    WorkspaceContext& m_workspaceContext;
    RemoteReadApplicationServices* m_remoteReads = nullptr;
    RemoteStorageMutationApplicationService* m_remoteMutations = nullptr;
    quint64 m_storageRequestToken = 0;
    QElapsedTimer m_storageRequestTimer;
    QList<RemoteReadDto::StorageSummary> m_remoteStorage;
    QList<RemoteReadDto::StorageType> m_remoteTypes;
    QPointer<StorageLocationDialog> m_remoteDialog;
    QPointer<QMessageBox> m_remoteConfirmation;
    std::optional<RemoteReadDto::StorageDetail> m_remoteDialogDetail;
    std::optional<RemoteStorageMutationDto::Request> m_retainedRequest;
    QString m_retainedOperation;
    quint64 m_actionGeneration = 0;
    bool m_remoteConnected = false;
    bool m_remoteStale = true;
    bool m_remoteMutationPending = false;

    QTreeWidget* m_tree = nullptr;
    QPushButton* m_addButton = nullptr;
    QPushButton* m_editButton = nullptr;
    QPushButton* m_deactivateButton = nullptr;
    QPushButton* m_reactivateButton = nullptr;
    QLabel* m_statusLabel = nullptr;
};
