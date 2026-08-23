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

#include "../models/BuildAllocation.h"

#include <QList>
#include <optional>

class QSqlQuery;

struct BuildPartManufacturerProvenance
{
    int manufacturerId = 0;
    int quantityPulled = 0;
};

class BuildAllocationRepository
{
public:
    bool create(BuildAllocation& allocation);

    std::optional<BuildAllocation> getById(int id) const;

    QList<BuildAllocation> getByBuild(int buildId) const;

    QList<BuildAllocation> getByInventoryRecord(int inventoryRecordId) const;

    bool update(BuildAllocation& allocation);

    bool remove(int allocationId);

    bool removeAllForBuild(int buildId);

    int totalAllocatedForPartColor(int workspaceId, int partId, int colorId) const;

    int totalAllocatedForPartColorForBuild(int buildId, int partId, int colorId) const;

    int totalAllocatedForInventoryRecord(int inventoryRecordId) const;

    int totalAllocatedForInventoryRecordForBuild(int inventoryRecordId, int buildId) const;

    bool recordPulledManufacturer(int buildId,
                                  int partId,
                                  int colorId,
                                  int manufacturerId,
                                  int quantity);

    QList<BuildPartManufacturerProvenance> pulledManufacturerProvenance(
        int buildId,
        int partId,
        int colorId) const;

    bool reducePulledManufacturer(int buildId,
                                  int partId,
                                  int colorId,
                                  int manufacturerId,
                                  int quantity);

private:
    BuildAllocation allocationFromQuery(const QSqlQuery& query) const;
};