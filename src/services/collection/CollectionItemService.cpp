#include "CollectionItemService.h"

#include "../../database/DatabaseManager.h"
#include "../application/HostOperationalGate.h"
#include "../../models/Build.h"
#include "../../repositories/BuildRepository.h"
#include "../../repositories/CollectionRepository.h"
#include "../../repositories/MinifigCatalogRepository.h"
#include "../../repositories/SetCatalogRepository.h"
#include "../../repositories/StorageLocationRepository.h"
#include "../../repositories/WorkspaceRepository.h"

#include <QSqlError>

namespace {
CollectionItemService::Result failure(CollectionItemService::Error error, const QString& message)
{
    CollectionItemService::Result result;
    result.error = error;
    result.message = message;
    return result;
}

CollectionItemService::Result success(const CollectionItem& item, const QString& message,
                                      bool changed = true)
{
    CollectionItemService::Result result;
    result.success = true;
    result.message = message;
    result.collectionItemId = item.id;
    result.item = item;
    result.changed = changed;
    return result;
}
}

CollectionItemService::CollectionItemService()
    : CollectionItemService(DatabaseManager::instance().database()) {}

CollectionItemService::CollectionItemService(const QSqlDatabase& database)
    : m_connectionName(database.connectionName())
{
    Q_ASSERT(database.isValid());
    Q_ASSERT(!m_connectionName.isEmpty());
}

QSqlDatabase CollectionItemService::database() const
{
    return QSqlDatabase::database(m_connectionName, false);
}

CollectionItemService::Result CollectionItemService::inTransaction(
    const std::function<Result()>& operation) const
{
    if (database().connectionName() == QStringLiteral("qt_sql_default_connection")
        && !HostOperationalGate::localWritesAllowed())
        return failure(Error::InvalidInput, QStringLiteral("Host Maintenance prevents operational changes."));
    QSqlDatabase db = database();
    if (!db.transaction()) return failure(Error::DatabaseFailure, db.lastError().text());
    Result result = operation();
    if (!result.success) { db.rollback(); return result; }
    if (!db.commit()) {
        const QString error = db.lastError().text();
        db.rollback();
        return failure(Error::DatabaseFailure, error);
    }
    return result;
}

CollectionItemService::Result CollectionItemService::createSet(
    int workspaceId, int setCatalogId, CollectionItemState state, int storageLocationId,
    int sourceBuildId, const QString& nickname, const QString& notes,
    CollectionItemCondition condition, CollectionItemCompleteness completeness) const
{
    return inTransaction([&] { return createSetInCurrentTransaction(workspaceId, setCatalogId,
        state, storageLocationId, sourceBuildId, nickname, notes, condition, completeness); });
}

CollectionItemService::Result CollectionItemService::createSetInCurrentTransaction(
    int workspaceId, int setCatalogId, CollectionItemState state, int storageLocationId,
    int sourceBuildId, const QString& nickname, const QString& notes,
    CollectionItemCondition condition, CollectionItemCompleteness completeness) const
{
    CollectionItem item;
    item.workspaceId = workspaceId; item.type = CollectionItemType::Set;
    item.setCatalogId = setCatalogId; item.state = state; item.condition = condition;
    item.completeness = completeness; item.storageLocationId = storageLocationId;
    item.sourceBuildId = sourceBuildId; item.nickname = nickname; item.notes = notes;
    return createInCurrentTransaction(item);
}

CollectionItemService::Result CollectionItemService::createMinifig(
    int workspaceId, int minifigCatalogId, CollectionItemState state, int storageLocationId,
    int sourceBuildId, const QString& nickname, const QString& notes,
    CollectionItemCondition condition, CollectionItemCompleteness completeness) const
{
    return inTransaction([&] { return createMinifigInCurrentTransaction(workspaceId,
        minifigCatalogId, state, storageLocationId, sourceBuildId, nickname, notes,
        condition, completeness); });
}

