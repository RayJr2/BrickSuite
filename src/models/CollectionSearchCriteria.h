#pragma once

#include "CollectionItem.h"

#include <QString>

struct CollectionSearchCriteria
{
    int workspaceId = 0;
    QString searchText;
    CollectionItemType type = CollectionItemType::Invalid;
    CollectionItemState state = CollectionItemState::Invalid;
    int storageLocationId = 0;
    bool includeInactive = false;
    int limit = 100;
    int offset = 0;
};
