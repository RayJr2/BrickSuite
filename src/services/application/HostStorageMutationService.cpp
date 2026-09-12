#include "HostStorageMutationService.h"
#include "HostOperationalGate.h"

#include "../../repositories/StorageLocationRepository.h"
#include "../../repositories/StorageLocationTypeRepository.h"
#include "../../repositories/WorkspaceRepository.h"

#include <QSet>
#include <QThread>

namespace {
using Check = StorageLocationRepository::CheckResult;
}

HostStorageMutationService::HostStorageMutationService(const QSqlDatabase& database)
    : m_connectionName(database.connectionName()), m_ownerThread(QThread::currentThread())
{
    Q_ASSERT(database.isValid());
    Q_ASSERT(!m_connectionName.isEmpty());
}

QSqlDatabase HostStorageMutationService::database() const
{
    Q_ASSERT(QThread::currentThread() == m_ownerThread);
    return QSqlDatabase::database(m_connectionName, false);
}

HostStorageMutationService::Result HostStorageMutationService::fail(
    ErrorCode code, const QString& message) const
{
    Result result; result.error = {code, message}; return result;
}

HostStorageMutationService::ExpectedState
HostStorageMutationService::expectedState(const StorageLocation& value)
{
    ExpectedState expected; expected.provided = true; expected.modifiedUtc = value.modifiedUtc();
    expected.parentStorageId = value.parentLocationId(); expected.storageTypeId = value.locationTypeId();
    expected.name = value.name(); expected.description = value.description();
    expected.sortOrder = value.sortOrder(); expected.active = value.isActive();
    expected.allowsInventory = value.allowsInventory();
    expected.allowsCollection = value.allowsCollection(); return expected;
}

bool HostStorageMutationService::matchesExpected(
    const StorageLocation& value, const ExpectedState& expected) const
{
    return !expected.provided || (value.modifiedUtc() == expected.modifiedUtc
        && value.parentLocationId() == expected.parentStorageId
        && value.locationTypeId() == expected.storageTypeId && value.name() == expected.name
        && value.description() == expected.description && value.sortOrder() == expected.sortOrder
        && value.isActive() == expected.active
        && value.allowsInventory() == expected.allowsInventory
        && value.allowsCollection() == expected.allowsCollection);
}

HostStorageMutationService::Error HostStorageMutationService::validateWorkspaceAndType(
    int workspaceId, int storageTypeId) const
{
    if (workspaceId <= 0 || storageTypeId <= 0)
        return {ErrorCode::InvalidArgument, QStringLiteral("Workspace and Storage type are required.")};
    if (const Error error = validateWorkspace(workspaceId); error.code != ErrorCode::None)
        return error;
    std::optional<StorageLocationType> type;
    if (!StorageLocationTypeRepository(database()).tryGetById(storageTypeId, type))
        return {ErrorCode::DatabaseFailure, QStringLiteral("Unable to validate the Storage location type.")};
    if (!type) return {ErrorCode::TypeMissing, QStringLiteral("The Storage location type does not exist.")};
    if (!type->isActive()) return {ErrorCode::TypeInactive, QStringLiteral("The Storage location type is inactive.")};
    return {};
}

HostStorageMutationService::Error HostStorageMutationService::validateWorkspace(int workspaceId) const
{
    if (workspaceId <= 0)
        return {ErrorCode::InvalidArgument, QStringLiteral("The Workspace is invalid.")};
    std::optional<Workspace> workspace;
    if (!WorkspaceRepository(database()).tryGetById(workspaceId, workspace))
        return {ErrorCode::DatabaseFailure, QStringLiteral("Unable to validate the Workspace.")};
    if (!workspace) return {ErrorCode::WorkspaceMissing, QStringLiteral("The Workspace does not exist.")};
    if (!workspace->isActive()) return {ErrorCode::WorkspaceInactive, QStringLiteral("The Workspace is inactive.")};
    return {};
}

HostStorageMutationService::Error HostStorageMutationService::validateParent(
    int workspaceId, int parentStorageId) const
{
    if (parentStorageId == 0) return {};
    if (parentStorageId < 0)
        return {ErrorCode::InvalidArgument, QStringLiteral("The parent Storage location is invalid.")};
    std::optional<StorageLocation> parent;
    if (!StorageLocationRepository(database()).tryGetById(parentStorageId, parent))
        return {ErrorCode::DatabaseFailure, QStringLiteral("Unable to validate the parent Storage location.")};
    if (!parent) return {ErrorCode::ParentMissing, QStringLiteral("The parent Storage location does not exist.")};
    if (parent->workspaceId() != workspaceId)
        return {ErrorCode::ParentWrongWorkspace, QStringLiteral("The parent Storage location belongs to another Workspace.")};
    if (!parent->isActive())
        return {ErrorCode::ParentInactive, QStringLiteral("The parent Storage location is inactive.")};
    return {};
}

