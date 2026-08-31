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
 */

#include "BuildPullingService.h"

#include "../../database/DatabaseManager.h"

#include "../../models/Build.h"
#include "../../models/BuildAllocation.h"
#include "../../models/BuildRequirement.h"
#include "../../models/Color.h"
#include "../../models/InventoryMovement.h"
#include "../../models/InventoryRecord.h"
#include "../../models/Part.h"
#include "../../models/StorageLocation.h"

#include "../../repositories/BuildAllocationRepository.h"
#include "../../repositories/BuildRepository.h"
#include "../../repositories/BuildRequirementRepository.h"
#include "../../repositories/ColorRepository.h"
#include "../../repositories/InventoryMovementRepository.h"
#include "../../repositories/InventoryRecordRepository.h"
#include "../../repositories/PartRepository.h"
#include "../../repositories/StorageLocationRepository.h"

#include <QDebug>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlError>

#include <algorithm>

BuildPullingService::PullingView BuildPullingService::getPullingView(int buildId) const
{
    PullingView view;
    view.summary.buildId = buildId;

    if (buildId <= 0) {
        view.message = "A valid Build is required.";
        return view;
    }

    BuildRepository buildRepository;
    BuildRequirementRepository requirementRepository;
    BuildAllocationRepository allocationRepository;
    InventoryRecordRepository inventoryRepository;
    PartRepository partRepository;
    ColorRepository colorRepository;

    const std::optional<Build> build = buildRepository.getById(buildId);

    if (!build) {
        view.message = "Unable to load the selected Build.";
        return view;
    }

    if (build->inventoryMode() != "Stock") {
        view.message = "Interactive pulling is available only for Build from Stock.";
        return view;
    }

    view.summary.buildName = build->name();
    view.summary.setNumber = build->setNumber();

    const QList<BuildRequirement> requirements = requirementRepository.getByBuild(buildId);

    QSet<int> remainingLocations;

    for (const BuildRequirement& requirement : requirements) {
        if (requirement.isSpare())
            continue;

        view.summary.totalRequired += requirement.quantityRequired();
        view.summary.totalPulled += qMin(requirement.quantityPulled(),
                                         requirement.quantityRequired());

        const QList<BuildAllocation> allocations =
            allocationRepository.getByRequirement(requirement.id());

        for (const BuildAllocation& allocation : allocations) {
            const std::optional<InventoryRecord> inventoryRecord =
                inventoryRepository.getById(allocation.inventoryRecordId());

            const std::optional<Part> part = partRepository.getById(allocation.partId());
            const std::optional<Color> color = colorRepository.getById(allocation.colorId());

            if (!inventoryRecord || !part || !color)
                continue;

            PullingItem item;
            item.buildId = buildId;
            item.buildRequirementId = requirement.id();
            item.allocationId = allocation.id();
            item.inventoryRecordId = inventoryRecord->id();
            item.storageLocationId = allocation.storageLocationId();
            item.partId = allocation.partId();
            item.colorId = allocation.colorId();
            item.partNumber = part->partNumber();
            item.partName = part->name();
            item.colorName = color->name();
            item.storagePath = storagePath(allocation.storageLocationId());
            item.quantityRequired = requirement.quantityRequired();
            item.quantityPulledForRequirement = requirement.quantityPulled();
            item.quantityAllocatedHere = allocation.quantityAllocated();

            item.originalPartId = requirement.partId();
            item.originalColorId = requirement.colorId();
            item.isSubstitution = requirement.effectivePartId() != requirement.partId()
                                  || requirement.effectiveColorId() != requirement.colorId();

            if (item.isSubstitution) {
                const std::optional<Part> originalPart =
                    partRepository.getById(requirement.partId());
                const std::optional<Color> originalColor =
                    colorRepository.getById(requirement.colorId());

                if (originalPart)
                    item.originalPartNumber = originalPart->partNumber();

                if (originalColor)
                    item.originalColorName = originalColor->name();
            }

            if (allocation.quantityAllocated() > 0)
                remainingLocations.insert(allocation.storageLocationId());

            view.items.append(item);
        }
    }

    view.summary.remaining = qMax(view.summary.totalRequired - view.summary.totalPulled, 0);
    view.summary.locationsRemaining = remainingLocations.size();

    std::sort(view.items.begin(), view.items.end(), [](const PullingItem& left,
                                                       const PullingItem& right) {
        const int storageCompare = QString::compare(left.storagePath,
                                                    right.storagePath,
                                                    Qt::CaseInsensitive);

        if (storageCompare != 0)
            return storageCompare < 0;

        const int partCompare = QString::compare(left.partNumber,
                                                 right.partNumber,
                                                 Qt::CaseInsensitive);

        if (partCompare != 0)
            return partCompare < 0;

        return left.allocationId < right.allocationId;
    });

    view.success = true;
    return view;
}

