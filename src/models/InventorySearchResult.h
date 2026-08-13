#pragma once

#include <QString>

struct InventorySearchResult
{
    int inventoryRecordId = 0;

    int partId = 0;
    QString partNumber;
    QString partName;

    int categoryId = 0;
    QString categoryName;

    int colorId = 0;
    QString colorName;
    QString colorRgb;

    int storageLocationId = 0;
    QString storageLocationName;

    QString condition;
    QString ownershipType;

    int quantity = 0;
};