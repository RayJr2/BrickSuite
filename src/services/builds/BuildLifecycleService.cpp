#include "BuildLifecycleService.h"

#include "../../database/DatabaseManager.h"
#include "../../repositories/BuildAllocationRepository.h"
#include "../../repositories/BuildRepository.h"
#include "../../repositories/BuildRequirementRepository.h"
#include "../../repositories/CollectionRepository.h"
#include "../../repositories/InventoryRecordRepository.h"
#include "../../repositories/StorageLocationRepository.h"
#include "../collection/CollectionItemService.h"

namespace {
BuildLifecycleService::Result failure(BuildLifecycleService::Error error, const QString& message)
{
    BuildLifecycleService::Result result;
    result.error = error;
    result.message = message;
    return result;
}
}

BuildLifecycleService::BuildLifecycleService()
    : BuildLifecycleService(DatabaseManager::instance().database()) {}

BuildLifecycleService::BuildLifecycleService(const QSqlDatabase& database)
    : m_connectionName(database.connectionName()) {}

QSqlDatabase BuildLifecycleService::database() const
{
    return QSqlDatabase::database(m_connectionName, false);
}

BuildLifecycleService::Result BuildLifecycleService::inTransaction(
    const std::function<Result()>& operation) const
{
    QSqlDatabase db = database();
    if (!db.isValid() || !db.isOpen() || !db.transaction())
        return failure(Error::DatabaseFailure, "Unable to begin the Build lifecycle transaction.");
    Result result = operation();
    if (!result.success) {
        db.rollback();
        return result;
    }
    if (!db.commit()) {
        db.rollback();
        return failure(Error::DatabaseFailure, "Unable to commit the Build lifecycle transaction.");
    }
    return result;
}

BuildLifecycleService::Result BuildLifecycleService::returnPiecesInCurrentTransaction(
    Build& build, const QList<DisassemblyReturn>& rows) const
{
    QSqlDatabase db = database();
    InventoryRecordRepository inventory(db);
    BuildRequirementRepository requirements(db);
    BuildAllocationRepository allocations(db);
    StorageLocationRepository storage(db);
    Result result;

    for (const DisassemblyReturn& row : rows) {
        if (row.quantity <= 0)
            continue;
        std::optional<BuildRequirement> requirement;
        if (!requirements.tryGetById(row.requirementId, requirement))
            return failure(Error::DatabaseFailure, "Unable to validate a Build requirement.");
        std::optional<StorageLocation> destination;
        if (!storage.tryGetById(row.storageLocationId, destination))
            return failure(Error::DatabaseFailure, "Unable to validate a storage destination.");
        const auto children = storage.hasActiveChildrenChecked(row.storageLocationId);
        if (children == StorageLocationRepository::CheckResult::Error)
            return failure(Error::DatabaseFailure, "Unable to validate a storage destination.");
        const int expectedPartId=requirement&&build.inventoryMode()==QStringLiteral("Stock")
            ?requirement->effectivePartId():(requirement?requirement->partId():0);
        const int expectedColorId=requirement&&build.inventoryMode()==QStringLiteral("Stock")
            ?requirement->effectiveColorId():(requirement?requirement->colorId():0);
        if (!requirement || requirement->buildId() != build.id()
            || expectedPartId != row.partId || expectedColorId != row.colorId
            || !destination || !destination->isActive()
            || destination->workspaceId() != build.workspaceId()
            || children == StorageLocationRepository::CheckResult::Yes) {
            return failure(Error::InvalidState, "A Build requirement or storage destination changed.");
        }
        if (build.inventoryMode() == "Stock"
            && (row.quantity > requirement->quantityPulled() || row.manufacturerId <= 0)) {
            return failure(Error::InvalidState, "The return exceeds pulled manufacturer provenance.");
        }
        if (build.inventoryMode() == "CompleteSet"
            && row.quantity > qMax(requirement->quantityRequired()
                                       - requirement->quantityReleased(), 0)) {
            return failure(Error::InvalidState, "The return exceeds available Complete Set pieces.");
        }

        InventoryRecord record;
        record.setWorkspaceId(build.workspaceId());
        record.setPartId(row.partId);
        record.setColorId(row.colorId);
        record.setStorageLocationId(row.storageLocationId);
        record.setManufacturerId(row.manufacturerId);
        record.setCondition(build.inventoryMode() == "CompleteSet" && row.spare ? "New" : "Used");
        record.setOwnershipType("Owned");
        record.setQuantity(row.quantity);
        InventoryRecordRepository::AddResult added;
        const QString movement = build.inventoryMode() == "CompleteSet"
            ? QStringLiteral("SetDisassembly") : QStringLiteral("BuildDisassembly");
        const QString notes = QStringLiteral("Returned while disassembling %1.").arg(build.name());
        if (!inventory.addOrIncreaseQuantityInCurrentTransaction(
                record, movement, "Build", QString::number(build.id()), notes, &added)) {
            return failure(Error::DatabaseFailure, "Unable to return a Build piece to Inventory.");
        }
        if (build.inventoryMode() == "Stock") {
            if (!allocations.reducePulledManufacturer(build.id(), row.partId, row.colorId,
                                                      row.manufacturerId, row.quantity)) {
                return failure(Error::DatabaseFailure,
                               "Unable to update Build manufacturer provenance.");
            }
            requirement->setQuantityPulled(requirement->quantityPulled() - row.quantity);
            if (!requirements.update(*requirement))
                return failure(Error::DatabaseFailure, "Unable to update the pulled quantity.");
        }
        result.affectedRequirementIds.append(row.requirementId);
        result.affectedInventoryIds.append(added.inventoryRecordId);
        result.returnedPieces += row.quantity;
    }
    result.success = true;
    return result;
}

