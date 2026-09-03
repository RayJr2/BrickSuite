#include "CollectionItemService.h"

#include "../../database/DatabaseManager.h"
#include "../../models/Build.h"
#include "../../repositories/BuildRepository.h"
#include "../../repositories/CollectionRepository.h"
#include "../../repositories/MinifigCatalogRepository.h"
#include "../../repositories/SetCatalogRepository.h"
#include "../../repositories/StorageLocationRepository.h"
#include "../../repositories/WorkspaceRepository.h"

#include <QSqlDatabase>
#include <QSqlError>

namespace {
CollectionItemService::Result failure(CollectionItemService::Error error,
                                      const QString& message)
{
    return {false, error, message, 0};
}
}

CollectionItemService::Result CollectionItemService::createSet(
    int workspaceId, int setCatalogId, CollectionItemState state,
    int storageLocationId, int sourceBuildId, const QString& nickname,
    const QString& notes, CollectionItemCondition condition,
    CollectionItemCompleteness completeness) const
{
    CollectionItem item;
    item.workspaceId = workspaceId;
    item.type = CollectionItemType::Set;
    item.setCatalogId = setCatalogId;
    item.state = state;
    item.condition = condition;
    item.completeness = completeness;
    item.storageLocationId = storageLocationId;
    item.sourceBuildId = sourceBuildId;
    item.nickname = nickname;
    item.notes = notes;
    return create(item);
}

CollectionItemService::Result CollectionItemService::createMinifig(
    int workspaceId, int minifigCatalogId, CollectionItemState state,
    int storageLocationId, int sourceBuildId, const QString& nickname,
    const QString& notes, CollectionItemCondition condition,
    CollectionItemCompleteness completeness) const
{
    CollectionItem item;
    item.workspaceId = workspaceId;
    item.type = CollectionItemType::Minifig;
    item.minifigCatalogId = minifigCatalogId;
    item.state = state;
    item.condition = condition;
    item.completeness = completeness;
    item.storageLocationId = storageLocationId;
    item.sourceBuildId = sourceBuildId;
    item.nickname = nickname;
    item.notes = notes;
    return create(item);
}

CollectionItemService::Result CollectionItemService::createMocFromBuild(
    int workspaceId, int sourceBuildId, CollectionItemState state,
    int storageLocationId, const QString& nickname, const QString& notes,
    CollectionItemCondition condition, CollectionItemCompleteness completeness) const
{
    CollectionItem item;
    item.workspaceId = workspaceId;
    item.type = CollectionItemType::Moc;
    item.state = state;
    item.condition = condition;
    item.completeness = completeness;
    item.storageLocationId = storageLocationId;
    item.sourceBuildId = sourceBuildId;
    item.nickname = nickname;
    item.notes = notes;
    return create(item);
}

CollectionItemService::Result CollectionItemService::createFromBuild(
    int workspaceId, int sourceBuildId, CollectionItemState state,
    int storageLocationId, const QString& nickname, const QString& notes,
    CollectionItemCondition condition, CollectionItemCompleteness completeness) const
{
    const auto build = BuildRepository().getById(sourceBuildId);
    if (!build) return failure(Error::SourceBuildNotFound, "The source Build is unavailable.");
    if (!build->isActive()) return failure(Error::SourceBuildMismatch, "Only an active Build can be added to My Collection.");
    if (build->workspaceId() != workspaceId) return failure(Error::SourceBuildMismatch, "The source Build belongs to another workspace.");
    if (build->status() != QStringLiteral("Complete")) return failure(Error::SourceBuildIncomplete, "Only a completed Build can be added to My Collection.");
    if (build->buildType() == QStringLiteral("Set")) {
        if (build->setCatalogId() <= 0) return failure(Error::CatalogItemNotFound, "This historical Set Build is not linked to a Sets Catalog definition.");
        return createSet(workspaceId, build->setCatalogId(), state, storageLocationId,
                         sourceBuildId, nickname, notes, condition, completeness);
    }
    if (build->buildType() == QStringLiteral("Minifig")) {
        if (build->minifigCatalogId() <= 0) return failure(Error::CatalogItemNotFound, "This Minifig Build is not linked to a Minifigs Catalog definition.");
        return createMinifig(workspaceId, build->minifigCatalogId(), state,
                             storageLocationId, sourceBuildId, nickname, notes,
                             condition, completeness);
    }
    if (build->buildType() == QStringLiteral("MOC")) {
        if (build->setCatalogId() > 0 || build->minifigCatalogId() > 0)
            return failure(Error::SourceBuildMismatch, "The MOC Build has invalid catalog identity data.");
        return createMocFromBuild(workspaceId, sourceBuildId, state, storageLocationId,
                                  nickname, notes, condition, completeness);
    }
    return failure(Error::SourceBuildMismatch, "This Build type cannot be added to My Collection.");
}

