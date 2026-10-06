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

#include "../../models/StorageLocation.h"

#include <QSqlDatabase>
#include <QString>

class HostStorageMutationService
{
public:
    static constexpr int MaximumNameLength = 200;
    static constexpr int MaximumDescriptionLength = 2000;

    enum class ErrorCode {
        None, InvalidArgument, WorkspaceMissing, WorkspaceInactive, StorageMissing,
        ParentMissing, ParentInactive, ParentWrongWorkspace, TypeMissing, TypeInactive,
        SelfParent, DescendantCycle, InventoryOccupied, CollectionOccupied,
        ActiveChildren, StaleExpectedState, DatabaseFailure
    };

    struct Error { ErrorCode code = ErrorCode::None; QString message; };
    struct ExpectedState {
        bool provided = false;
        QDateTime modifiedUtc;
        int parentStorageId = 0;
        int storageTypeId = 0;
        QString name;
        QString description;
        int sortOrder = 0;
        bool active = true;
        bool allowsInventory = true;
        bool allowsCollection = false;
    };
    struct Result {
        bool success = false;
        StorageLocation location;
        QString displayPath;
        Error error;
    };
    struct AddRequest {
        int workspaceId = 0;
        int parentStorageId = 0;
        int storageTypeId = 0;
        QString name;
        QString description;
        bool allowsInventory = true;
        bool allowsCollection = false;
    };
    struct EditRequest {
        int workspaceId = 0;
        int storageId = 0;
        int parentStorageId = 0;
        int storageTypeId = 0;
        QString name;
        QString description;
        bool allowsInventory = true;
        bool allowsCollection = false;
        ExpectedState expected;
    };
    struct SetActiveRequest {
        int workspaceId = 0;
        int storageId = 0;
        bool active = true;
        ExpectedState expected;
    };

    explicit HostStorageMutationService(const QSqlDatabase& database);
    Result add(const AddRequest& request) const;
    Result edit(const EditRequest& request) const;
    Result setActive(const SetActiveRequest& request) const;
    static ExpectedState expectedState(const StorageLocation& location);

private:
    QSqlDatabase database() const;
    Result authoritative(int storageId) const;
    Result fail(ErrorCode code, const QString& message) const;
    Error validateWorkspace(int workspaceId) const;
    Error validateWorkspaceAndType(int workspaceId, int storageTypeId) const;
    Error validateParent(int workspaceId, int parentStorageId) const;
    bool matchesExpected(const StorageLocation& location, const ExpectedState& expected) const;

    QString m_connectionName;
    class QThread* m_ownerThread = nullptr;
};