BuildLifecycleService::ReturnPlanResult BuildLifecycleService::disassemblyReturnPlan(int buildId) const
{
    QSqlDatabase db=database();ReturnPlanResult result;
    std::optional<Build> build;
    if(!BuildRepository(db).tryGetById(buildId,build)){result.error=Error::DatabaseFailure;result.message="Unable to load the Build.";return result;}
    if(!build){result.error=Error::NotFound;result.message="The Build was not found.";return result;}
    if(!build->isActive()||build->status()!=QStringLiteral("Complete")
        ||(build->inventoryMode()!=QStringLiteral("Stock")&&build->inventoryMode()!=QStringLiteral("CompleteSet"))){result.error=Error::InvalidState;result.message="Only an active Complete Build can be disassembled.";return result;}
    QList<BuildRequirement> requirements;
    if(!BuildRequirementRepository(db).tryGetByBuild(buildId,requirements)){result.error=Error::DatabaseFailure;result.message="Unable to load Build requirements.";return result;}
    BuildAllocationRepository allocations(db);
    for(const auto& requirement:requirements){
        if(build->inventoryMode()==QStringLiteral("CompleteSet")){
            const int quantity=requirement.isSpare()?qMax(requirement.quantityRequired()-requirement.quantityReleased(),0):requirement.quantityRequired();
            if(quantity>0)result.rows.append({requirement.id(),requirement.partId(),requirement.colorId(),build->manufacturerId(),0,quantity,requirement.isSpare()});
            continue;
        }
        int tracked=0;
        for(const auto& provenance:allocations.pulledManufacturerProvenance(buildId,requirement.effectivePartId(),requirement.effectiveColorId())){
            result.rows.append({requirement.id(),requirement.effectivePartId(),requirement.effectiveColorId(),provenance.manufacturerId,0,provenance.quantityPulled,requirement.isSpare()});
            tracked+=provenance.quantityPulled;
        }
        if(tracked!=requirement.quantityPulled()){result.error=Error::InvalidState;result.message=QString("Manufacturer provenance is incomplete for requirement %1.").arg(requirement.id());result.rows.clear();return result;}
    }
    if(result.rows.isEmpty()){result.error=Error::InvalidState;result.message="The Build has no pieces available to return.";return result;}
    result.success=true;result.build=*build;return result;
}

