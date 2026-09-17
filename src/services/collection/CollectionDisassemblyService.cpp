#include "CollectionDisassemblyService.h"

#include "../../database/DatabaseManager.h"
#include "../../models/InventoryRecord.h"
#include "../../repositories/CollectionRepository.h"
#include "../../repositories/EffectiveSetCompositionRepository.h"
#include "../../repositories/InventoryRecordRepository.h"
#include "../../repositories/ManufacturerRepository.h"
#include "../../repositories/MinifigCatalogPartRepository.h"
#include "../../repositories/PartRepository.h"
#include "../../repositories/ColorRepository.h"
#include "../../repositories/StorageLocationRepository.h"
#include "../application/HostOperationalGate.h"

#include <QMap>
#include <QPair>
#include <QSet>
#include <QSqlError>

namespace {
CollectionDisassemblyService::Plan planFailure(
    CollectionDisassemblyService::Error error, const QString& message)
{
    CollectionDisassemblyService::Plan result;
    result.error = error; result.message = message; return result;
}
CollectionDisassemblyService::Result resultFailure(
    CollectionDisassemblyService::Error error, const QString& message)
{
    CollectionDisassemblyService::Result result;
    result.error = error; result.message = message; return result;
}
}

CollectionDisassemblyService::CollectionDisassemblyService()
    : CollectionDisassemblyService(DatabaseManager::instance().database()) {}

CollectionDisassemblyService::CollectionDisassemblyService(const QSqlDatabase& database)
    : m_connectionName(database.connectionName())
{
    Q_ASSERT(database.isValid()); Q_ASSERT(!m_connectionName.isEmpty());
}

QSqlDatabase CollectionDisassemblyService::database() const
{ return QSqlDatabase::database(m_connectionName, false); }

CollectionDisassemblyService::Plan CollectionDisassemblyService::preview(int itemId) const
{ return buildPlan(itemId); }

CollectionDisassemblyService::Plan CollectionDisassemblyService::buildPlan(int itemId) const
{
    if (itemId <= 0) return planFailure(Error::InvalidInput, "Select a Collection item.");
    const auto display = CollectionRepository(database()).displayById(itemId);
    if (!display) return planFailure(Error::NotFound, "The Collection item is unavailable.");
    const CollectionItem& item = display->item;
    if (!item.isActive) return planFailure(Error::Ineligible, "Archived Collection items cannot be disassembled.");
    if (item.state != CollectionItemState::Assembled)
        return planFailure(Error::Ineligible, "Only an Assembled Collection item can be disassembled.");
    if (item.completeness != CollectionItemCompleteness::Complete)
        return planFailure(Error::Ineligible, "Only a Complete Collection item can be disassembled.");
    if (item.sourceBuildId > 0)
        return planFailure(Error::Unsupported, "Build-linked Collection disassembly is not available in this release phase.");
    if (item.type != CollectionItemType::Set && item.type != CollectionItemType::Minifig)
        return planFailure(Error::Unsupported, "Only catalog Sets and Minifigs are supported in this release phase.");

    Plan result; result.item=item; result.reference=display->displayReference;
    result.name=display->displayName;
    QMap<QPair<int,int>, Row> aggregated;
    auto append = [&](int partId, int colorId, int quantity, bool spare,
                      const QString& number, const QString& name, const QString& color) {
        if (spare) { ++result.excludedSpareRows; result.excludedSparePieces += qMax(0, quantity); return; }
        if (partId <= 0 || colorId <= 0 || quantity <= 0) return;
        const QPair<int,int> key(partId,colorId);
        auto row=aggregated.value(key); row.partId=partId; row.colorId=colorId;
        row.quantity+=quantity; row.partNumber=number; row.partName=name; row.colorName=color;
        aggregated.insert(key,row);
    };
    if (item.type == CollectionItemType::Set) {
        const auto composition=EffectiveSetCompositionRepository(database()).forSet(item.setCatalogId,true);
        if (!composition.success)
            return planFailure(Error::DatabaseFailure, composition.message.isEmpty()
                ? QStringLiteral("Unable to read the Set composition.") : composition.message);
        for (const auto& part:composition.parts)
            append(part.partId,part.colorId,int(part.quantity),part.spare,
                   part.partNumber,part.partName,part.colorName);
    } else {
        const auto parts=MinifigCatalogPartRepository(database()).listForMinifig(item.minifigCatalogId);
        for (const auto& part:parts)
            append(part.partId,part.colorId,part.quantityRequired,part.isSpare,
                   part.partNumber,part.partName,part.colorName);
    }
    result.rows=aggregated.values();
    for (const Row& row:result.rows) {
        const auto part=PartRepository(database()).getById(row.partId);
        const auto color=ColorRepository(database()).getById(row.colorId);
        if (!part || !part->isActive() || !color)
            return planFailure(Error::CompositionUnavailable,
                "The authoritative composition contains an unavailable Part or Color.");
        result.totalPieces+=row.quantity;
    }
    if (result.rows.isEmpty())
        return planFailure(Error::CompositionUnavailable,
            "The Collection item has no required catalog composition to disassemble.");
    result.success=true; return result;
}

