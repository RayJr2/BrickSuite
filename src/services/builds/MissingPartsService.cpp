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

#include "MissingPartsService.h"

#include "../../models/BuildRequirement.h"
#include "../../models/Color.h"
#include "../../models/Part.h"

#include "../../repositories/BuildAllocationRepository.h"
#include "../../repositories/BuildRequirementRepository.h"
#include "../../repositories/ColorRepository.h"
#include "../../repositories/InventoryRecordRepository.h"
#include "../../repositories/PartRepository.h"

#include <QtGlobal>

QList<MissingPartsService::MissingPart>
MissingPartsService::getMissingParts(
    int workspaceId,
    int buildId) const
{
    QList<MissingPart> results;

    if (workspaceId <= 0 ||
        buildId <= 0)
    {
        return results;
    }

    BuildRequirementRepository
        requirementRepository;

    const QList<BuildRequirement> requirements =
        requirementRepository.getByBuild(
            buildId);

    PartRepository partRepository;
    ColorRepository colorRepository;

    InventoryRecordRepository
        inventoryRepository;

    BuildAllocationRepository
        allocationRepository;

    for (const BuildRequirement& requirement :
         requirements)
    {
        //
        // Spare requirements are optional and should
        // not appear on an acquisition list.
        //
        if (requirement.isSpare())
            continue;

        const int remaining =
            qMax(
                requirement.quantityRequired() -
                    requirement.quantityPulled(),
                0);

        if (remaining <= 0)
            continue;

        //
        // A substituted requirement is fulfilled by its
        // effective Part / Color identity. The original
        // requirement identity remains unchanged for the
        // Build definition itself.
        //
        const int effectivePartId =
            requirement.effectivePartId();

        const int effectiveColorId =
            requirement.effectiveColorId();

        const int owned =
            inventoryRepository
                .totalQuantityForPartColor(
                    workspaceId,
                    effectivePartId,
                    effectiveColorId);

        const int totalAllocated =
            allocationRepository
                .totalAllocatedForPartColor(
                    workspaceId,
                    effectivePartId,
                    effectiveColorId);

        //
        // Requirement-aware allocation is critical when
        // more than one requirement resolves to the same
        // effective Part / Color. Only allocations tied to
        // this requirement satisfy this requirement.
        //
        const int thisBuildAllocated =
            allocationRepository
                .totalAllocatedForRequirement(
                    requirement.id());

        const int otherBuildsAllocated =
            qMax(
                totalAllocated -
                    thisBuildAllocated,
                0);

        const int available =
            qMax(
                owned -
                    totalAllocated,
                0);

        const int missing =
            qMax(
                remaining -
                    thisBuildAllocated -
                    available,
                0);

        if (missing <= 0)
            continue;

        const std::optional<Part> part =
            partRepository.getById(
                effectivePartId);

        const std::optional<Color> color =
            colorRepository.getById(
                effectiveColorId);

        MissingPart item;

        item.partId =
            effectivePartId;

        item.colorId =
            effectiveColorId;

        item.partNumber =
            part
                ? part->partNumber()
                : QString::number(
                      effectivePartId);

        item.partName =
            part
                ? part->name()
                : QString();

        item.colorName =
            color
                ? color->name()
                : QString::number(
                      effectiveColorId);

        item.required =
            requirement.quantityRequired();

        item.pulled =
            requirement.quantityPulled();

        item.remaining =
            remaining;

        item.owned =
            owned;

        item.thisBuildAllocated =
            thisBuildAllocated;

        item.otherBuildsAllocated =
            otherBuildsAllocated;

        item.available =
            available;

        item.missing =
            missing;

        results.append(
            item);
    }

    return results;
}