BuildLifecycleService::Result BuildLifecycleService::disassemble(
    int buildId, const QList<DisassemblyReturn>& returns,
    CollectionItemState linkedCollectionState) const
{
    return inTransaction([&] {
        return disassembleInCurrentTransaction(buildId, returns, linkedCollectionState);
    });
}

BuildLifecycleService::Result BuildLifecycleService::disassembleInCurrentTransaction(
    int buildId, const QList<DisassemblyReturn>& returns,
    CollectionItemState linkedCollectionState) const
{
    QSqlDatabase db = database();
    std::optional<Build> build;
    if (!BuildRepository(db).tryGetById(buildId, build))
        return failure(Error::DatabaseFailure, "Unable to load the Build.");
    if (!build)
        return failure(Error::NotFound, "The Build was not found.");
    if (build->status() != "Complete")
        return failure(Error::InvalidState, "Only a Complete Build can be disassembled.");
    Result result = returnPiecesInCurrentTransaction(*build, returns);
    if (!result.success)
        return result;
    build->setStatus("Disassembled");
    if (!BuildRepository(db).update(*build))
        return failure(Error::DatabaseFailure, "Unable to update the Build status.");
    std::optional<CollectionItem> linked;
    if (!CollectionRepository(db).tryGetBySourceBuild(buildId, linked))
        return failure(Error::DatabaseFailure, "Unable to inspect the linked Collection item.");
    if (linked) {
        const auto collection = CollectionItemService(db)
            .updateStateForDisassemblyInCurrentTransaction(buildId, linkedCollectionState);
        if (!collection.success)
            return failure(Error::DatabaseFailure, collection.message);
        result.collectionChanged = collection.changed;
    }
    result.build = *build;
    result.message = "Build disassembled.";
    return result;
}

BuildLifecycleService::Result BuildLifecycleService::cancel(
    int buildId, const QList<DisassemblyReturn>& returns,
    CollectionItemState linkedCollectionState) const
{
    return inTransaction([&] {
        return cancelInCurrentTransaction(buildId, returns, linkedCollectionState);
    });
}

BuildLifecycleService::Result BuildLifecycleService::cancelInCurrentTransaction(
    int buildId, const QList<DisassemblyReturn>& returns,
    CollectionItemState linkedCollectionState) const
{
    QSqlDatabase db = database();
    std::optional<Build> build;
    if (!BuildRepository(db).tryGetById(buildId, build))
        return failure(Error::DatabaseFailure, "Unable to load the Build.");
    if (!build)
        return failure(Error::NotFound, "The Build was not found.");
    if (build->status() != "Planned" && build->status() != "Pulling")
        return failure(Error::InvalidState, "Only a Planned or Pulling Build can be cancelled.");
    Result result = returnPiecesInCurrentTransaction(*build, returns);
    if (!result.success)
        return result;
    QList<BuildRequirement> requirements;
    if (!BuildRequirementRepository(db).tryGetByBuild(buildId, requirements))
        return failure(Error::DatabaseFailure, "Unable to validate Build requirements.");
    for (const BuildRequirement& requirement : requirements) {
        if (requirement.quantityPulled() > 0)
            return failure(Error::InvalidState,
                           "Every pulled piece must be returned before cancellation.");
    }
    BuildAllocationRepository allocations(db);
    QList<BuildAllocation> existingAllocations;
    if (!allocations.tryGetByBuild(buildId, existingAllocations))
        return failure(Error::DatabaseFailure, "Unable to validate Build allocations.");
    for (const BuildAllocation& allocation : existingAllocations)
        result.affectedAllocationIds.append(allocation.id());
    if (!allocations.removeAllForBuild(buildId))
        return failure(Error::DatabaseFailure, "Unable to release Build allocations.");
    std::optional<CollectionItem> linked;
    if (!CollectionRepository(db).tryGetBySourceBuild(buildId, linked))
        return failure(Error::DatabaseFailure, "Unable to inspect the linked Collection item.");
    if (linked) {
        const auto collection = CollectionItemService(db)
            .updateStateForDisassemblyInCurrentTransaction(buildId, linkedCollectionState);
        if (!collection.success)
            return failure(Error::DatabaseFailure, collection.message);
        result.collectionChanged = collection.changed;
    }
    build->setStatus("Cancelled");
    if (!BuildRepository(db).update(*build))
        return failure(Error::DatabaseFailure, "Unable to cancel the Build.");
    result.build = *build;
    result.message = "Build cancelled.";
    return result;
}

