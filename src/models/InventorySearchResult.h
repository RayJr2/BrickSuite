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

    int manufacturerId = 0;
    QString manufacturerCode;
    QString manufacturerName;

    QString condition;
    QString ownershipType;

    int quantity = 0;
};