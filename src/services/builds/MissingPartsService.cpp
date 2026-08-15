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

        const int owned =
            inventoryRepository
                .totalQuantityForPartColor(
                    workspaceId,
                    requirement.partId(),
                    requirement.colorId());

        const int totalAllocated =
            allocationRepository
                .totalAllocatedForPartColor(
                    workspaceId,
                    requirement.partId(),
                    requirement.colorId());

        const int thisBuildAllocated =
            allocationRepository
                .totalAllocatedForPartColorForBuild(
                    buildId,
                    requirement.partId(),
                    requirement.colorId());

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
                requirement.partId());

        const std::optional<Color> color =
            colorRepository.getById(
                requirement.colorId());

        MissingPart item;

        item.partId =
            requirement.partId();

        item.colorId =
            requirement.colorId();

        item.partNumber =
            part
                ? part->partNumber()
                : QString::number(
                      requirement.partId());

        item.partName =
            part
                ? part->name()
                : QString();

        item.colorName =
            color
                ? color->name()
                : QString::number(
                      requirement.colorId());

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