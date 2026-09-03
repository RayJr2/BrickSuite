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
    const QString& notes) const
{
    CollectionItem item;
    item.workspaceId = workspaceId;
    item.type = CollectionItemType::Set;
    item.setCatalogId = setCatalogId;
    item.state = state;
    item.storageLocationId = storageLocationId;
    item.sourceBuildId = sourceBuildId;
    item.nickname = nickname;
    item.notes = notes;
    return create(item);
}

CollectionItemService::Result CollectionItemService::createMinifig(
    int workspaceId, int minifigCatalogId, CollectionItemState state,
    int storageLocationId, int sourceBuildId, const QString& nickname,
    const QString& notes) const
{
    CollectionItem item;
    item.workspaceId = workspaceId;
    item.type = CollectionItemType::Minifig;
    item.minifigCatalogId = minifigCatalogId;
    item.state = state;
    item.storageLocationId = storageLocationId;
    item.sourceBuildId = sourceBuildId;
    item.nickname = nickname;
    item.notes = notes;
    return create(item);
}

CollectionItemService::Result CollectionItemService::createMocFromBuild(
    int workspaceId, int sourceBuildId, CollectionItemState state,
    int storageLocationId, const QString& nickname, const QString& notes) const
{
    CollectionItem item;
    item.workspaceId = workspaceId;
    item.type = CollectionItemType::Moc;
    item.state = state;
    item.storageLocationId = storageLocationId;
    item.sourceBuildId = sourceBuildId;
    item.nickname = nickname;
    item.notes = notes;
    return create(item);
}

CollectionItemService::Result CollectionItemService::validate(
    CollectionItem& item, bool creating) const
{
    if (item.workspaceId <= 0 || item.type == CollectionItemType::Invalid
        || item.state == CollectionItemState::Invalid) {
        return failure(Error::InvalidInput, "Collection type, state, and workspace are required.");
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

    if (item.storageLocationId > 0
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
    const QString& nickname, const QString& notes, bool allowPartsSource) const
{
    CollectionRepository repository;
    const auto existing = repository.getById(itemId);
    if (!existing) return failure(Error::ItemNotFound, "The Collection item is unavailable.");
    CollectionItem item = *existing;
    item.state = state;
    item.storageLocationId = storageLocationId;
    item.nickname = nickname;
    item.notes = notes;
    item.allowPartsSource = allowPartsSource;
    Result validation = validate(item, false);
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