CollectionDisassemblyService::Result CollectionDisassemblyService::disassemble(
    int itemId, const QDateTime& expectedModifiedUtc,
    const QList<DestinationAssignment>& assignments) const
{
    if (!expectedModifiedUtc.isValid()) return resultFailure(Error::InvalidInput,"The Collection version is invalid.");
    if (database().connectionName()==QStringLiteral("qt_sql_default_connection")
        && !HostOperationalGate::localWritesAllowed())
        return resultFailure(Error::InvalidInput,"Host Maintenance prevents operational changes.");
    QSqlDatabase db=database();
    if (!db.transaction()) return resultFailure(Error::DatabaseFailure,db.lastError().text());
    const Result result = disassembleInCurrentTransaction(
        itemId, expectedModifiedUtc, assignments);
    if (!result.success) { db.rollback(); return result; }
    if (!db.commit()) { const QString error=db.lastError().text(); db.rollback();
        return resultFailure(Error::DatabaseFailure,error); }
    return result;
}

CollectionDisassemblyService::Result
CollectionDisassemblyService::disassembleInCurrentTransaction(
    int itemId, const QDateTime& expectedModifiedUtc,
    const QList<DestinationAssignment>& assignments) const
{
    if (!expectedModifiedUtc.isValid()) return resultFailure(Error::InvalidInput,"The Collection version is invalid.");
    QSqlDatabase db=database();
    const Plan plan=buildPlan(itemId);
    if (!plan.success) return resultFailure(plan.error,plan.message);
    if (plan.item.modifiedUtc != expectedModifiedUtc.toUTC()) {
        return resultFailure(Error::Stale,"The Collection item changed. Review it and try again.");
    }
    QMap<QPair<int,int>,int> requiredQuantities;
    QMap<QPair<int,int>,int> assignedQuantities;
    QMap<QPair<int,int>,Row> rowsByIdentity;
    for (const Row& row : plan.rows) {
        requiredQuantities.insert({row.partId,row.colorId},row.quantity);
        rowsByIdentity.insert({row.partId,row.colorId},row);
    }
    for (const DestinationAssignment& assignment : assignments) {
        const QPair<int,int> key(assignment.partId,assignment.colorId);
        if (!requiredQuantities.contains(key) || assignment.quantity<=0
            || assignment.storageLocationId<=0) {
            return resultFailure(Error::InvalidInput,
                "The Collection return plan contains an invalid Part, Color, quantity, or destination.");
        }
        if (!StorageLocationRepository(db).isValidInventoryDestination(
                plan.item.workspaceId,assignment.storageLocationId)) {
            return resultFailure(Error::InvalidDestination,
                "Every returned Part requires an active Inventory-capable leaf destination in this Workspace.");
        }
        assignedQuantities[key]+=assignment.quantity;
    }
    if (assignedQuantities!=requiredQuantities) {
        return resultFailure(Error::InvalidInput,
            "The Collection return plan must include the full required quantity for every Part and Color.");
    }
    const int manufacturerId=ManufacturerRepository(db).legoManufacturerId();
    if (manufacturerId<=0) return resultFailure(Error::DatabaseFailure,"The LEGO manufacturer identity is unavailable.");
    InventoryRecordRepository inventory(db);
    Result result; result.collectionItemId=itemId;
    const QString reference=QString::number(itemId);
    const QString notes=QStringLiteral("Required piece returned while disassembling Collection item %1 (%2).")
        .arg(itemId).arg(plan.reference);
    for (const DestinationAssignment& assignment:assignments) {
        const QPair<int,int> key(assignment.partId,assignment.colorId);
        const Row row=rowsByIdentity.value(key);
        InventoryRecord record; record.setWorkspaceId(plan.item.workspaceId);
        record.setPartId(row.partId); record.setColorId(row.colorId);
        record.setStorageLocationId(assignment.storageLocationId); record.setManufacturerId(manufacturerId);
        record.setCondition(collectionItemConditionToString(plan.item.condition));
        record.setOwnershipType(QStringLiteral("Owned")); record.setQuantity(assignment.quantity);
        InventoryRecordRepository::AddResult added;
        if (!inventory.addOrIncreaseQuantityInCurrentTransaction(record,
                QStringLiteral("CollectionDisassembly"),QStringLiteral("Collection"),reference,notes,&added)) {
            return resultFailure(Error::DatabaseFailure,"Unable to add a Collection piece to Inventory.");
        }
        result.affectedInventoryIds.append(added.inventoryRecordId);
        result.totalPieces+=assignment.quantity; ++result.distinctRows;
    }
    if (!CollectionRepository(db).transitionCatalogItemToUnassembled(
            itemId,plan.item.workspaceId,expectedModifiedUtc)) {
        return resultFailure(Error::Stale,
            "The Collection item changed. No Inventory was added; review it and try again.");
    }
    result.success=true;
    result.message=QStringLiteral("Added %1 required piece(s) across %2 Inventory row(s). The Collection item is now Unassembled.")
        .arg(result.totalPieces).arg(result.distinctRows);
    return result;
}
