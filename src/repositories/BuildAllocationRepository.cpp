#include "BuildAllocationRepository.h"

#include "../database/DatabaseManager.h"

#include <QDateTime>
#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>

bool BuildAllocationRepository::create(BuildAllocation& allocation)
{
    if (allocation.buildId() <= 0 || allocation.inventoryRecordId() <= 0 || allocation.partId() <= 0
        || allocation.colorId() <= 0 || allocation.storageLocationId() <= 0
        || allocation.quantityAllocated() <= 0) {
        qWarning() << "Build allocation create rejected due to invalid arguments."
                   << "BuildId:" << allocation.buildId()
                   << "InventoryRecordId:" << allocation.inventoryRecordId()
                   << "PartId:" << allocation.partId()
                   << "ColorId:" << allocation.colorId()
                   << "StorageLocationId:" << allocation.storageLocationId()
                   << "QuantityAllocated:" << allocation.quantityAllocated();
        return false;
    }

    QSqlDatabase database = DatabaseManager::instance().database();

    const QDateTime now = QDateTime::currentDateTimeUtc();

    QSqlQuery query(database);

    query.prepare(R"(
        INSERT INTO build_allocation
        (
            build_id,
            inventory_record_id,
            part_id,
            color_id,
            storage_location_id,
            quantity_allocated,
            created_utc,
            modified_utc
        )
        VALUES
        (
            :build_id,
            :inventory_record_id,
            :part_id,
            :color_id,
            :storage_location_id,
            :quantity_allocated,
            :created_utc,
            :modified_utc
        )
    )");

    query.bindValue(":build_id", allocation.buildId());

    query.bindValue(":inventory_record_id", allocation.inventoryRecordId());

    query.bindValue(":part_id", allocation.partId());

    query.bindValue(":color_id", allocation.colorId());

    query.bindValue(":storage_location_id", allocation.storageLocationId());

    query.bindValue(":quantity_allocated", allocation.quantityAllocated());

    query.bindValue(":created_utc", now.toString(Qt::ISODateWithMs));

    query.bindValue(":modified_utc", now.toString(Qt::ISODateWithMs));

    if (!query.exec()) {
        qCritical() << "Unable to create build allocation:" << query.lastError().text();

        return false;
    }

    allocation.setId(query.lastInsertId().toInt());

    allocation.setCreatedUtc(now);
    allocation.setModifiedUtc(now);

    return true;
}

std::optional<BuildAllocation> BuildAllocationRepository::getById(int id) const
{
    if (id <= 0)
        return std::nullopt;

    QSqlDatabase database = DatabaseManager::instance().database();

    QSqlQuery query(database);

    query.prepare(R"(
        SELECT
            id,
            build_id,
            inventory_record_id,
            part_id,
            color_id,
            storage_location_id,
            quantity_allocated,
            created_utc,
            modified_utc
        FROM build_allocation
        WHERE id = :id
    )");

    query.bindValue(":id", id);

    if (!query.exec()) {
        qCritical() << "Unable to retrieve build allocation:" << query.lastError().text();

        return std::nullopt;
    }

    if (!query.next())
        return std::nullopt;

    return allocationFromQuery(query);
}

QList<BuildAllocation> BuildAllocationRepository::getByBuild(int buildId) const
{
    QList<BuildAllocation> allocations;

    if (buildId <= 0)
        return allocations;

    QSqlDatabase database = DatabaseManager::instance().database();

    QSqlQuery query(database);

    query.prepare(R"(
        SELECT
            id,
            build_id,
            inventory_record_id,
            part_id,
            color_id,
            storage_location_id,
            quantity_allocated,
            created_utc,
            modified_utc
        FROM build_allocation
        WHERE build_id = :build_id
        ORDER BY
            part_id,
            color_id,
            storage_location_id
    )");

    query.bindValue(":build_id", buildId);

    if (!query.exec()) {
        qCritical() << "Unable to retrieve build allocations:" << query.lastError().text();

        return allocations;
    }

    while (query.next()) {
        allocations.append(allocationFromQuery(query));
    }

    return allocations;
}