BuildPullingService::PullResult BuildPullingService::recordPull(int allocationId,
                                                                 int quantity) const
{
    PullRequest request;
    request.allocationId = allocationId;
    request.quantity = quantity;

    return recordPulls(QList<PullRequest>() << request);
}

BuildPullingService::PullResult
BuildPullingService::recordPulls(const QList<PullRequest>& requests) const
{
    PullResult result;

    if (requests.isEmpty()) {
        result.message = "No pull quantities were supplied.";
        return result;
    }

    for (const PullRequest& request : requests) {
        if (request.allocationId <= 0 || request.quantity < 0) {
            result.message = "A pull request contains an invalid allocation or quantity.";
            return result;
        }
    }

    QSqlDatabase database = DatabaseManager::instance().database();

    if (!database.transaction()) {
        qCritical() << "Unable to start Build pulling transaction:"
                    << database.lastError().text();
        result.message = "Unable to start the Build pulling transaction.";
        return result;
    }

    int expectedBuildId = 0;
    int rowsPulled = 0;
    int piecesPulled = 0;

    for (const PullRequest& request : requests) {
        if (request.quantity == 0)
            continue;

        int rowBuildId = 0;
        int rowPieces = 0;
        QString errorMessage;

        if (!applyPull(request, rowBuildId, rowPieces, errorMessage)) {
            database.rollback();
            result.message = errorMessage;
            return result;
        }

        if (expectedBuildId == 0) {
            expectedBuildId = rowBuildId;
        } else if (rowBuildId != expectedBuildId) {
            database.rollback();
            result.message = "Pull requests from different Builds cannot be committed together.";
            return result;
        }

        ++rowsPulled;
        piecesPulled += rowPieces;
    }

    if (!database.commit()) {
        qCritical() << "Unable to commit Build pulling transaction:"
                    << database.lastError().text();
        database.rollback();
        result.message = "Unable to commit the Build pulling transaction.";
        return result;
    }

    result.success = true;
    result.rowsPulled = rowsPulled;
    result.piecesPulled = piecesPulled;
    result.message = QString("Pulled %1 piece(s) from %2 allocation row(s).")
                         .arg(piecesPulled)
                         .arg(rowsPulled);

    if (piecesPulled > 0) {
        qInfo() << "Build pulling completed."
                << "BuildId:" << expectedBuildId
                << "Rows:" << rowsPulled
                << "PiecesPulled:" << piecesPulled;
    }

    return result;
}