CollectionItemService::Result CollectionItemService::createMinifigInCurrentTransaction(
    int workspaceId, int minifigCatalogId, CollectionItemState state, int storageLocationId,
    int sourceBuildId, const QString& nickname, const QString& notes,
    CollectionItemCondition condition, CollectionItemCompleteness completeness) const
{
    CollectionItem item;
    item.workspaceId = workspaceId; item.type = CollectionItemType::Minifig;
    item.minifigCatalogId = minifigCatalogId; item.state = state; item.condition = condition;
    item.completeness = completeness; item.storageLocationId = storageLocationId;
    item.sourceBuildId = sourceBuildId; item.nickname = nickname; item.notes = notes;
    return createInCurrentTransaction(item);
}

CollectionItemService::Result CollectionItemService::createMocFromBuild(
    int workspaceId, int sourceBuildId, CollectionItemState state, int storageLocationId,
    const QString& nickname, const QString& notes, CollectionItemCondition condition,
    CollectionItemCompleteness completeness) const
{
    return inTransaction([&] { return createMocFromBuildInCurrentTransaction(workspaceId,
        sourceBuildId, state, storageLocationId, nickname, notes, condition, completeness); });
}

CollectionItemService::Result CollectionItemService::createMocFromBuildInCurrentTransaction(
    int workspaceId, int sourceBuildId, CollectionItemState state, int storageLocationId,
    const QString& nickname, const QString& notes, CollectionItemCondition condition,
    CollectionItemCompleteness completeness) const
{
    CollectionItem item;
    item.workspaceId = workspaceId; item.type = CollectionItemType::Moc;
    item.state = state; item.condition = condition; item.completeness = completeness;
    item.storageLocationId = storageLocationId; item.sourceBuildId = sourceBuildId;
    item.nickname = nickname; item.notes = notes;
    return createInCurrentTransaction(item);
}

CollectionItemService::Result CollectionItemService::createFromBuild(
    int workspaceId, int sourceBuildId, CollectionItemState state, int storageLocationId,
    const QString& nickname, const QString& notes, CollectionItemCondition condition,
    CollectionItemCompleteness completeness) const
{
    return inTransaction([&] { return createFromBuildInCurrentTransaction(workspaceId,
        sourceBuildId, state, storageLocationId, nickname, notes, condition, completeness); });
}

CollectionItemService::Result CollectionItemService::createFromBuildInCurrentTransaction(
    int workspaceId, int sourceBuildId, CollectionItemState state, int storageLocationId,
    const QString& nickname, const QString& notes, CollectionItemCondition condition,
    CollectionItemCompleteness completeness) const
{
    std::optional<Build> build;
    if (!BuildRepository(database()).tryGetById(sourceBuildId, build))
        return failure(Error::DatabaseFailure, "Unable to read the source Build.");
    if (!build) return failure(Error::SourceBuildNotFound, "The source Build is unavailable.");
    if (!build->isActive()) return failure(Error::SourceBuildMismatch, "Only an active Build can be added to My Collection.");
    if (build->workspaceId() != workspaceId) return failure(Error::SourceBuildMismatch, "The source Build belongs to another workspace.");
    if (build->status() != QStringLiteral("Complete")) return failure(Error::SourceBuildIncomplete, "Only a completed Build can be added to My Collection.");
    if (build->buildType() == QStringLiteral("Set")) {
        if (build->setCatalogId() <= 0) return failure(Error::CatalogItemNotFound, "This historical Set Build is not linked to a Sets Catalog definition.");
        return createSetInCurrentTransaction(workspaceId, build->setCatalogId(), state,
            storageLocationId, sourceBuildId, nickname, notes, condition, completeness);
    }
    if (build->buildType() == QStringLiteral("Minifig")) {
        if (build->minifigCatalogId() <= 0) return failure(Error::CatalogItemNotFound, "This Minifig Build is not linked to a Minifigs Catalog definition.");
        return createMinifigInCurrentTransaction(workspaceId, build->minifigCatalogId(), state,
            storageLocationId, sourceBuildId, nickname, notes, condition, completeness);
    }
    if (build->buildType() == QStringLiteral("MOC")) {
        if (build->setCatalogId() > 0 || build->minifigCatalogId() > 0)
            return failure(Error::SourceBuildMismatch, "The MOC Build has invalid catalog identity data.");
        return createMocFromBuildInCurrentTransaction(workspaceId, sourceBuildId, state,
            storageLocationId, nickname, notes, condition, completeness);
    }
    return failure(Error::SourceBuildMismatch, "This Build type cannot be added to My Collection.");
}

