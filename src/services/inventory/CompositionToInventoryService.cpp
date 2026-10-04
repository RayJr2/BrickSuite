#include "CompositionToInventoryService.h"
#include "../../models/InventoryRecord.h"
#include "../../repositories/InventoryRecordRepository.h"
#include "../../repositories/StorageLocationRepository.h"
#include <QSqlQuery>
#include <QVariant>
#include <limits>
#include <map>
#include <tuple>

namespace {
using Key = std::tuple<int, int, int>;
using Rows = std::map<Key, CompositionToInventoryService::Row>;
bool aggregate(const QList<CompositionToInventoryService::Row>& input, Rows& rows)
{
    for (const auto& row : input) {
        if (row.partId <= 0 || row.colorId <= 0 || row.storageId <= 0 || row.quantity <= 0
            || row.quantity > std::numeric_limits<int>::max()) return false;
        const Key key{row.partId, row.colorId, row.storageId};
        auto [it, inserted] = rows.emplace(key, row);
        if (!inserted) {
            if (it->second.quantity > std::numeric_limits<int>::max() - row.quantity) return false;
            it->second.quantity += row.quantity;
        }
    }
    return !rows.empty();
}
}

CompositionToInventoryService::Result CompositionToInventoryService::validate(
    const Context& context, const QList<Row>& input) const
{
    Result result;
    Rows rows;
    if (context.workspaceId <= 0 || context.manufacturerId <= 0
        || (context.condition != "Used" && context.condition != "New")
        || context.ownership != "Owned" || !aggregate(input, rows)) {
        result.message = QStringLiteral("Invalid inventory identity, quantity, or quantity overflow.");
        return result;
    }
    StorageLocationRepository storage(m_database);
    for (const auto& entry : rows) {
        const Row& row = entry.second;
        if (!storage.isValidInventoryDestination(context.workspaceId, row.storageId)) {
            result.message = QStringLiteral("Every destination must be an active Inventory-capable leaf in this Workspace.");
            return result;
        }
        QSqlQuery identity(m_database);
        identity.prepare("SELECT 1 FROM part p, color c, manufacturer m WHERE p.id=? AND p.is_active=1 AND c.id=? AND m.id=?");
        identity.addBindValue(row.partId); identity.addBindValue(row.colorId); identity.addBindValue(context.manufacturerId);
        if (!identity.exec() || !identity.next()) {
            result.message = QStringLiteral("A composition Part, Color, or manufacturer is unavailable."); return result;
        }
        QSqlQuery existing(m_database);
        existing.prepare("SELECT quantity FROM inventory_record WHERE workspace_id=? AND part_id=? AND color_id=? "
                         "AND storage_location_id=? AND manufacturer_id=? AND condition=? AND ownership_type=?");
        existing.addBindValue(context.workspaceId); existing.addBindValue(row.partId); existing.addBindValue(row.colorId);
        existing.addBindValue(row.storageId); existing.addBindValue(context.manufacturerId);
        existing.addBindValue(context.condition); existing.addBindValue(context.ownership);
        if (!existing.exec()) { result.message = QStringLiteral("Unable to validate existing Inventory quantities."); return result; }
        while (existing.next()) {
            const qint64 quantity = existing.value(0).toLongLong();
            if (quantity < 0 || quantity > std::numeric_limits<int>::max() - row.quantity) {
                result.message = QStringLiteral("Adding these pieces would overflow an Inventory quantity."); return result;
            }
        }
        if (result.totalPieces > std::numeric_limits<qint64>::max() - row.quantity) {
            result.message = QStringLiteral("The total piece quantity is too large."); return result;
        }
        result.totalPieces += row.quantity;
    }
    result.success = true;
    return result;
}

CompositionToInventoryService::Result CompositionToInventoryService::addInCurrentTransaction(
    const Context& context, const QList<Row>& input) const
{
    Result result = validate(context, input);
    if (!result.success) return result;
    InventoryRecordRepository inventory(m_database);
    // Preserve each caller-supplied movement while validating aggregate additions above.
    for (const Row& row : input) {
        InventoryRecord record;
        record.setWorkspaceId(context.workspaceId); record.setPartId(row.partId); record.setColorId(row.colorId);
        record.setStorageLocationId(row.storageId); record.setManufacturerId(context.manufacturerId);
        record.setCondition(context.condition); record.setOwnershipType(context.ownership); record.setQuantity(int(row.quantity));
        InventoryRecordRepository::AddResult added;
        if (!inventory.addOrIncreaseQuantityInCurrentTransaction(record, context.movementType,
                context.referenceType, context.referenceId, context.notes, &added)) {
            result.success = false; result.message = QStringLiteral("Unable to add a composition row and its Inventory history.");
            return result;
        }
        result.inventoryIds.append(added.inventoryRecordId);
    }
    return result;
}
