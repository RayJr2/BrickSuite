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
