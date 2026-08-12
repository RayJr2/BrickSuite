#include "InventoryRecordRepository.h"

#include "../database/DatabaseManager.h"

#include <QDateTime>
#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>
#include <QtGlobal>

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

QList<InventorySearchResult> InventoryRecordRepository::search(
    const InventorySearchCriteria& criteria) const
{
    QList<InventorySearchResult> results;

    if (criteria.workspaceId <= 0)
        return results;

    QSqlDatabase database = DatabaseManager::instance().database();

    QString sql = R"(
        SELECT
            ir.id AS inventory_record_id,

            p.id AS part_id,
            p.part_number,
            p.name AS part_name,

            pc.id AS category_id,
            pc.name AS category_name,

            c.id AS color_id,
            c.name AS color_name,

            sl.id AS storage_location_id,
            sl.name AS storage_location_name,

            ir.condition,
            ir.ownership_type,
            ir.quantity

        FROM inventory_record ir

        INNER JOIN part p
            ON p.id = ir.part_id

        LEFT JOIN part_category pc
            ON pc.id = p.part_category_id

        INNER JOIN color c
            ON c.id = ir.color_id

        INNER JOIN storage_location sl
            ON sl.id = ir.storage_location_id

        WHERE ir.workspace_id = :workspace_id
    )";

    const QString searchText = criteria.searchText.trimmed();

    if (!searchText.isEmpty()) {
        sql += R"(
            AND
            (
                p.part_number LIKE :search
                OR p.name LIKE :search
            )
        )";
    }

    if (criteria.categoryId > 0) {
        sql += R"(
            AND p.part_category_id = :category_id
        )";
    }

    if (criteria.colorId > 0) {
        sql += R"(
            AND ir.color_id = :color_id
        )";
    }

    if (criteria.storageLocationId > 0) {
        sql += R"(
            AND ir.storage_location_id = :storage_location_id
        )";
    }

    sql += R"(
        ORDER BY
            p.part_number,
            c.name,
            sl.name

        LIMIT :limit
        OFFSET :offset
    )";

    QSqlQuery query(database);

    if (!query.prepare(sql)) {
        qCritical() << "Unable to prepare inventory search:" << query.lastError().text();

        return results;
    }

    query.bindValue(":workspace_id", criteria.workspaceId);

    if (!searchText.isEmpty()) {
        query.bindValue(":search", "%" + searchText + "%");
    }

    if (criteria.categoryId > 0) {
        query.bindValue(":category_id", criteria.categoryId);
    }

    if (criteria.colorId > 0) {
        query.bindValue(":color_id", criteria.colorId);
    }

    if (criteria.storageLocationId > 0) {
        query.bindValue(":storage_location_id", criteria.storageLocationId);
    }

    const int safeLimit = qBound(1, criteria.limit, 500);

    const int safeOffset = qMax(0, criteria.offset);

    query.bindValue(":limit", safeLimit);

    query.bindValue(":offset", safeOffset);

    if (!query.exec()) {
        qCritical() << "Unable to search inventory:" << query.lastError().text();

        return results;
    }

    while (query.next()) {
        InventorySearchResult result;

        result.inventoryRecordId = query.value("inventory_record_id").toInt();

        result.partId = query.value("part_id").toInt();

        result.partNumber = query.value("part_number").toString();

        result.partName = query.value("part_name").toString();

        if (!query.value("category_id").isNull()) {
            result.categoryId = query.value("category_id").toInt();
        }

        result.categoryName = query.value("category_name").toString();

        result.colorId = query.value("color_id").toInt();

        result.colorName = query.value("color_name").toString();

        result.storageLocationId = query.value("storage_location_id").toInt();

        result.storageLocationName = query.value("storage_location_name").toString();

        result.condition = query.value("condition").toString();

        result.ownershipType = query.value("ownership_type").toString();

        result.quantity = query.value("quantity").toInt();

        results.append(result);
    }

    return results;
}

bool InventoryRecordRepository::addOrIncreaseQuantity(InventoryRecord& record)
{
    if (record.workspaceId() <= 0 || record.partId() <= 0 || record.colorId() <= 0
        || record.storageLocationId() <= 0 || record.quantity() <= 0) {
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

        ON CONFLICT
        (
            workspace_id,
            part_id,
            color_id,
            storage_location_id,
            condition,
            ownership_type
        )
        DO UPDATE SET
            quantity =
                inventory_record.quantity
                + excluded.quantity,

            modified_utc =
                excluded.modified_utc
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
        qCritical() << "Unable to add inventory quantity:" << query.lastError().text();

        return false;
    }

    record.setModifiedUtc(now);

    return true;
}

