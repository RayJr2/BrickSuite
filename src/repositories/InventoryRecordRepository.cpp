#include "InventoryRecordRepository.h"

#include "../database/DatabaseManager.h"

#include "../models/InventoryMovement.h"
#include "InventoryMovementRepository.h"

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
            c.rgb AS color_rgb,

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
            AND ir.quantity > 0
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

        result.colorRgb = query.value("color_rgb").toString();

        result.storageLocationId = query.value("storage_location_id").toInt();

        result.storageLocationName = query.value("storage_location_name").toString();

        result.condition = query.value("condition").toString();

        result.ownershipType = query.value("ownership_type").toString();

        result.quantity = query.value("quantity").toInt();

        results.append(result);
    }

    return results;
}

bool InventoryRecordRepository::addOrIncreaseQuantity(InventoryRecord& record,
                                                      const QString& movementType,
                                                      const QString& referenceType,
                                                      const QString& referenceId,
                                                      const QString& notes,
                                                      bool manageTransaction)
{
    if (record.workspaceId() <= 0 || record.partId() <= 0 || record.colorId() <= 0
        || record.storageLocationId() <= 0 || record.quantity() <= 0) {
        return false;
    }

    QSqlDatabase database = DatabaseManager::instance().database();

    if (manageTransaction) {
        if (!database.transaction()) {
            qCritical() << "Unable to begin inventory add transaction:"
                        << database.lastError().text();

            return false;
        }
    }

    const QDateTime now = QDateTime::currentDateTimeUtc();

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
        LIMIT 1
    )");

    existingQuery.bindValue(":workspace_id", record.workspaceId());

    existingQuery.bindValue(":part_id", record.partId());

    existingQuery.bindValue(":color_id", record.colorId());

    existingQuery.bindValue(":storage_location_id", record.storageLocationId());

    existingQuery.bindValue(":condition", record.condition().trimmed());

    existingQuery.bindValue(":ownership_type", record.ownershipType().trimmed());

    if (!existingQuery.exec()) {
        qCritical() << "Unable to check existing inventory:" << existingQuery.lastError().text();

        if (manageTransaction)
            database.rollback();

        return false;
    }

    const int quantityAdded = record.quantity();

    bool existingRecord = false;

    if (existingQuery.next()) {
        existingRecord = true;

        const int inventoryRecordId = existingQuery.value("id").toInt();

        const int existingQuantity = existingQuery.value("quantity").toInt();

        const int newQuantity = existingQuantity + quantityAdded;

        QSqlQuery updateQuery(database);

        updateQuery.prepare(R"(
            UPDATE inventory_record
            SET
                quantity = :quantity,
                modified_utc = :modified_utc
            WHERE id = :id
        )");

        updateQuery.bindValue(":quantity", newQuantity);

        updateQuery.bindValue(":modified_utc", now.toString(Qt::ISODateWithMs));

        updateQuery.bindValue(":id", inventoryRecordId);

        if (!updateQuery.exec()) {
            qCritical() << "Unable to increase inventory quantity:"
                        << updateQuery.lastError().text();

            if (manageTransaction)
                database.rollback();

            return false;
        }

        record.setId(inventoryRecordId);

        record.setQuantity(newQuantity);
    } else {
        QSqlQuery insertQuery(database);

        insertQuery.prepare(R"(
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

        insertQuery.bindValue(":workspace_id", record.workspaceId());

        insertQuery.bindValue(":part_id", record.partId());

        insertQuery.bindValue(":color_id", record.colorId());

        insertQuery.bindValue(":storage_location_id", record.storageLocationId());

        insertQuery.bindValue(":condition", record.condition().trimmed());

        insertQuery.bindValue(":ownership_type", record.ownershipType().trimmed());

        insertQuery.bindValue(":quantity", quantityAdded);

        insertQuery.bindValue(":created_utc", now.toString(Qt::ISODateWithMs));

        insertQuery.bindValue(":modified_utc", now.toString(Qt::ISODateWithMs));

        if (!insertQuery.exec()) {
            qCritical() << "Unable to create inventory record:" << insertQuery.lastError().text();

            if (manageTransaction)
                database.rollback();

            return false;
        }

        record.setId(insertQuery.lastInsertId().toInt());
    }

    InventoryMovement movement;

    movement.setWorkspaceId(record.workspaceId());

    movement.setInventoryRecordId(record.id());

    movement.setPartId(record.partId());

    movement.setColorId(record.colorId());

    // If the caller specifies an event type, use it.
    // Otherwise retain normal manual-add behavior.
    if (!movementType.trimmed().isEmpty()) {
        movement.setMovementType(movementType.trimmed());
    } else {
        movement.setMovementType(existingRecord ? "QuantityIncrease" : "InitialAdd");
    }

    movement.setQuantityChange(quantityAdded);

    movement.setToStorageLocationId(record.storageLocationId());

    movement.setCondition(record.condition());

    movement.setOwnershipType(record.ownershipType());

    movement.setReferenceType(referenceType.trimmed());

    movement.setReferenceId(referenceId.trimmed());

    movement.setNotes(notes.trimmed());

    InventoryMovementRepository movementRepository;

    if (!movementRepository.create(movement)) {
        qCritical() << "Unable to create inventory movement.";

        if (manageTransaction)
            database.rollback();

        return false;
    }

    if (manageTransaction) {
        if (!database.commit()) {
            qCritical() << "Unable to commit inventory add transaction:"
                        << database.lastError().text();

            database.rollback();
            return false;
        }
    }

    if (!existingRecord) {
        record.setCreatedUtc(now);
    }

    record.setModifiedUtc(now);

    return true;
}

