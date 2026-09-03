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

#include "../models/StorageLocation.h"

#include <QList>
#include <optional>

class QSqlQuery;

class StorageLocationRepository
{
public:
    bool create(StorageLocation& location);

    QList<StorageLocation> getByWorkspace(int workspaceId) const;

    QList<StorageLocation> getInventoryHierarchy(int workspaceId) const;

    QList<StorageLocation> getCollectionHierarchy(int workspaceId) const;

    QList<StorageLocation> getByWorkspaceIncludingInactive(int workspaceId) const;

    QList<StorageLocation> getChildren(int workspaceId, int parentLocationId) const;

    std::optional<StorageLocation> getById(int id) const;

    bool update(StorageLocation& location);

    bool hasChildren(int locationId) const;

    bool isValidOperationalDestination(int workspaceId, int locationId,
                                       int excludedLocationId = 0) const;

    bool isValidInventoryDestination(int workspaceId, int locationId,
                                     int excludedLocationId = 0) const;

    bool isValidCollectionDestination(int workspaceId, int locationId) const;

    bool hasInventory(int locationId) const;

    bool isDescendant(int locationId, int possibleDescendantId) const;

    bool deactivate(int locationId);

    bool reactivate(int locationId);

private:
    QList<StorageLocation> getCapabilityHierarchy(int workspaceId,
                                                  const QString& capabilityColumn) const;
    bool isValidCapabilityDestination(int workspaceId, int locationId,
                                      const QString& capabilityColumn,
                                      int excludedLocationId = 0) const;
    StorageLocation locationFromQuery(const QSqlQuery& query) const;
};