HostStorageMutationService::Result HostStorageMutationService::authoritative(int storageId) const
{
    StorageLocationRepository repository(database());
    std::optional<StorageLocation> value;
    if (!repository.tryGetById(storageId, value))
        return fail(ErrorCode::DatabaseFailure, QStringLiteral("Unable to reload the Storage location."));
    if (!value) return fail(ErrorCode::DatabaseFailure, QStringLiteral("Unable to reload the Storage location."));
    QStringList names; QSet<int> visited; int current = storageId;
    while (current > 0) {
        if (visited.size() >= 10000 || visited.contains(current))
            return fail(ErrorCode::DatabaseFailure, QStringLiteral("The Storage hierarchy is invalid."));
        visited.insert(current); std::optional<StorageLocation> row;
        if (!repository.tryGetById(current, row))
            return fail(ErrorCode::DatabaseFailure, QStringLiteral("Unable to resolve the Storage hierarchy."));
        if (!row || row->workspaceId() != value->workspaceId())
            return fail(ErrorCode::DatabaseFailure, QStringLiteral("Unable to resolve the Storage hierarchy."));
        names.prepend(row->name()); current = row->parentLocationId();
    }
    Result result; result.success = true; result.location = *value;
    result.displayPath = names.join(QStringLiteral(" / ")); return result;
}

HostStorageMutationService::Result HostStorageMutationService::add(const AddRequest& request) const
{
    if (database().connectionName() == QStringLiteral("qt_sql_default_connection")
        && !HostOperationalGate::localWritesAllowed())
        return fail(ErrorCode::InvalidArgument, QStringLiteral("Host Maintenance prevents operational changes."));
    const QString name = request.name.trimmed();
    if (name.isEmpty() || name.size() > MaximumNameLength
        || request.description.size() > MaximumDescriptionLength)
        return fail(ErrorCode::InvalidArgument, QStringLiteral("The Storage name or description is invalid."));
    if (const Error error = validateWorkspaceAndType(request.workspaceId, request.storageTypeId);
        error.code != ErrorCode::None) return fail(error.code, error.message);
    if (const Error error = validateParent(request.workspaceId, request.parentStorageId);
        error.code != ErrorCode::None) return fail(error.code, error.message);
    StorageLocation value; value.setWorkspaceId(request.workspaceId);
    value.setParentLocationId(request.parentStorageId); value.setLocationTypeId(request.storageTypeId);
    value.setName(name); value.setDescription(request.description); value.setSortOrder(0);
    value.setIsActive(true); value.setAllowsInventory(request.allowsInventory);
    value.setAllowsCollection(request.allowsCollection);
    if (!StorageLocationRepository(database()).create(value))
        return fail(ErrorCode::DatabaseFailure, QStringLiteral("Unable to create the Storage location."));
    return authoritative(value.id());
}

HostStorageMutationService::Result HostStorageMutationService::edit(const EditRequest& request) const
{
    if (database().connectionName() == QStringLiteral("qt_sql_default_connection")
        && !HostOperationalGate::localWritesAllowed())
        return fail(ErrorCode::InvalidArgument, QStringLiteral("Host Maintenance prevents operational changes."));
    const QString name = request.name.trimmed();
    if (request.storageId <= 0 || name.isEmpty() || name.size() > MaximumNameLength
        || request.description.size() > MaximumDescriptionLength)
        return fail(ErrorCode::InvalidArgument, QStringLiteral("The Storage edit is invalid."));
    StorageLocationRepository repository(database());
    std::optional<StorageLocation> current;
    if (!repository.tryGetById(request.storageId, current))
        return fail(ErrorCode::DatabaseFailure, QStringLiteral("Unable to load the Storage location."));
    if (!current || current->workspaceId() != request.workspaceId)
        return fail(ErrorCode::StorageMissing, QStringLiteral("The Storage location is not available in this Workspace."));
    if (!matchesExpected(*current, request.expected))
        return fail(ErrorCode::StaleExpectedState, QStringLiteral("The Storage location changed.") );
    if (const Error error = validateWorkspaceAndType(request.workspaceId, request.storageTypeId);
        error.code != ErrorCode::None) return fail(error.code, error.message);
    if (request.parentStorageId == request.storageId)
        return fail(ErrorCode::SelfParent, QStringLiteral("A Storage location cannot be its own parent."));
    if (const Error error = validateParent(request.workspaceId, request.parentStorageId);
        error.code != ErrorCode::None) return fail(error.code, error.message);
    if (request.parentStorageId > 0) {
        const Check cycle = repository.isDescendantChecked(request.storageId, request.parentStorageId);
        if (cycle == Check::Error) return fail(ErrorCode::DatabaseFailure, QStringLiteral("Unable to validate the Storage hierarchy."));
        if (cycle == Check::Yes) return fail(ErrorCode::DescendantCycle, QStringLiteral("A Storage location cannot be moved beneath its descendant."));
    }
    if (current->allowsInventory() && !request.allowsInventory) {
        const Check occupied = repository.hasInventoryChecked(request.storageId);
        if (occupied == Check::Error) return fail(ErrorCode::DatabaseFailure, QStringLiteral("Unable to validate Inventory occupancy."));
        if (occupied == Check::Yes) return fail(ErrorCode::InventoryOccupied, QStringLiteral("Move the loose Inventory before removing this capability."));
    }
    if (current->allowsCollection() && !request.allowsCollection) {
        const Check occupied = repository.hasCollectionChecked(request.storageId);
        if (occupied == Check::Error) return fail(ErrorCode::DatabaseFailure, QStringLiteral("Unable to validate Collection occupancy."));
        if (occupied == Check::Yes) return fail(ErrorCode::CollectionOccupied, QStringLiteral("Move the Collection items before removing this capability."));
    }
    StorageLocation updated = *current; updated.setParentLocationId(request.parentStorageId);
    updated.setLocationTypeId(request.storageTypeId); updated.setName(name);
    updated.setDescription(request.description); updated.setAllowsInventory(request.allowsInventory);
    updated.setAllowsCollection(request.allowsCollection);
    if (!repository.update(updated))
        return fail(ErrorCode::DatabaseFailure, QStringLiteral("Unable to update the Storage location."));
    return authoritative(updated.id());
}