bool InventoryRecordRepository::updateOrMerge(InventoryRecord& record)
{
    if (record.id() <= 0 || record.workspaceId() <= 0 || record.partId() <= 0
        || record.colorId() <= 0 || record.storageLocationId() <= 0 || record.quantity() <= 0) {
        return false;
    }

    QSqlDatabase database = DatabaseManager::instance().database();

    if (!database.transaction()) {
        qCritical() << "Unable to begin inventory edit transaction:" << database.lastError().text();

        return false;
    }

    //
    // Load original record.
    //
    QSqlQuery originalQuery(database);

    originalQuery.prepare(R"(
        SELECT
            id,
            workspace_id,
            part_id,
            color_id,
            storage_location_id,
            condition,
            ownership_type,
            quantity
        FROM inventory_record
        WHERE id = :id
    )");

    originalQuery.bindValue(":id", record.id());

    if (!originalQuery.exec() || !originalQuery.next()) {
        qCritical() << "Unable to load original inventory record:"
                    << originalQuery.lastError().text();

        database.rollback();
        return false;
    }

    const int originalColorId = originalQuery.value("color_id").toInt();

    const int originalStorageLocationId = originalQuery.value("storage_location_id").toInt();

    const QString originalCondition = originalQuery.value("condition").toString();

    const QString originalOwnershipType = originalQuery.value("ownership_type").toString();

    const int originalQuantity = originalQuery.value("quantity").toInt();

    const QString now = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);

    //
    // Check whether the edited combination collides
    // with another existing inventory record.
    //
    QSqlQuery destinationQuery(database);

    destinationQuery.prepare(R"(
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

    destinationQuery.bindValue(":workspace_id", record.workspaceId());

    destinationQuery.bindValue(":part_id", record.partId());

    destinationQuery.bindValue(":color_id", record.colorId());

    destinationQuery.bindValue(":storage_location_id", record.storageLocationId());

    destinationQuery.bindValue(":condition", record.condition().trimmed());

    destinationQuery.bindValue(":ownership_type", record.ownershipType().trimmed());

    destinationQuery.bindValue(":id", record.id());

    if (!destinationQuery.exec()) {
        qCritical() << "Unable to check inventory edit destination:"
                    << destinationQuery.lastError().text();

        database.rollback();
        return false;
    }

    bool merged = false;
    int finalRecordId = record.id();

    if (destinationQuery.next()) {
        merged = true;

        const int destinationId = destinationQuery.value("id").toInt();

        const int destinationQuantity = destinationQuery.value("quantity").toInt();

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

        //
        // Keep the source record for history, but set
        // quantity to zero instead of deleting it.
        //
        QSqlQuery zeroSourceQuery(database);

        zeroSourceQuery.prepare(R"(
            UPDATE inventory_record
            SET
                quantity = 0,
                modified_utc = :modified_utc
            WHERE id = :id
        )");

        zeroSourceQuery.bindValue(":modified_utc", now);

        zeroSourceQuery.bindValue(":id", record.id());

        if (!zeroSourceQuery.exec()) {
            qCritical() << "Unable to zero merged source inventory:"
                        << zeroSourceQuery.lastError().text();

            database.rollback();
            return false;
        }

        finalRecordId = destinationId;

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

    InventoryMovementRepository movementRepository;

    auto createMovement = [&](const QString& movementType,
                              int quantityChange,
                              int colorId,
                              const QString& notes) -> bool {
        InventoryMovement movement;

        movement.setWorkspaceId(record.workspaceId());

        movement.setInventoryRecordId(finalRecordId);

        movement.setPartId(record.partId());

        movement.setColorId(colorId);

        movement.setMovementType(movementType);

        movement.setQuantityChange(quantityChange);

        movement.setToStorageLocationId(record.storageLocationId());

        movement.setCondition(record.condition());

        movement.setOwnershipType(record.ownershipType());

        movement.setNotes(notes);

        return movementRepository.create(movement);
    };

    //
    // Quantity adjustment history.
    //
    if (record.quantity() != originalQuantity && !merged) {
        const int difference = record.quantity() - originalQuantity;

        const QString movementType = difference > 0 ? "QuantityIncrease" : "QuantityDecrease";

        if (!createMovement(movementType, difference, record.colorId(), QString())) {
            database.rollback();
            return false;
        }
    }

    //
    // Color change.
    //
    if (record.colorId() != originalColorId) {
        if (!createMovement("ColorChange",
                            0,
                            record.colorId(),
                            QString("Color ID %1 -> %2").arg(originalColorId).arg(record.colorId()))) {
            database.rollback();
            return false;
        }
    }

    //
    // Condition change.
    //
    if (record.condition() != originalCondition) {
        if (!createMovement("ConditionChange",
                            0,
                            record.colorId(),
                            QString("%1 -> %2").arg(originalCondition).arg(record.condition()))) {
            database.rollback();
            return false;
        }
    }

    //
    // Ownership change.
    //
    if (record.ownershipType() != originalOwnershipType) {
        if (!createMovement("OwnershipChange",
                            0,
                            record.colorId(),
                            QString("%1 -> %2")
                                .arg(originalOwnershipType)
                                .arg(record.ownershipType()))) {
            database.rollback();
            return false;
        }
    }

    //
    // If Storage somehow changed through Edit,
    // record it clearly, although we'll soon remove
    // Storage from Edit and require Move instead.
    //
    if (record.storageLocationId() != originalStorageLocationId) {
        InventoryMovement movement;

        movement.setWorkspaceId(record.workspaceId());

        movement.setInventoryRecordId(finalRecordId);

        movement.setPartId(record.partId());

        movement.setColorId(record.colorId());

        movement.setMovementType("Move");

        movement.setQuantityChange(record.quantity());

        movement.setFromStorageLocationId(originalStorageLocationId);

        movement.setToStorageLocationId(record.storageLocationId());

        movement.setCondition(record.condition());

        movement.setOwnershipType(record.ownershipType());

        if (!movementRepository.create(movement)) {
            database.rollback();
            return false;
        }
    }

    if (merged) {
        if (!createMovement("Merge",
                            record.quantity(),
                            record.colorId(),
                            "Inventory record merged with existing destination record.")) {
            database.rollback();
            return false;
        }
    }

    if (!database.commit()) {
        qCritical() << "Unable to commit inventory edit transaction:"
                    << database.lastError().text();

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

bool InventoryRecordRepository::moveInventory(int inventoryRecordId,
                                              int destinationStorageLocationId,
                                              int quantityToMove)
{
    if (inventoryRecordId <= 0 || destinationStorageLocationId <= 0 || quantityToMove <= 0) {
        return false;
    }

    QSqlDatabase database = DatabaseManager::instance().database();

    if (!database.transaction()) {
        qCritical() << "Unable to begin inventory move transaction:" << database.lastError().text();

        return false;
    }

    //
    // Load the source inventory record.
    //
    QSqlQuery sourceQuery(database);

    sourceQuery.prepare(R"(
        SELECT
            id,
            workspace_id,
            part_id,
            color_id,
            storage_location_id,
            condition,
            ownership_type,
            quantity
        FROM inventory_record
        WHERE id = :id
    )");

    sourceQuery.bindValue(":id", inventoryRecordId);

    if (!sourceQuery.exec() || !sourceQuery.next()) {
        qCritical() << "Unable to load source inventory record:" << sourceQuery.lastError().text();

        database.rollback();
        return false;
    }

    const int workspaceId = sourceQuery.value("workspace_id").toInt();

    const int partId = sourceQuery.value("part_id").toInt();

    const int colorId = sourceQuery.value("color_id").toInt();

    const int sourceStorageLocationId = sourceQuery.value("storage_location_id").toInt();

    const QString condition = sourceQuery.value("condition").toString();

    const QString ownershipType = sourceQuery.value("ownership_type").toString();

    const int sourceQuantity = sourceQuery.value("quantity").toInt();

    if (destinationStorageLocationId == sourceStorageLocationId) {
        qWarning() << "Source and destination storage locations are identical.";

        database.rollback();
        return false;
    }

    if (quantityToMove > sourceQuantity) {
        qWarning() << "Cannot move more inventory than is available.";

        database.rollback();
        return false;
    }

    //
    // Make sure the destination storage location
    // belongs to the same workspace.
    //
    QSqlQuery destinationLocationQuery(database);

    destinationLocationQuery.prepare(R"(
        SELECT id
        FROM storage_location
        WHERE id = :id
          AND workspace_id = :workspace_id
          AND is_active = 1
    )");

    destinationLocationQuery.bindValue(":id", destinationStorageLocationId);

    destinationLocationQuery.bindValue(":workspace_id", workspaceId);

    if (!destinationLocationQuery.exec() || !destinationLocationQuery.next()) {
        qCritical() << "Invalid destination storage location.";

        database.rollback();
        return false;
    }

    const QString now = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);

    //
    // Look for an existing destination record with
    // the exact same inventory attributes.
    //
    QSqlQuery destinationQuery(database);

    destinationQuery.prepare(R"(
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
          AND id <> :source_id
        LIMIT 1
    )");

    destinationQuery.bindValue(":workspace_id", workspaceId);

    destinationQuery.bindValue(":part_id", partId);

    destinationQuery.bindValue(":color_id", colorId);

    destinationQuery.bindValue(":storage_location_id", destinationStorageLocationId);

    destinationQuery.bindValue(":condition", condition);

    destinationQuery.bindValue(":ownership_type", ownershipType);

    destinationQuery.bindValue(":source_id", inventoryRecordId);

    if (!destinationQuery.exec()) {
        qCritical() << "Unable to query move destination:" << destinationQuery.lastError().text();

        database.rollback();
        return false;
    }

    int destinationRecordId = 0;

    if (destinationQuery.next()) {
        destinationRecordId = destinationQuery.value("id").toInt();

        const int destinationQuantity = destinationQuery.value("quantity").toInt();

        QSqlQuery updateDestinationQuery(database);

        updateDestinationQuery.prepare(R"(
            UPDATE inventory_record
            SET
                quantity = :quantity,
                modified_utc = :modified_utc
            WHERE id = :id
        )");

        updateDestinationQuery.bindValue(":quantity", destinationQuantity + quantityToMove);

        updateDestinationQuery.bindValue(":modified_utc", now);

        updateDestinationQuery.bindValue(":id", destinationRecordId);

        if (!updateDestinationQuery.exec()) {
            qCritical() << "Unable to update destination inventory:"
                        << updateDestinationQuery.lastError().text();

            database.rollback();
            return false;
        }
    } else {
        //
        // No destination record exists, so create one.
        //
        QSqlQuery insertDestinationQuery(database);

        insertDestinationQuery.prepare(R"(
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

        insertDestinationQuery.bindValue(":workspace_id", workspaceId);

        insertDestinationQuery.bindValue(":part_id", partId);

        insertDestinationQuery.bindValue(":color_id", colorId);

        insertDestinationQuery.bindValue(":storage_location_id", destinationStorageLocationId);

        insertDestinationQuery.bindValue(":condition", condition);

        insertDestinationQuery.bindValue(":ownership_type", ownershipType);

        insertDestinationQuery.bindValue(":quantity", quantityToMove);

        insertDestinationQuery.bindValue(":created_utc", now);

        insertDestinationQuery.bindValue(":modified_utc", now);

        if (!insertDestinationQuery.exec()) {
            qCritical() << "Unable to create destination inventory:"
                        << insertDestinationQuery.lastError().text();

            database.rollback();
            return false;
        }

        destinationRecordId = insertDestinationQuery.lastInsertId().toInt();
    }

    //
    // Reduce the source quantity.
    // A full move leaves the historical source record
    // at quantity zero rather than deleting it.
    //
    const int remainingQuantity = sourceQuantity - quantityToMove;

    QSqlQuery updateSourceQuery(database);

    updateSourceQuery.prepare(R"(
        UPDATE inventory_record
        SET
            quantity = :quantity,
            modified_utc = :modified_utc
        WHERE id = :id
    )");

    updateSourceQuery.bindValue(":quantity", remainingQuantity);

    updateSourceQuery.bindValue(":modified_utc", now);

    updateSourceQuery.bindValue(":id", inventoryRecordId);

    if (!updateSourceQuery.exec()) {
        qCritical() << "Unable to update source inventory:" << updateSourceQuery.lastError().text();

        database.rollback();
        return false;
    }

    //
    // Record the movement.
    //
    InventoryMovement movement;

    movement.setWorkspaceId(workspaceId);

    // Keep the movement attached to the source record.
    // The source record remains in the database even when
    // its quantity reaches zero.
    movement.setInventoryRecordId(inventoryRecordId);

    movement.setPartId(partId);

    movement.setColorId(colorId);

    movement.setMovementType("Move");

    // For a Move event, quantity_change represents
    // the quantity affected by the movement.
    movement.setQuantityChange(quantityToMove);

    movement.setFromStorageLocationId(sourceStorageLocationId);

    movement.setToStorageLocationId(destinationStorageLocationId);

    movement.setCondition(condition);

    movement.setOwnershipType(ownershipType);

    InventoryMovementRepository movementRepository;

    if (!movementRepository.create(movement)) {
        qCritical() << "Unable to record inventory movement.";

        database.rollback();
        return false;
    }

    if (!database.commit()) {
        qCritical() << "Unable to commit inventory move:" << database.lastError().text();

        database.rollback();
        return false;
    }

    return true;
}