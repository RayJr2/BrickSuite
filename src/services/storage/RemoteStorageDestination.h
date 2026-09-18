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