HostStorageMutationService::Result HostStorageMutationService::setActive(
    const SetActiveRequest& request) const
{
    if (database().connectionName() == QStringLiteral("qt_sql_default_connection")
        && !HostOperationalGate::localWritesAllowed())
        return fail(ErrorCode::InvalidArgument, QStringLiteral("Host Maintenance prevents operational changes."));
    if (request.workspaceId <= 0 || request.storageId <= 0)
        return fail(ErrorCode::InvalidArgument, QStringLiteral("The Storage location is invalid."));
    StorageLocationRepository repository(database()); std::optional<StorageLocation> current;
    if (!repository.tryGetById(request.storageId, current))
        return fail(ErrorCode::DatabaseFailure, QStringLiteral("Unable to load the Storage location."));
    if (!current || current->workspaceId() != request.workspaceId)
        return fail(ErrorCode::StorageMissing, QStringLiteral("The Storage location is not available in this Workspace."));
    if (!matchesExpected(*current, request.expected))
        return fail(ErrorCode::StaleExpectedState, QStringLiteral("The Storage location changed."));
    if (current->isActive() == request.active)
        return fail(ErrorCode::InvalidArgument, QStringLiteral("The Storage location already has the requested active state."));
    if (const Error error = validateWorkspace(request.workspaceId);
        error.code != ErrorCode::None) return fail(error.code, error.message);
    if (request.active) {
        std::optional<StorageLocationType> type;
        if (!StorageLocationTypeRepository(database()).tryGetById(current->locationTypeId(), type))
            return fail(ErrorCode::DatabaseFailure, QStringLiteral("Unable to validate the Storage location type."));
        if (!type) return fail(ErrorCode::TypeMissing, QStringLiteral("The Storage location type does not exist."));
        if (!type->isActive()) return fail(ErrorCode::TypeInactive, QStringLiteral("The Storage location type is inactive."));
        if (const Error error = validateParent(request.workspaceId, current->parentLocationId());
            error.code != ErrorCode::None) return fail(error.code, error.message);
        if (!repository.reactivate(request.storageId))
            return fail(ErrorCode::DatabaseFailure, QStringLiteral("Unable to reactivate the Storage location."));
    } else {
        const Check children = repository.hasActiveChildrenChecked(request.storageId);
        const Check inventory = repository.hasInventoryChecked(request.storageId);
        const Check collection = repository.hasCollectionChecked(request.storageId);
        if (children == Check::Error || inventory == Check::Error || collection == Check::Error)
            return fail(ErrorCode::DatabaseFailure, QStringLiteral("Unable to validate Storage contents."));
        if (children == Check::Yes) return fail(ErrorCode::ActiveChildren, QStringLiteral("Move or deactivate active child locations first."));
        if (inventory == Check::Yes) return fail(ErrorCode::InventoryOccupied, QStringLiteral("Move the loose Inventory before deactivating this location."));
        if (collection == Check::Yes) return fail(ErrorCode::CollectionOccupied, QStringLiteral("Move the Collection items before deactivating this location."));
        if (!repository.deactivate(request.storageId))
            return fail(ErrorCode::DatabaseFailure, QStringLiteral("Unable to deactivate the Storage location."));
    }
    return authoritative(request.storageId);
}