CollectionItemService::Result CollectionItemService::validate(
    CollectionItem& item, bool creating, int preservedLocationId) const
{
    if (item.workspaceId <= 0 || item.type == CollectionItemType::Invalid
        || item.state == CollectionItemState::Invalid
        || item.condition == CollectionItemCondition::Invalid
        || item.completeness == CollectionItemCompleteness::Invalid) {
        return failure(Error::InvalidInput,
                       "Collection type, state, condition, completeness, and workspace are required.");
    }
    if (!WorkspaceRepository().getById(item.workspaceId))
        return failure(Error::WorkspaceNotFound, "The selected workspace is unavailable.");

    if (item.type == CollectionItemType::Set) {
        if (item.setCatalogId <= 0 || item.minifigCatalogId > 0)
            return failure(Error::InvalidInput, "A Set Collection item requires one Set catalog identity.");
        if (!SetCatalogRepository().getById(item.setCatalogId))
            return failure(Error::CatalogItemNotFound, "The selected Set catalog item is unavailable.");
    } else if (item.type == CollectionItemType::Minifig) {
        if (item.minifigCatalogId <= 0 || item.setCatalogId > 0 || item.allowPartsSource)
            return failure(Error::InvalidInput, "A Minifig Collection item has an invalid identity or parts-source setting.");
        if (!MinifigCatalogRepository().getById(item.minifigCatalogId))
            return failure(Error::CatalogItemNotFound, "The selected Minifig catalog item is unavailable.");
    } else if (item.setCatalogId > 0 || item.minifigCatalogId > 0 || item.sourceBuildId <= 0) {
        return failure(Error::InvalidInput, "A MOC Collection item requires a source MOC Build and no catalog identity.");
    }

    if (item.storageLocationId > 0 && item.storageLocationId != preservedLocationId
        && !StorageLocationRepository().isValidCollectionDestination(
            item.workspaceId, item.storageLocationId)) {
        return failure(Error::LocationInvalid,
                       "Select an active Collection-capable leaf location in this workspace.");
    }

    if (item.sourceBuildId > 0) {
        const auto build = BuildRepository().getById(item.sourceBuildId);
        if (!build)
            return failure(Error::SourceBuildNotFound, "The source Build is unavailable.");
        if (build->workspaceId() != item.workspaceId)
            return failure(Error::SourceBuildMismatch, "The source Build belongs to another workspace.");
        if (creating && build->status() != QStringLiteral("Complete"))
            return failure(Error::SourceBuildIncomplete, "Only a completed Build can enter the Collection.");
        const QString expectedType = collectionItemTypeToString(item.type);
        if (build->buildType() != expectedType)
            return failure(Error::SourceBuildMismatch, "The source Build type does not match the Collection item.");
        if (item.type == CollectionItemType::Set
            && build->setCatalogId() != item.setCatalogId)
            return failure(Error::SourceBuildMismatch, "The Set Build catalog identity does not match.");
        if (item.type == CollectionItemType::Minifig
            && build->minifigCatalogId() != item.minifigCatalogId)
            return failure(Error::SourceBuildMismatch, "The Minifig Build catalog identity does not match.");
        if (creating && CollectionRepository().hasSourceBuild(item.sourceBuildId))
            return failure(Error::SourceBuildAlreadyUsed,
                           "This Build already has a Collection item.");
    }
    return {true, Error::None, {}, item.id};
}

CollectionItemService::Result CollectionItemService::create(CollectionItem item) const
{
    Result validation = validate(item, true);
    if (!validation.success) return validation;
    QSqlDatabase database = DatabaseManager::instance().database();
    if (!database.transaction())
        return failure(Error::DatabaseFailure, database.lastError().text());
    if (!CollectionRepository().create(item)) {
        const QString error = database.lastError().text();
        database.rollback();
        return failure(Error::DatabaseFailure,
                       error.isEmpty() ? "Unable to create the Collection item." : error);
    }
    if (!database.commit()) {
        const QString error = database.lastError().text();
        database.rollback();
        return failure(Error::DatabaseFailure, error);
    }
    return {true, Error::None, "Collection item created.", item.id};
}

CollectionItemService::Result CollectionItemService::updateDetails(
    int itemId, CollectionItemState state, int storageLocationId,
    const QString& nickname, const QString& notes, bool allowPartsSource,
    CollectionItemCondition condition, CollectionItemCompleteness completeness) const
{
    CollectionRepository repository;
    const auto existing = repository.getById(itemId);
    if (!existing) return failure(Error::ItemNotFound, "The Collection item is unavailable.");
    CollectionItem item = *existing;
    item.state = state;
    item.condition = condition;
    item.completeness = completeness;
    item.storageLocationId = storageLocationId;
    item.nickname = nickname;
    item.notes = notes;
    item.allowPartsSource = allowPartsSource;
    Result validation = validate(item, false, existing->storageLocationId);
    if (!validation.success) return validation;
    QSqlDatabase database = DatabaseManager::instance().database();
    if (!database.transaction())
        return failure(Error::DatabaseFailure, database.lastError().text());
    if (!repository.update(item)) {
        database.rollback();
        return failure(Error::DatabaseFailure, "Unable to update the Collection item.");
    }
    if (!database.commit()) {
        const QString error = database.lastError().text();
        database.rollback();
        return failure(Error::DatabaseFailure, error);
    }
    return {true, Error::None, "Collection item updated.", item.id};
}

