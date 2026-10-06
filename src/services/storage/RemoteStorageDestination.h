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

#include "../application/dto/RemoteReadDtos.h"

#include <QList>
#include <QSet>
#include <climits>

namespace RemoteStorageDestination {

inline QSet<int> validInventoryDestinationIds(
    const QList<RemoteReadDto::StorageSummary>& locations)
{
    QSet<qint64> activeParentIds;
    for (const auto& location : locations) {
        if (location.active && location.parentStorageId > 0)
            activeParentIds.insert(location.parentStorageId);
    }

    QSet<int> result;
    for (const auto& location : locations) {
        if (location.storageId > 0 && location.storageId <= INT_MAX
            && location.active && location.allowsInventory
            && !activeParentIds.contains(location.storageId)) {
            result.insert(int(location.storageId));
        }
    }
    return result;
}

inline bool isValidInventoryDestination(
    const QList<RemoteReadDto::StorageSummary>& locations, int workspaceId,
    int locationId, int excludedLocationId = 0)
{
    if (workspaceId <= 0 || locationId <= 0 || locationId == excludedLocationId)
        return false;
    return validInventoryDestinationIds(locations).contains(locationId);
}

} // namespace RemoteStorageDestination
