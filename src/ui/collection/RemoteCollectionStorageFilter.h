#pragma once

#include "../../services/application/dto/RemoteReadDtos.h"

#include <QList>
#include <QSet>

namespace RemoteCollectionStorageFilter {
inline QList<RemoteReadDto::StorageSummary> eligibleLeaves(
    const QList<RemoteReadDto::StorageSummary>& locations)
{
    QSet<qint64> activeParents;
    for (const auto& location : locations)
        if (location.active && location.parentStorageId > 0)
            activeParents.insert(location.parentStorageId);
    QList<RemoteReadDto::StorageSummary> result;
    for (const auto& location : locations)
        if (location.active && location.allowsCollection
            && !activeParents.contains(location.storageId))
            result.append(location);
    return result;
}
}
