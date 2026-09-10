#include "HostInventoryMutationService.h"

#include "../../models/InventoryRecord.h"
#include "../../repositories/InventoryRecordRepository.h"

#include <QDateTime>
#include <QSqlQuery>

namespace {
HostWriteExecutor::MutationOutcome failure(const QString& code, const QString& message)
{
    HostWriteExecutor::MutationOutcome value;
    value.error={code, message, false};
    if (code == QStringLiteral("CONFLICT") || code == QStringLiteral("STALE_VERSION"))
        value.error.conflict={{QStringLiteral("refreshRequired"), true}};
    return value;
}

int scalarId(const QSqlDatabase& db, const QString& sql, const QVariant& value)
{
    QSqlQuery query(db); query.prepare(sql); query.bindValue(0, value);
    return query.exec() && query.next() ? query.value(0).toInt() : 0;
}

bool validWorkspace(const QSqlDatabase& db, qint64 id, bool* inactive)
{
    QSqlQuery query(db); query.prepare("SELECT is_active FROM workspace WHERE id=?"); query.addBindValue(id);
    if (!query.exec() || !query.next()) return false;
    *inactive = !query.value(0).toBool(); return true;
}

bool validStorage(const QSqlDatabase& db, qint64 id, qint64 workspace)
{
    QSqlQuery query(db);
    query.prepare("SELECT 1 FROM storage_location s WHERE s.id=? AND s.workspace_id=? "
                  "AND s.is_active=1 AND s.allows_inventory=1 AND NOT EXISTS "
                  "(SELECT 1 FROM storage_location child WHERE child.parent_location_id=s.id AND child.is_active=1)");
    query.addBindValue(id); query.addBindValue(workspace);
    return query.exec() && query.next();
}

bool expectedMatches(const InventoryRecord& record,
                     const RemoteInventoryMutationDto::ExpectedState& expected)
{
    return expected.inventoryRecordId == record.id()
        && expected.quantity == record.quantity()
        && expected.storageLocationId == record.storageLocationId()
        && !expected.modifiedUtc.isEmpty()
        && expected.modifiedUtc == record.modifiedUtc().toUTC().toString(Qt::ISODateWithMs);
}

QJsonObject authoritative(const InventoryRecordRepository& repository, int sourceId,
                          int destinationId, int survivingId, int sourceQuantity,
                          bool created, bool merged)
{
    const auto surviving=repository.getById(survivingId);
    return {{"sourceRecordId", sourceId}, {"destinationRecordId", destinationId},
        {"survivingRecordId", survivingId},
        {"resultingQuantity", surviving ? surviving->quantity() : 0},
        {"sourceQuantity", sourceQuantity},
        {"storageLocationId", surviving ? surviving->storageLocationId() : 0},
        {"modifiedUtc", surviving ? surviving->modifiedUtc().toUTC().toString(Qt::ISODateWithMs) : QString()},
        {"created", created}, {"merged", merged}};
}
}