CollectionItemService::Result CollectionItemService::validate(
    CollectionItem& item, bool creating, int preservedLocationId) const
{
    if (item.workspaceId <= 0 || item.type == CollectionItemType::Invalid
        || item.state == CollectionItemState::Invalid || item.condition == CollectionItemCondition::Invalid
        || item.completeness == CollectionItemCompleteness::Invalid)
        return failure(Error::InvalidInput, "Collection type, state, condition, completeness, and workspace are required.");

    std::optional<Workspace> workspace;
    if (!WorkspaceRepository(database()).tryGetById(item.workspaceId, workspace))
        return failure(Error::DatabaseFailure, "Unable to validate the selected workspace.");
    if (!workspace) return failure(Error::WorkspaceNotFound, "The selected workspace is unavailable.");

    if (item.type == CollectionItemType::Set) {
        if (item.setCatalogId <= 0 || item.minifigCatalogId > 0)
            return failure(Error::InvalidInput, "A Set Collection item requires one Set catalog identity.");
        std::optional<SetCatalogItem> catalog;
        if (!SetCatalogRepository(database()).tryGetById(item.setCatalogId, catalog))
            return failure(Error::DatabaseFailure, "Unable to validate the selected Set catalog item.");
        if (!catalog) return failure(Error::CatalogItemNotFound, "The selected Set catalog item is unavailable.");
    } else if (item.type == CollectionItemType::Minifig) {
        if (item.minifigCatalogId <= 0 || item.setCatalogId > 0 || item.allowPartsSource)
            return failure(Error::InvalidInput, "A Minifig Collection item has an invalid identity or parts-source setting.");
        std::optional<MinifigCatalogItem> catalog;
        if (!MinifigCatalogRepository(database()).tryGetById(item.minifigCatalogId, catalog))
            return failure(Error::DatabaseFailure, "Unable to validate the selected Minifig catalog item.");
        if (!catalog) return failure(Error::CatalogItemNotFound, "The selected Minifig catalog item is unavailable.");
    } else if (item.setCatalogId > 0 || item.minifigCatalogId > 0 || item.sourceBuildId <= 0) {
        return failure(Error::InvalidInput, "A MOC Collection item requires a source MOC Build and no catalog identity.");
    }

    if (item.storageLocationId > 0 && item.storageLocationId != preservedLocationId) {
        const auto check = StorageLocationRepository(database()).isValidCollectionDestinationChecked(
            item.workspaceId, item.storageLocationId);
        if (check == StorageLocationRepository::CheckResult::Error)
            return failure(Error::DatabaseFailure, "Unable to validate the selected Collection location.");
        if (check != StorageLocationRepository::CheckResult::Yes)
            return failure(Error::LocationInvalid, "Select an active Collection-capable leaf location in this workspace.");
    }

    if (item.sourceBuildId > 0) {
        std::optional<Build> build;
        if (!BuildRepository(database()).tryGetById(item.sourceBuildId, build))
            return failure(Error::DatabaseFailure, "Unable to validate the source Build.");
        if (!build) return failure(Error::SourceBuildNotFound, "The source Build is unavailable.");
        if (build->workspaceId() != item.workspaceId) return failure(Error::SourceBuildMismatch, "The source Build belongs to another workspace.");
        if (creating && build->status() != QStringLiteral("Complete")) return failure(Error::SourceBuildIncomplete, "Only a completed Build can enter the Collection.");
        if (build->buildType() != collectionItemTypeToString(item.type)) return failure(Error::SourceBuildMismatch, "The source Build type does not match the Collection item.");
        if (item.type == CollectionItemType::Set && build->setCatalogId() != item.setCatalogId) return failure(Error::SourceBuildMismatch, "The Set Build catalog identity does not match.");
        if (item.type == CollectionItemType::Minifig && build->minifigCatalogId() != item.minifigCatalogId) return failure(Error::SourceBuildMismatch, "The Minifig Build catalog identity does not match.");
        if (creating) {
            bool used = false;
            if (!CollectionRepository(database()).tryHasSourceBuild(item.sourceBuildId, used))
                return failure(Error::DatabaseFailure, "Unable to validate the source Build linkage.");
            if (used) return failure(Error::SourceBuildAlreadyUsed, "This Build already has a Collection item.");
        }
    }
    return success(item, {}, false);
}