bool InventoryRecordRepository::updateOrMerge(InventoryRecord& record)
{
    if (record.id() <= 0 || record.workspaceId() <= 0 || record.partId() <= 0
        || record.colorId() <= 0 || record.storageLocationId() <= 0 || record.quantity() < 0) {
        return false;
    }

    QSqlDatabase database = DatabaseManager::instance().database();

    if (!database.transaction()) {
        qCritical() << "Unable to begin inventory update transaction:"
                    << database.lastError().text();

        return false;
    }

    // Look for another record that already represents
    // the destination combination.
    QSqlQuery existingQuery(database);

    existingQuery.prepare(R"(
        SELECT
            id,
            quantity
        FROM inventory_record
        WHERE workspace_id = :workspace_id
          AND part_id = :part_id
          AND color_id = :color_id
          AND storage_location_id = :storage_location_id
          AND condition = :condition
          AND ownership_type = :ownership_type
          AND id <> :id
        LIMIT 1
    )");

    existingQuery.bindValue(":workspace_id", record.workspaceId());

    existingQuery.bindValue(":part_id", record.partId());

    existingQuery.bindValue(":color_id", record.colorId());

    existingQuery.bindValue(":storage_location_id", record.storageLocationId());

    existingQuery.bindValue(":condition", record.condition().trimmed());

    existingQuery.bindValue(":ownership_type", record.ownershipType().trimmed());

    existingQuery.bindValue(":id", record.id());

    if (!existingQuery.exec()) {
        qCritical() << "Unable to check inventory merge destination:"
                    << existingQuery.lastError().text();

        database.rollback();
        return false;
    }

    const QString now = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);

    if (existingQuery.next()) {
        const int destinationId = existingQuery.value("id").toInt();

        const int destinationQuantity = existingQuery.value("quantity").toInt();

        const int mergedQuantity = destinationQuantity + record.quantity();

        QSqlQuery mergeQuery(database);

        mergeQuery.prepare(R"(
            UPDATE inventory_record
            SET
                quantity = :quantity,
                modified_utc = :modified_utc
            WHERE id = :id
        )");

        mergeQuery.bindValue(":quantity", mergedQuantity);

        mergeQuery.bindValue(":modified_utc", now);

        mergeQuery.bindValue(":id", destinationId);

        if (!mergeQuery.exec()) {
            qCritical() << "Unable to merge inventory records:" << mergeQuery.lastError().text();

            database.rollback();
            return false;
        }

        QSqlQuery deleteQuery(database);

        deleteQuery.prepare(R"(
            DELETE FROM inventory_record
            WHERE id = :id
        )");

        deleteQuery.bindValue(":id", record.id());

        if (!deleteQuery.exec()) {
            qCritical() << "Unable to remove merged inventory record:"
                        << deleteQuery.lastError().text();

            database.rollback();
            return false;
        }

        record.setId(destinationId);
        record.setQuantity(mergedQuantity);
    } else {
        QSqlQuery updateQuery(database);

        updateQuery.prepare(R"(
            UPDATE inventory_record
            SET
                color_id = :color_id,
                storage_location_id = :storage_location_id,
                condition = :condition,
                ownership_type = :ownership_type,
                quantity = :quantity,
                modified_utc = :modified_utc
            WHERE id = :id
        )");

        updateQuery.bindValue(":color_id", record.colorId());

        updateQuery.bindValue(":storage_location_id", record.storageLocationId());

        updateQuery.bindValue(":condition", record.condition().trimmed());

        updateQuery.bindValue(":ownership_type", record.ownershipType().trimmed());

        updateQuery.bindValue(":quantity", record.quantity());

        updateQuery.bindValue(":modified_utc", now);

        updateQuery.bindValue(":id", record.id());

        if (!updateQuery.exec()) {
            qCritical() << "Unable to update inventory record:" << updateQuery.lastError().text();

            database.rollback();
            return false;
        }
    }

    if (!database.commit()) {
        qCritical() << "Unable to commit inventory update:" << database.lastError().text();

        database.rollback();
        return false;
    }

    record.setModifiedUtc(QDateTime::currentDateTimeUtc());

    return true;
}

bool InventoryRecordRepository::remove(int inventoryRecordId)
{
    if (inventoryRecordId <= 0)
        return false;

    QSqlDatabase database = DatabaseManager::instance().database();

    QSqlQuery query(database);

    query.prepare(R"(
        DELETE FROM inventory_record
        WHERE id = :id
    )");

    query.bindValue(":id", inventoryRecordId);

    if (!query.exec()) {
        qCritical() << "Unable to remove inventory record:" << query.lastError().text();

        return false;
    }

    return query.numRowsAffected() > 0;
}