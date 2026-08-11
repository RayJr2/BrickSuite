#include "InventoryRecordRepository.h"

#include "../database/DatabaseManager.h"

#include <QDateTime>
#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>

bool InventoryRecordRepository::create(InventoryRecord& record)
{
    if (record.workspaceId() <= 0 || record.partId() <= 0 || record.colorId() <= 0
        || record.storageLocationId() <= 0 || record.quantity() < 0) {
        return false;
    }

    QSqlDatabase database = DatabaseManager::instance().database();

    const QDateTime now = QDateTime::currentDateTimeUtc();

    QSqlQuery query(database);

    query.prepare(R"(
        INSERT INTO inventory_record
        (
            workspace_id,
            part_id,
            color_id,
            storage_location_id,
            condition,
            ownership_type,
            quantity,
            created_utc,
            modified_utc
        )
        VALUES
        (
            :workspace_id,
            :part_id,
            :color_id,
            :storage_location_id,
            :condition,
            :ownership_type,
            :quantity,
            :created_utc,
            :modified_utc
        )
    )");

    query.bindValue(":workspace_id", record.workspaceId());
    query.bindValue(":part_id", record.partId());
    query.bindValue(":color_id", record.colorId());
    query.bindValue(":storage_location_id", record.storageLocationId());
    query.bindValue(":condition", record.condition().trimmed());
    query.bindValue(":ownership_type", record.ownershipType().trimmed());
    query.bindValue(":quantity", record.quantity());
    query.bindValue(":created_utc", now.toString(Qt::ISODateWithMs));
    query.bindValue(":modified_utc", now.toString(Qt::ISODateWithMs));

    if (!query.exec()) {
        qCritical() << "Unable to create inventory record:" << query.lastError().text();

        return false;
    }

    record.setId(query.lastInsertId().toInt());

    record.setCreatedUtc(now);
    record.setModifiedUtc(now);

    return true;
}

QList<InventoryRecord> InventoryRecordRepository::getByWorkspace(int workspaceId) const
{
    QList<InventoryRecord> records;

    QSqlDatabase database = DatabaseManager::instance().database();

    QSqlQuery query(database);

    query.prepare(R"(
        SELECT
            id,
            workspace_id,
            part_id,
            color_id,
            storage_location_id,
            condition,
            ownership_type,
            quantity,
            created_utc,
            modified_utc
        FROM inventory_record
        WHERE workspace_id = :workspace_id
        ORDER BY part_id, color_id, storage_location_id
    )");

    query.bindValue(":workspace_id", workspaceId);

    if (!query.exec()) {
        qCritical() << "Unable to retrieve workspace inventory:" << query.lastError().text();

        return records;
    }

    while (query.next())
        records.append(inventoryRecordFromQuery(query));

    return records;
}

QList<InventoryRecord> InventoryRecordRepository::getByStorageLocation(int workspaceId,
                                                                       int storageLocationId) const
{
    QList<InventoryRecord> records;

    QSqlDatabase database = DatabaseManager::instance().database();

    QSqlQuery query(database);

    query.prepare(R"(
        SELECT
            id,
            workspace_id,
            part_id,
            color_id,
            storage_location_id,
            condition,
            ownership_type,
            quantity,
            created_utc,
            modified_utc
        FROM inventory_record
        WHERE workspace_id = :workspace_id
          AND storage_location_id = :storage_location_id
        ORDER BY part_id, color_id
    )");

    query.bindValue(":workspace_id", workspaceId);
    query.bindValue(":storage_location_id", storageLocationId);

    if (!query.exec()) {
        qCritical() << "Unable to retrieve storage inventory:" << query.lastError().text();

        return records;
    }

    while (query.next())
        records.append(inventoryRecordFromQuery(query));

    return records;
}

std::optional<InventoryRecord> InventoryRecordRepository::getById(int id) const
{
    QSqlDatabase database = DatabaseManager::instance().database();

    QSqlQuery query(database);

    query.prepare(R"(
        SELECT
            id,
            workspace_id,
            part_id,
            color_id,
            storage_location_id,
            condition,
            ownership_type,
            quantity,
            created_utc,
            modified_utc
        FROM inventory_record
        WHERE id = :id
    )");

    query.bindValue(":id", id);

    if (!query.exec()) {
        qCritical() << "Unable to retrieve inventory record:" << query.lastError().text();

        return std::nullopt;
    }

    if (!query.next())
        return std::nullopt;

    return inventoryRecordFromQuery(query);
}

bool InventoryRecordRepository::updateQuantity(int inventoryRecordId, int quantity)
{
    if (inventoryRecordId <= 0 || quantity < 0) {
        return false;
    }

    QSqlDatabase database = DatabaseManager::instance().database();

    const QString now = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);

    QSqlQuery query(database);

    query.prepare(R"(
        UPDATE inventory_record
        SET
            quantity = :quantity,
            modified_utc = :modified_utc
        WHERE id = :id
    )");

    query.bindValue(":quantity", quantity);
    query.bindValue(":modified_utc", now);
    query.bindValue(":id", inventoryRecordId);

    if (!query.exec()) {
        qCritical() << "Unable to update inventory quantity:" << query.lastError().text();

        return false;
    }

    return query.numRowsAffected() > 0;
}

InventoryRecord InventoryRecordRepository::inventoryRecordFromQuery(const QSqlQuery& query) const
{
    InventoryRecord record;

    record.setId(query.value("id").toInt());

    record.setWorkspaceId(query.value("workspace_id").toInt());

    record.setPartId(query.value("part_id").toInt());

    record.setColorId(query.value("color_id").toInt());

    record.setStorageLocationId(query.value("storage_location_id").toInt());

    record.setCondition(query.value("condition").toString());

    record.setOwnershipType(query.value("ownership_type").toString());

    record.setQuantity(query.value("quantity").toInt());

    record.setCreatedUtc(
        QDateTime::fromString(query.value("created_utc").toString(), Qt::ISODateWithMs));

    record.setModifiedUtc(
        QDateTime::fromString(query.value("modified_utc").toString(), Qt::ISODateWithMs));

    return record;
}