CollectionItemService::Result CollectionItemService::createInCurrentTransaction(CollectionItem item) const
{
    Result validation = validate(item, true);
    if (!validation.success) return validation;
    if (!CollectionRepository(database()).create(item))
        return failure(Error::DatabaseFailure, "Unable to create the Collection item.");
    return success(item, "Collection item created.");
}

CollectionItemService::Result CollectionItemService::updateDetails(
    int itemId, CollectionItemState state, int storageLocationId, const QString& nickname,
    const QString& notes, bool allowPartsSource, CollectionItemCondition condition,
    CollectionItemCompleteness completeness) const
{
    return inTransaction([&] { return updateDetailsInCurrentTransaction(itemId, state,
        storageLocationId, nickname, notes, allowPartsSource, condition, completeness); });
}

CollectionItemService::Result CollectionItemService::updateDetailsInCurrentTransaction(
    int itemId, CollectionItemState state, int storageLocationId, const QString& nickname,
    const QString& notes, bool allowPartsSource, CollectionItemCondition condition,
    CollectionItemCompleteness completeness) const
{
    CollectionRepository repository(database());
    std::optional<CollectionItem> existing;
    if (!repository.tryGetById(itemId, existing)) return failure(Error::DatabaseFailure, "Unable to read the Collection item.");
    if (!existing) return failure(Error::ItemNotFound, "The Collection item is unavailable.");
    CollectionItem item = *existing;
    item.state = state; item.condition = condition; item.completeness = completeness;
    item.storageLocationId = storageLocationId; item.nickname = nickname;
    item.notes = notes; item.allowPartsSource = allowPartsSource;
    Result validation = validate(item, false, existing->storageLocationId);
    if (!validation.success) return validation;
    if (!repository.update(item)) return failure(Error::DatabaseFailure, "Unable to update the Collection item.");
    return success(item, "Collection item updated.");
}

CollectionItemService::Result CollectionItemService::updateStateForDisassembly(
    int sourceBuildId, CollectionItemState state) const
{
    // This operation is deliberately transaction-neutral: Build disassembly owns
    // the transaction that covers inventory, Build, and Collection changes.
    return updateStateForDisassemblyInCurrentTransaction(sourceBuildId, state);
}

CollectionItemService::Result CollectionItemService::updateStateForDisassemblyInCurrentTransaction(
    int sourceBuildId, CollectionItemState state) const
{
    if (state == CollectionItemState::Invalid || sourceBuildId <= 0)
        return failure(Error::InvalidInput, "A valid resulting Collection state is required.");
    CollectionRepository repository(database());
    std::optional<CollectionItem> item;
    if (!repository.tryGetBySourceBuild(sourceBuildId, item)) return failure(Error::DatabaseFailure, "Unable to check for a linked Collection item.");
    if (!item) return success({}, {}, false);
    if (!repository.updateStateForSourceBuild(sourceBuildId, state)) return failure(Error::DatabaseFailure, "Unable to synchronize the linked Collection item state.");
    if (!repository.tryGetById(item->id, item) || !item) return failure(Error::DatabaseFailure, "Unable to reload the synchronized Collection item.");
    return success(*item, "Collection state synchronized.");
}