QList<BuildAllocation> BuildAllocationRepository::getByInventoryRecord(int inventoryRecordId) const
{
    QList<BuildAllocation> allocations;

    if (inventoryRecordId <= 0)
        return allocations;

    QSqlDatabase database = DatabaseManager::instance().database();

    QSqlQuery query(database);

    query.prepare(R"(
        SELECT
            id,
            build_id,
            inventory_record_id,
            part_id,
            color_id,
            storage_location_id,
            quantity_allocated,
            created_utc,
            modified_utc
        FROM build_allocation
        WHERE inventory_record_id = :inventory_record_id
        ORDER BY build_id
    )");

    query.bindValue(":inventory_record_id", inventoryRecordId);

    if (!query.exec()) {
        qCritical() << "Unable to retrieve inventory build allocations:"
                    << query.lastError().text();

        return allocations;
    }

    while (query.next()) {
        allocations.append(allocationFromQuery(query));
    }

    return allocations;
}

bool BuildAllocationRepository::update(BuildAllocation& allocation)
{
    if (allocation.id() <= 0 || allocation.buildId() <= 0 || allocation.inventoryRecordId() <= 0
        || allocation.partId() <= 0 || allocation.colorId() <= 0
        || allocation.storageLocationId() <= 0 || allocation.quantityAllocated() <= 0) {
        return false;
    }

    QSqlDatabase database = DatabaseManager::instance().database();

    const QDateTime now = QDateTime::currentDateTimeUtc();

    QSqlQuery query(database);

    query.prepare(R"(
        UPDATE build_allocation
        SET
            build_id = :build_id,
            inventory_record_id = :inventory_record_id,
            part_id = :part_id,
            color_id = :color_id,
            storage_location_id = :storage_location_id,
            quantity_allocated = :quantity_allocated,
            modified_utc = :modified_utc
        WHERE id = :id
    )");

    query.bindValue(":build_id", allocation.buildId());

    query.bindValue(":inventory_record_id", allocation.inventoryRecordId());

    query.bindValue(":part_id", allocation.partId());

    query.bindValue(":color_id", allocation.colorId());

    query.bindValue(":storage_location_id", allocation.storageLocationId());

    query.bindValue(":quantity_allocated", allocation.quantityAllocated());

    query.bindValue(":modified_utc", now.toString(Qt::ISODateWithMs));

    query.bindValue(":id", allocation.id());

    if (!query.exec()) {
        qCritical() << "Unable to update build allocation:" << query.lastError().text();

        return false;
    }

    if (query.numRowsAffected() <= 0) {
        qWarning() << "Build allocation update affected no rows."
                   << "AllocationId:" << allocation.id();
        return false;
    }

    allocation.setModifiedUtc(now);

    return true;
}

bool BuildAllocationRepository::remove(int allocationId)
{
    if (allocationId <= 0) {
        qWarning() << "Build allocation remove rejected."
                   << "AllocationId:" << allocationId;
        return false;
    }

    QSqlDatabase database = DatabaseManager::instance().database();

    QSqlQuery query(database);

    query.prepare(R"(
        DELETE FROM build_allocation
        WHERE id = :id
    )");

    query.bindValue(":id", allocationId);

    if (!query.exec()) {
        qCritical() << "Unable to remove build allocation:" << query.lastError().text();

        return false;
    }

    if (query.numRowsAffected() <= 0) {
        qWarning() << "Build allocation remove affected no rows."
                   << "AllocationId:" << allocationId;
        return false;
    }

    return true;
}

bool BuildAllocationRepository::removeAllForBuild(int buildId)
{
    if (buildId <= 0) {
        qWarning() << "Remove all Build allocations rejected."
                   << "BuildId:" << buildId;
        return false;
    }

    QSqlDatabase database = DatabaseManager::instance().database();

    QSqlQuery query(database);

    query.prepare(R"(
        DELETE FROM build_allocation
        WHERE build_id = :build_id
    )");

    query.bindValue(":build_id", buildId);

    if (!query.exec()) {
        qCritical() << "Unable to remove build allocations:" << query.lastError().text();

        return false;
    }

    return true;
}