BuildLifecycleService::Result BuildLifecycleService::storeCompleteSetSpare(
    int buildId, int requirementId, int storageId, int quantity) const
{
    return inTransaction([&] {
        return storeCompleteSetSpareInCurrentTransaction(buildId, requirementId, storageId, quantity);
    });
}

BuildLifecycleService::Result BuildLifecycleService::storeCompleteSetSpareInCurrentTransaction(
    int buildId, int requirementId, int storageId, int quantity) const
{
    QSqlDatabase db = database();
    std::optional<Build> build;
    std::optional<BuildRequirement> requirement;
    if (!BuildRepository(db).tryGetById(buildId, build)
        || !BuildRequirementRepository(db).tryGetById(requirementId, requirement)) {
        return failure(Error::DatabaseFailure, "Unable to validate the Complete Set spare.");
    }
    StorageLocationRepository storage(db);
    std::optional<StorageLocation> destination;
    if (!storage.tryGetById(storageId, destination))
        return failure(Error::DatabaseFailure, "Unable to validate the storage destination.");
    const auto children = storage.hasActiveChildrenChecked(storageId);
    if (children == StorageLocationRepository::CheckResult::Error)
        return failure(Error::DatabaseFailure, "Unable to validate the storage destination.");
    if (!build || !requirement || !destination)
        return failure(Error::NotFound,
                       "The Complete Set, spare requirement, or storage destination was not found.");
    if (build->inventoryMode() != "CompleteSet" || build->status() != "Complete"
        || requirement->buildId() != buildId || !requirement->isSpare() || quantity <= 0
        || quantity > qMax(requirement->quantityRequired() - requirement->quantityReleased(), 0)
        || !destination->isActive() || destination->workspaceId() != build->workspaceId()
        || children == StorageLocationRepository::CheckResult::Yes) {
        return failure(Error::InvalidState,
                       "The Complete Set, spare requirement, or storage destination changed.");
    }
    InventoryRecord record;
    record.setWorkspaceId(build->workspaceId());
    record.setPartId(requirement->partId());
    record.setColorId(requirement->colorId());
    record.setStorageLocationId(storageId);
    record.setManufacturerId(build->manufacturerId());
    record.setCondition("New");
    record.setOwnershipType("Owned");
    record.setQuantity(quantity);
    InventoryRecordRepository::AddResult added;
    if (!InventoryRecordRepository(db).addOrIncreaseQuantityInCurrentTransaction(
            record, "SetSpareRelease", "Build", QString::number(buildId),
            QStringLiteral("Stored boxed spare from Complete Set %1.").arg(build->name()), &added)) {
        return failure(Error::DatabaseFailure, "Unable to add the spare to Inventory.");
    }
    requirement->setQuantityReleased(requirement->quantityReleased() + quantity);
    if (!BuildRequirementRepository(db).update(*requirement))
        return failure(Error::DatabaseFailure, "Unable to record the stored spare quantity.");
    Result result;
    result.success = true;
    result.build = *build;
    result.affectedRequirementIds.append(requirementId);
    result.affectedInventoryIds.append(added.inventoryRecordId);
    result.returnedPieces = quantity;
    result.message = "Complete Set spare stored.";
    return result;
}