CollectionItemService::SetLinkPreview CollectionItemService::previewLegacySetBuildLink(int buildId) const
{
    SetLinkPreview preview;
    std::optional<Build> build;
    if (!BuildRepository(database()).tryGetById(buildId, build)) { preview.result = failure(Error::DatabaseFailure, "Unable to read the Build."); return preview; }
    if (!build) { preview.result = failure(Error::SourceBuildNotFound, "The Build is unavailable."); return preview; }
    if (build->buildType() != QStringLiteral("Set") || build->setNumber().trimmed().isEmpty()) { preview.result = failure(Error::SourceBuildMismatch, "Only a Set Build with a usable reference can be linked."); return preview; }
    if (build->setCatalogId() > 0) { preview.result = failure(Error::SourceBuildAlreadyLinked, "This Set Build is already linked to the Sets Catalog."); return preview; }
    bool querySucceeded = false;
    const auto matches = SetCatalogRepository(database()).getExactMatchesBySetNumber(build->setNumber(), &querySucceeded);
    if (!querySucceeded) { preview.result = failure(Error::DatabaseFailure, "Unable to search the Sets Catalog."); return preview; }
    if (matches.isEmpty()) { preview.result = failure(Error::CatalogMatchNotFound, "No exact Sets Catalog match was found for this Build reference."); return preview; }
    if (matches.size() != 1) { preview.result = failure(Error::CatalogMatchAmbiguous, "More than one exact Sets Catalog match exists; the Build was not linked."); return preview; }
    preview.result.success = true;
    preview.setCatalogId = matches.first().id(); preview.buildName = build->name();
    preview.buildReference = build->setNumber(); preview.catalogName = matches.first().name();
    preview.catalogReference = matches.first().setNumber();
    return preview;
}

CollectionItemService::Result CollectionItemService::linkLegacySetBuild(int buildId, int expectedSetCatalogId) const
{
    return inTransaction([&] { return linkLegacySetBuildInCurrentTransaction(buildId, expectedSetCatalogId); });
}

CollectionItemService::Result CollectionItemService::linkLegacySetBuildInCurrentTransaction(int buildId, int expectedSetCatalogId) const
{
    const SetLinkPreview preview = previewLegacySetBuildLink(buildId);
    if (!preview.result.success) return preview.result;
    if (preview.setCatalogId != expectedSetCatalogId) return failure(Error::SourceBuildMismatch, "The Sets Catalog match changed; review the link again.");
    if (!BuildRepository(database()).linkSetCatalog(buildId, expectedSetCatalogId)) return failure(Error::DatabaseFailure, "Unable to link the Set Build.");
    Result result; result.success = true; result.changed = true; result.message = "Set Build linked to the Sets Catalog.";
    return result;
}

CollectionItemService::Result CollectionItemService::setActive(int itemId, bool active) const
{
    return inTransaction([&] { return setActiveInCurrentTransaction(itemId, active); });
}

CollectionItemService::Result CollectionItemService::setActiveInCurrentTransaction(int itemId, bool active) const
{
    CollectionRepository repository(database());
    std::optional<CollectionItem> item;
    if (!repository.tryGetById(itemId, item)) return failure(Error::DatabaseFailure, "Unable to read the Collection item.");
    if (!item) return failure(Error::ItemNotFound, "The Collection item is unavailable.");
    if (!repository.setActive(itemId, active)) return failure(Error::DatabaseFailure, "Unable to update Collection activity.");
    if (!repository.tryGetById(itemId, item) || !item) return failure(Error::DatabaseFailure, "Unable to reload the Collection item.");
    return success(*item, active ? "Collection item reactivated." : "Collection item archived.");
}
