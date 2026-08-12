#pragma once

#include <QString>

struct InventorySearchCriteria
{
    int workspaceId = 0;

    QString searchText;

    int categoryId = 0;
    int colorId = 0;
    int storageLocationId = 0;

    int limit = 250;
    int offset = 0;
};