HostWriteExecutor::Mutation HostInventoryMutationService::createMutation(
    const QString& operation, const RemoteMutationDto::Metadata& metadata,
    RemoteMutationDto::Error* error)
{
    RemoteInventoryMutationDto::Request request;
    if (!RemoteInventoryMutationDto::fromMetadata(operation, metadata, &request, error)) return {};

    return [operation, request](const QSqlDatabase& database) {
        bool inactive=false;
        if (!validWorkspace(database, request.workspaceId, &inactive))
            return failure(QStringLiteral("WORKSPACE_NOT_FOUND"), QStringLiteral("The Workspace does not exist."));
        if (inactive)
            return failure(QStringLiteral("WORKSPACE_INACTIVE"), QStringLiteral("The Workspace is inactive."));

        InventoryRecordRepository repository(database);
        const auto source = request.inventoryRecordId > 0
                                ? repository.getById(int(request.inventoryRecordId))
                                : std::optional<InventoryRecord>{};
        if (request.inventoryRecordId > 0
            && (!source || source->workspaceId() != request.workspaceId))
            return failure(QStringLiteral("NOT_FOUND"), QStringLiteral("The Inventory record is not available in this Workspace."));
        if (source && !expectedMatches(*source, request.expected))
            return failure(QStringLiteral("STALE_VERSION"), QStringLiteral("The Inventory record changed on the Host."));
        if (source && (operation == QStringLiteral("inventory.edit")
                       || operation == QStringLiteral("inventory.move")
                       || operation == QStringLiteral("inventory.correct")
                       || operation == QStringLiteral("inventory.markLost"))) {
            QSqlQuery allocatedQuery(database);
            allocatedQuery.prepare("SELECT COALESCE(SUM(quantity_allocated),0) FROM build_allocation "
                                   "WHERE inventory_record_id=?");
            allocatedQuery.addBindValue(source->id());
            if (!allocatedQuery.exec() || !allocatedQuery.next())
                return failure(QStringLiteral("INTERNAL_ERROR"), QStringLiteral("Unable to validate active Build allocations."));
            if (allocatedQuery.value(0).toInt() > 0)
                return failure(QStringLiteral("CONFLICT"),
                    QStringLiteral("Remote changes to Inventory allocated to an active Build are not permitted."));
        }

        const auto partId=[&](const QString& number) {
            return scalarId(database, "SELECT id FROM part WHERE part_number=? COLLATE NOCASE AND is_active=1", number);
        };
        const auto colorId=[&](int external) {
            return scalarId(database, "SELECT id FROM color WHERE rebrickable_id=?", external);
        };
        const auto manufacturerId=[&](const QString& name) {
            return scalarId(database, "SELECT id FROM manufacturer WHERE name=? COLLATE NOCASE AND is_active=1", name);
        };
        auto successful=[&](const QJsonObject& state, bool scopeSingle) {
            HostWriteExecutor::MutationOutcome outcome; outcome.success=true; outcome.authoritative=state;
            outcome.publicationWorkflow=HostMutationPublicationService::Workflow::Inventory;
            outcome.publicationScope.workspaceId=request.workspaceId;
            if (scopeSingle) outcome.publicationScope.inventoryRecordId=
                qint64(state.value("survivingRecordId").toDouble());
            return outcome;
        };

        if (operation == QStringLiteral("inventory.add")) {
            const int part=partId(request.partNumber), color=colorId(request.colorExternalId);
            const int manufacturer=manufacturerId(request.manufacturerName);
            if (!part || !color || !manufacturer)
                return failure(QStringLiteral("NOT_FOUND"), QStringLiteral("The Part, Color, or Manufacturer is not available on the Host."));
            if (!validStorage(database, request.storageLocationId, request.workspaceId))
                return failure(QStringLiteral("INVALID_ARGUMENT"), QStringLiteral("The destination Storage location cannot accept Inventory."));
            InventoryRecord record; record.setWorkspaceId(int(request.workspaceId)); record.setPartId(part);
            record.setColorId(color); record.setManufacturerId(manufacturer);
            record.setStorageLocationId(int(request.storageLocationId)); record.setQuantity(request.quantity);
            record.setCondition(request.condition); record.setOwnershipType(request.ownershipType);
            InventoryRecordRepository::AddResult result;
            if (!repository.addOrIncreaseQuantityInCurrentTransaction(record, {}, {}, {}, request.notes, &result))
                return failure(QStringLiteral("CONFLICT"), QStringLiteral("The Inventory could not be added."));
            return successful(authoritative(repository, 0, 0, result.inventoryRecordId, -1,
                                             result.created, result.merged), true);
        }

        if (operation == QStringLiteral("inventory.edit")) {
            const int part=partId(request.partNumber), color=colorId(request.colorExternalId);
            const int manufacturer=manufacturerId(request.manufacturerName);
            if (!part || !color || !manufacturer)
                return failure(QStringLiteral("NOT_FOUND"), QStringLiteral("The Part, Color, or Manufacturer is not available on the Host."));
            if (!validStorage(database, request.storageLocationId, request.workspaceId))
                return failure(QStringLiteral("INVALID_ARGUMENT"), QStringLiteral("The destination Storage location cannot accept Inventory."));
            InventoryRecord record=*source; record.setPartId(part); record.setColorId(color);
            record.setManufacturerId(manufacturer); record.setStorageLocationId(int(request.storageLocationId));
            record.setQuantity(request.quantity); record.setCondition(request.condition);
            record.setOwnershipType(request.ownershipType);
            InventoryRecordRepository::UpdateResult result;
            if (!repository.updateOrMergeInCurrentTransaction(record, &result))
                return failure(QStringLiteral("CONFLICT"), QStringLiteral("The Inventory could not be updated."));
            return successful(authoritative(repository, result.sourceRecordId, 0,
                result.survivingRecordId, result.merged ? 0 : result.resultingQuantity,
                false, result.merged), !result.merged);
        }

        if (operation == QStringLiteral("inventory.move")) {
            if (!validStorage(database, request.destinationStorageLocationId, request.workspaceId))
                return failure(QStringLiteral("INVALID_ARGUMENT"), QStringLiteral("The destination Storage location cannot accept Inventory."));
            InventoryRecordRepository::MoveResult result;
            if (!repository.moveInventoryInCurrentTransaction(int(request.inventoryRecordId),
                    int(request.destinationStorageLocationId), request.quantity, &result))
                return failure(QStringLiteral("CONFLICT"), QStringLiteral("The Inventory could not be moved."));
            QJsonObject state=authoritative(repository, result.sourceRecordId, result.destinationRecordId,
                result.destinationRecordId, result.resultingSourceQuantity,
                result.destinationCreated, !result.destinationCreated);
            return successful(state, false);
        }

        if (operation == QStringLiteral("inventory.correct")) {
            const int replacement=partId(request.partNumber);
            if (!replacement) return failure(QStringLiteral("NOT_FOUND"), QStringLiteral("The replacement Part is not available on the Host."));
            if (!repository.correctEntryInCurrentTransaction(int(request.inventoryRecordId), replacement,
                                                              request.quantity, request.notes))
                return failure(QStringLiteral("CONFLICT"), QStringLiteral("The Inventory correction was rejected."));
            QSqlQuery query(database); query.prepare("SELECT id FROM inventory_record WHERE workspace_id=? AND part_id=? "
                "AND color_id=? AND storage_location_id=? AND manufacturer_id=? AND condition=? AND ownership_type=? LIMIT 1");
            query.addBindValue(request.workspaceId); query.addBindValue(replacement); query.addBindValue(source->colorId());
            query.addBindValue(source->storageLocationId()); query.addBindValue(source->manufacturerId());
            query.addBindValue(source->condition()); query.addBindValue(source->ownershipType());
            const int destination=query.exec() && query.next() ? query.value(0).toInt() : 0;
            return successful(authoritative(repository, source->id(), destination, destination,
                source->quantity()-request.quantity, false, false), false);
        }

        if (operation == QStringLiteral("inventory.remove")) {
            QString message;
            if (!repository.removeEntryInCurrentTransaction(int(request.inventoryRecordId), request.quantity,
                                                             request.notes, &message))
                return failure(QStringLiteral("CONFLICT"), message);
            return successful(authoritative(repository, source->id(), 0, source->id(),
                source->quantity()-request.quantity, false, false), true);
        }

        if (operation == QStringLiteral("inventory.markLost")) {
            if (!repository.markLostInCurrentTransaction(int(request.inventoryRecordId), request.quantity, request.notes))
                return failure(QStringLiteral("CONFLICT"), QStringLiteral("The Lost quantity was rejected."));
            return successful(authoritative(repository, source->id(), 0, source->id(),
                source->quantity()-request.quantity, false, false), true);
        }

        const int part=partId(request.partNumber), color=colorId(request.colorExternalId);
        if (!part || !color)
            return failure(QStringLiteral("NOT_FOUND"), QStringLiteral("The Part or Color is not available on the Host."));
        if (!validStorage(database, request.destinationStorageLocationId, request.workspaceId))
            return failure(QStringLiteral("INVALID_ARGUMENT"), QStringLiteral("The destination Storage location cannot accept Inventory."));
        InventoryRecordRepository::FoundResult result;
        if (!repository.markFoundInCurrentTransaction(int(request.workspaceId), part, color,
                request.quantity, int(request.destinationStorageLocationId), request.condition,
                request.ownershipType, request.notes, &result))
            return failure(QStringLiteral("CONFLICT"), QStringLiteral("The Found quantity exceeds the outstanding Lost quantity."));
        return successful(authoritative(repository, 0, 0, result.inventoryRecordId, -1,
                                         result.created, result.merged), true);
    };
}