int BuildAllocationRepository::totalAllocatedForPartColor(int workspaceId,
                                                          int partId,
                                                          int colorId) const
{
    if (workspaceId <= 0 || partId <= 0 || colorId <= 0) {
        return 0;
    }

    QSqlDatabase database = DatabaseManager::instance().database();

    QSqlQuery query(database);

    query.prepare(R"(
        SELECT
            COALESCE(
                SUM(ba.quantity_allocated),
                0)
        FROM build_allocation ba
        INNER JOIN build b
            ON b.id = ba.build_id
        WHERE b.workspace_id = :workspace_id
          AND ba.part_id = :part_id
          AND ba.color_id = :color_id
    )");

    query.bindValue(":workspace_id", workspaceId);

    query.bindValue(":part_id", partId);

    query.bindValue(":color_id", colorId);

    if (!query.exec()) {
        qCritical() << "Unable to calculate total build allocation:" << query.lastError().text();

        return 0;
    }

    if (!query.next())
        return 0;

    return query.value(0).toInt();
}

int BuildAllocationRepository::totalAllocatedForPartColorForBuild(int buildId,
                                                                  int partId,
                                                                  int colorId) const
{
    if (buildId <= 0 || partId <= 0 || colorId <= 0) {
        return 0;
    }

    QSqlDatabase database = DatabaseManager::instance().database();

    QSqlQuery query(database);

    query.prepare(R"(
        SELECT
            COALESCE(
                SUM(quantity_allocated),
                0)
        FROM build_allocation
        WHERE build_id = :build_id
          AND part_id = :part_id
          AND color_id = :color_id
    )");

    query.bindValue(":build_id", buildId);

    query.bindValue(":part_id", partId);

    query.bindValue(":color_id", colorId);

    if (!query.exec()) {
        qCritical() << "Unable to calculate build allocation:" << query.lastError().text();

        return 0;
    }

    if (!query.next())
        return 0;

    return query.value(0).toInt();
}

BuildAllocation BuildAllocationRepository::allocationFromQuery(const QSqlQuery& query) const
{
    BuildAllocation allocation;

    allocation.setId(query.value("id").toInt());

    allocation.setBuildId(query.value("build_id").toInt());

    allocation.setInventoryRecordId(query.value("inventory_record_id").toInt());

    allocation.setPartId(query.value("part_id").toInt());

    allocation.setColorId(query.value("color_id").toInt());

    allocation.setStorageLocationId(query.value("storage_location_id").toInt());

    allocation.setQuantityAllocated(query.value("quantity_allocated").toInt());

    allocation.setCreatedUtc(
        QDateTime::fromString(query.value("created_utc").toString(), Qt::ISODateWithMs));

    allocation.setModifiedUtc(
        QDateTime::fromString(query.value("modified_utc").toString(), Qt::ISODateWithMs));

    return allocation;
}

int BuildAllocationRepository::totalAllocatedForInventoryRecord(int inventoryRecordId) const
{
    if (inventoryRecordId <= 0)
        return 0;

    QSqlDatabase database = DatabaseManager::instance().database();

    QSqlQuery query(database);

    query.prepare(R"(
        SELECT
            COALESCE(
                SUM(quantity_allocated),
                0)
        FROM build_allocation
        WHERE inventory_record_id =
            :inventory_record_id
    )");

    query.bindValue(":inventory_record_id", inventoryRecordId);

    if (!query.exec()) {
        qCritical() << "Unable to calculate inventory-record "
                       "allocation:"
                    << query.lastError().text();

        return 0;
    }

    if (!query.next())
        return 0;

    return query.value(0).toInt();
}

int BuildAllocationRepository::totalAllocatedForInventoryRecordForBuild(int inventoryRecordId,
                                                                        int buildId) const
{
    if (inventoryRecordId <= 0 || buildId <= 0) {
        return 0;
    }

    QSqlDatabase database = DatabaseManager::instance().database();

    QSqlQuery query(database);

    query.prepare(R"(
        SELECT
            COALESCE(
                SUM(quantity_allocated),
                0)
        FROM build_allocation
        WHERE inventory_record_id =
            :inventory_record_id
          AND build_id =
            :build_id
    )");

    query.bindValue(":inventory_record_id", inventoryRecordId);

    query.bindValue(":build_id", buildId);

    if (!query.exec()) {
        qCritical() << "Unable to calculate Build allocation "
                       "for inventory record:"
                    << query.lastError().text();

        return 0;
    }

    if (!query.next())
        return 0;

    return query.value(0).toInt();
}