CollectionItemService::Result CollectionItemService::updateStateForDisassembly(
    int sourceBuildId, CollectionItemState state) const
{
    if (state == CollectionItemState::Invalid || sourceBuildId <= 0)
        return failure(Error::InvalidInput, "A valid resulting Collection state is required.");
    CollectionRepository repository;
    std::optional<CollectionItem> item;
    if (!repository.tryGetBySourceBuild(sourceBuildId, item))
        return failure(Error::DatabaseFailure,
                       "Unable to check for a linked Collection item.");
    if (!item) return {true, Error::None, {}, 0};
    if (!repository.updateStateForSourceBuild(sourceBuildId, state))
        return failure(Error::DatabaseFailure,
                       "Unable to synchronize the linked Collection item state.");
    return {true, Error::None, "Collection state synchronized.", item->id};
}

CollectionItemService::SetLinkPreview CollectionItemService::previewLegacySetBuildLink(
    int buildId) const
{
    SetLinkPreview preview;
    const auto build = BuildRepository().getById(buildId);
    if (!build) {
        preview.result = failure(Error::SourceBuildNotFound, "The Build is unavailable.");
        return preview;
    }
    if (build->buildType() != QStringLiteral("Set") || build->setNumber().trimmed().isEmpty()) {
        preview.result = failure(Error::SourceBuildMismatch,
                                 "Only a Set Build with a usable reference can be linked.");
        return preview;
    }
    if (build->setCatalogId() > 0) {
        preview.result = failure(Error::SourceBuildAlreadyLinked,
                                 "This Set Build is already linked to the Sets Catalog.");
        return preview;
    }
    bool querySucceeded = false;
    const auto matches = SetCatalogRepository().getExactMatchesBySetNumber(
        build->setNumber(), &querySucceeded);
    if (!querySucceeded) {
        preview.result = failure(Error::DatabaseFailure,
                                 "Unable to search the Sets Catalog.");
        return preview;
    }
    if (matches.isEmpty()) {
        preview.result = failure(Error::CatalogMatchNotFound,
            "No exact Sets Catalog match was found for this Build reference.");
        return preview;
    }
    if (matches.size() != 1) {
        preview.result = failure(Error::CatalogMatchAmbiguous,
            "More than one exact Sets Catalog match exists; the Build was not linked.");
        return preview;
    }
    preview.result = {true, Error::None, {}, 0};
    preview.setCatalogId = matches.first().id();
    preview.buildName = build->name();
    preview.buildReference = build->setNumber();
    preview.catalogName = matches.first().name();
    preview.catalogReference = matches.first().setNumber();
    return preview;
}

CollectionItemService::Result CollectionItemService::linkLegacySetBuild(
    int buildId, int expectedSetCatalogId) const
{
    const SetLinkPreview preview = previewLegacySetBuildLink(buildId);
    if (!preview.result.success) return preview.result;
    if (preview.setCatalogId != expectedSetCatalogId)
        return failure(Error::SourceBuildMismatch,
                       "The Sets Catalog match changed; review the link again.");
    QSqlDatabase database = DatabaseManager::instance().database();
    if (!database.transaction())
        return failure(Error::DatabaseFailure, database.lastError().text());
    if (!BuildRepository().linkSetCatalog(buildId, expectedSetCatalogId)) {
        database.rollback();
        return failure(Error::DatabaseFailure, "Unable to link the Set Build.");
    }
    if (!database.commit()) {
        const QString error = database.lastError().text();
        database.rollback();
        return failure(Error::DatabaseFailure, error);
    }
    return {true, Error::None, "Set Build linked to the Sets Catalog.", 0};
}

CollectionItemService::Result CollectionItemService::setActive(int itemId, bool active) const
{
    CollectionRepository repository;
    if (!repository.getById(itemId))
        return failure(Error::ItemNotFound, "The Collection item is unavailable.");
    QSqlDatabase database = DatabaseManager::instance().database();
    if (!database.transaction())
        return failure(Error::DatabaseFailure, database.lastError().text());
    if (!repository.setActive(itemId, active)) {
        database.rollback();
        return failure(Error::DatabaseFailure, "Unable to update Collection activity.");
    }
    if (!database.commit()) {
        const QString error = database.lastError().text();
        database.rollback();
        return failure(Error::DatabaseFailure, error);
    }
    return {true, Error::None, active ? "Collection item reactivated."
                                      : "Collection item archived.", itemId};
}
