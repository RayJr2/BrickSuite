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