bool BuildPullingService::applyPull(const PullRequest& request,
                                    int& buildId,
                                    int& piecesPulled,
                                    QString& errorMessage) const
{
    buildId = 0;
    piecesPulled = 0;

    BuildAllocationRepository allocationRepository;
    BuildRequirementRepository requirementRepository;
    InventoryRecordRepository inventoryRepository;
    InventoryMovementRepository movementRepository;
    BuildRepository buildRepository;

    const std::optional<BuildAllocation> allocation =
        allocationRepository.getById(request.allocationId);

    if (!allocation) {
        errorMessage = "The Build allocation no longer exists.";
        return false;
    }

    buildId = allocation->buildId();

    const std::optional<Build> build = buildRepository.getById(allocation->buildId());

    if (!build || build->inventoryMode() != "Stock") {
        errorMessage = "The selected allocation is not part of a Build from Stock.";
        return false;
    }

    if (allocation->buildRequirementId() <= 0) {
        errorMessage = "The Build allocation is not linked to a requirement.";
        return false;
    }

    const std::optional<BuildRequirement> requirement =
        requirementRepository.getById(allocation->buildRequirementId());

    if (!requirement
        || requirement->buildId() != allocation->buildId()
        || requirement->isSpare()) {
        errorMessage = "The Build requirement linked to this allocation is no longer valid.";
        return false;
    }

    if (requirement->effectivePartId() != allocation->partId()
        || requirement->effectiveColorId() != allocation->colorId()) {
        errorMessage = "The allocation no longer matches the requirement's effective Part/Color.";
        return false;
    }

    const std::optional<InventoryRecord> inventoryRecord =
        inventoryRepository.getById(allocation->inventoryRecordId());

    if (!inventoryRecord) {
        errorMessage = "The source inventory record no longer exists.";
        return false;
    }

    if (inventoryRecord->partId() != allocation->partId()
        || inventoryRecord->colorId() != allocation->colorId()
        || inventoryRecord->storageLocationId() != allocation->storageLocationId()) {
        errorMessage = "The source inventory identity or storage location changed.";
        return false;
    }

    if (request.quantity > allocation->quantityAllocated()) {
        errorMessage = "The pulled quantity exceeds the current allocation.";
        return false;
    }

    if (request.quantity > inventoryRecord->quantity()) {
        errorMessage = "The pulled quantity exceeds the available physical inventory.";
        return false;
    }

    const int newPulledQuantity = requirement->quantityPulled() + request.quantity;

    if (newPulledQuantity > requirement->quantityRequired()) {
        errorMessage = "The pulled quantity would exceed the Build requirement.";
        return false;
    }

    const int newInventoryQuantity = inventoryRecord->quantity() - request.quantity;

    if (!inventoryRepository.updateQuantity(inventoryRecord->id(), newInventoryQuantity)) {
        errorMessage = "Unable to update the source inventory quantity.";
        return false;
    }

    if (!allocationRepository.recordPulledManufacturer(allocation->buildId(),
                                                       inventoryRecord->partId(),
                                                       inventoryRecord->colorId(),
                                                       inventoryRecord->manufacturerId(),
                                                       request.quantity)) {
        errorMessage = "Unable to record manufacturer provenance for the pulled pieces.";
        return false;
    }

    const int remainingAllocation = allocation->quantityAllocated() - request.quantity;

    if (remainingAllocation == 0) {
        if (!allocationRepository.remove(allocation->id())) {
            errorMessage = "Unable to remove the completed Build allocation.";
            return false;
        }
    } else {
        BuildAllocation updatedAllocation = *allocation;
        updatedAllocation.setQuantityAllocated(remainingAllocation);

        if (!allocationRepository.update(updatedAllocation)) {
            errorMessage = "Unable to reduce the remaining Build allocation.";
            return false;
        }
    }

    BuildRequirement updatedRequirement = *requirement;
    updatedRequirement.setQuantityPulled(newPulledQuantity);

    if (!requirementRepository.update(updatedRequirement)) {
        errorMessage = "Unable to update the Build requirement pulled quantity.";
        return false;
    }

    InventoryMovement movement;
    movement.setWorkspaceId(inventoryRecord->workspaceId());
    movement.setInventoryRecordId(inventoryRecord->id());
    movement.setPartId(inventoryRecord->partId());
    movement.setColorId(inventoryRecord->colorId());
    movement.setMovementType("BuildPull");
    movement.setQuantityChange(-request.quantity);
    movement.setFromStorageLocationId(inventoryRecord->storageLocationId());
    movement.setCondition(inventoryRecord->condition());
    movement.setOwnershipType(inventoryRecord->ownershipType());
    movement.setReferenceType("Build");
    movement.setReferenceId(QString::number(allocation->buildId()));
    movement.setNotes(QString("Pulled for %1%2.")
                          .arg(build->name())
                          .arg(build->setNumber().trimmed().isEmpty()
                                   ? QString()
                                   : QString(" (%1)").arg(build->setNumber())));

    if (!movementRepository.create(movement)) {
        errorMessage = "Unable to create inventory movement history for the pull.";
        return false;
    }

    piecesPulled = request.quantity;
    return true;
}

QString BuildPullingService::storagePath(int storageLocationId) const
{
    StorageLocationRepository repository;

    QStringList parts;
    int currentId = storageLocationId;
    int safetyCount = 0;

    while (currentId > 0 && safetyCount < 100) {
        const std::optional<StorageLocation> location = repository.getById(currentId);

        if (!location)
            break;

        parts.prepend(location->name());
        currentId = location->parentLocationId();
        ++safetyCount;
    }

    return parts.join(" / ");
}
