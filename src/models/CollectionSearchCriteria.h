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

#include "CollectionItem.h"

#include <QString>

struct CollectionSearchCriteria
{
    int workspaceId = 0;
    QString searchText;
    CollectionItemType type = CollectionItemType::Invalid;
    CollectionItemState state = CollectionItemState::Invalid;
    CollectionItemCondition condition = CollectionItemCondition::Invalid;
    CollectionItemCompleteness completeness = CollectionItemCompleteness::Invalid;
    int storageLocationId = 0;
    // 1 = active, 0 = archived, -1 = both.
    int activeState = 1;
    int limit = 100;
    int offset = 0;
};
