/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 *
 * This file is part of BrickSuite.
 *
 * BrickSuite is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, version 3 of the License.
 *
 * BrickSuite is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with BrickSuite. If not, see <https://www.gnu.org/licenses/>.
 */

#include "InventoryMovementRepository.h"

#include "../database/DatabaseManager.h"

#include <QDateTime>
#include <QDebug>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

bool InventoryMovementRepository::create(InventoryMovement& movement)
{
    if (movement.workspaceId() <= 0 || movement.partId() <= 0 || movement.colorId() <= 0
        || movement.movementType().trimmed().isEmpty()) {
        qWarning() << "Inventory movement create rejected due to invalid arguments."
                   << "WorkspaceId:" << movement.workspaceId()
                   << "PartId:" << movement.partId()
                   << "ColorId:" << movement.colorId()
                   << "MovementType:" << movement.movementType();
        return false;
    }

    QSqlDatabase database = DatabaseManager::instance().database();

    const QDateTime now = QDateTime::currentDateTimeUtc();

    QSqlQuery query(database);

    query.prepare(R"(
        INSERT INTO inventory_movement
        (
            workspace_id,
            inventory_record_id,
            part_id,
            color_id,
            movement_type,
            quantity_change,
            from_storage_location_id,
            to_storage_location_id,
            condition,
            ownership_type,
            reference_type,
            reference_id,
            notes,
            created_utc
        )
        VALUES
        (
            :workspace_id,
            :inventory_record_id,
            :part_id,
            :color_id,
            :movement_type,
            :quantity_change,
            :from_storage_location_id,
            :to_storage_location_id,
            :condition,
            :ownership_type,
            :reference_type,
            :reference_id,
            :notes,
            :created_utc
        )
    )");

    query.bindValue(":workspace_id", movement.workspaceId());

    if (movement.inventoryRecordId() > 0)
        query.bindValue(":inventory_record_id", movement.inventoryRecordId());
    else
        query.bindValue(":inventory_record_id", QVariant());

    query.bindValue(":part_id", movement.partId());

    query.bindValue(":color_id", movement.colorId());

    query.bindValue(":movement_type", movement.movementType());

    query.bindValue(":quantity_change", movement.quantityChange());

    if (movement.fromStorageLocationId() > 0)
        query.bindValue(":from_storage_location_id", movement.fromStorageLocationId());
    else
        query.bindValue(":from_storage_location_id", QVariant());

    if (movement.toStorageLocationId() > 0)
        query.bindValue(":to_storage_location_id", movement.toStorageLocationId());
    else
        query.bindValue(":to_storage_location_id", QVariant());

    query.bindValue(":condition", movement.condition());

    query.bindValue(":ownership_type", movement.ownershipType());

    query.bindValue(":reference_type", movement.referenceType());

    query.bindValue(":reference_id", movement.referenceId());

    query.bindValue(":notes", movement.notes());

    query.bindValue(":created_utc", now.toString(Qt::ISODateWithMs));

    if (!query.exec()) {
        qCritical() << "Unable to create inventory movement:" << query.lastError().text();

        return false;
    }

    movement.setId(query.lastInsertId().toInt());

    movement.setCreatedUtc(now);

    return true;
}

QList<InventoryMovement> InventoryMovementRepository::getByInventoryRecord(int inventoryRecordId) const
{
    QList<InventoryMovement> movements;

    QSqlDatabase database = DatabaseManager::instance().database();

    QSqlQuery query(database);

    query.prepare(R"(
        SELECT
            id,
            workspace_id,
            inventory_record_id,
            part_id,
            color_id,
            movement_type,
            quantity_change,
            from_storage_location_id,
            to_storage_location_id,
            condition,
            ownership_type,
            reference_type,
            reference_id,
            notes,
            created_utc
        FROM inventory_movement
        WHERE inventory_record_id = :inventory_record_id
        ORDER BY created_utc DESC, id DESC
    )");

    query.bindValue(":inventory_record_id", inventoryRecordId);

    if (!query.exec()) {
        qCritical() << "Unable to retrieve movements for inventory record."
                    << "InventoryRecordId:" << inventoryRecordId
                    << "Error:" << query.lastError().text();
        return movements;
    }

    while (query.next())
        movements.append(movementFromQuery(query));

    return movements;
}

QList<InventoryMovement> InventoryMovementRepository::getByPart(int workspaceId, int partId) const
{
    QList<InventoryMovement> movements;

    QSqlDatabase database = DatabaseManager::instance().database();

    QSqlQuery query(database);

    query.prepare(R"(
        SELECT
            id,
            workspace_id,
            inventory_record_id,
            part_id,
            color_id,
            movement_type,
            quantity_change,
            from_storage_location_id,
            to_storage_location_id,
            condition,
            ownership_type,
            reference_type,
            reference_id,
            notes,
            created_utc
        FROM inventory_movement
        WHERE workspace_id = :workspace_id
          AND part_id = :part_id
        ORDER BY created_utc DESC, id DESC
    )");

    query.bindValue(":workspace_id", workspaceId);

    query.bindValue(":part_id", partId);

    if (!query.exec()) {
        qCritical() << "Unable to retrieve movements for part."
                    << "WorkspaceId:" << workspaceId
                    << "PartId:" << partId
                    << "Error:" << query.lastError().text();
        return movements;
    }

    while (query.next())
        movements.append(movementFromQuery(query));

    return movements;
}

InventoryMovement InventoryMovementRepository::movementFromQuery(const QSqlQuery& query) const
{
    InventoryMovement movement;

    movement.setId(query.value("id").toInt());

    movement.setWorkspaceId(query.value("workspace_id").toInt());

    if (!query.value("inventory_record_id").isNull())
        movement.setInventoryRecordId(query.value("inventory_record_id").toInt());

    movement.setPartId(query.value("part_id").toInt());

    movement.setColorId(query.value("color_id").toInt());

    movement.setMovementType(query.value("movement_type").toString());

    movement.setQuantityChange(query.value("quantity_change").toInt());

    if (!query.value("from_storage_location_id").isNull())
        movement.setFromStorageLocationId(query.value("from_storage_location_id").toInt());

    if (!query.value("to_storage_location_id").isNull())
        movement.setToStorageLocationId(query.value("to_storage_location_id").toInt());

    movement.setCondition(query.value("condition").toString());

    movement.setOwnershipType(query.value("ownership_type").toString());

    movement.setReferenceType(query.value("reference_type").toString());

    movement.setReferenceId(query.value("reference_id").toString());

    movement.setNotes(query.value("notes").toString());

    movement.setCreatedUtc(
        QDateTime::fromString(query.value("created_utc").toString(), Qt::ISODateWithMs));

    return movement;
}

QList<InventoryHistoryResult> InventoryMovementRepository::getHistoryForPartColor(int workspaceId,
                                                                                  int partId,
                                                                                  int colorId) const
{
    QList<InventoryHistoryResult> results;

    QSqlDatabase database = DatabaseManager::instance().database();

    QSqlQuery query(database);

    query.prepare(R"(
        SELECT
            im.id,
            im.movement_type,
            im.quantity_change,

            im.from_storage_location_id,
            from_sl.name AS from_storage_name,

            im.to_storage_location_id,
            to_sl.name AS to_storage_name,

            im.condition,
            im.ownership_type,
            im.reference_type,
            im.reference_id,
            im.notes,
            im.created_utc

        FROM inventory_movement im

        LEFT JOIN storage_location from_sl
            ON from_sl.id = im.from_storage_location_id

        LEFT JOIN storage_location to_sl
            ON to_sl.id = im.to_storage_location_id

        WHERE im.workspace_id = :workspace_id
          AND im.part_id = :part_id
          AND im.color_id = :color_id

        ORDER BY
            im.created_utc DESC,
            im.id DESC
    )");

    query.bindValue(":workspace_id", workspaceId);

    query.bindValue(":part_id", partId);

    query.bindValue(":color_id", colorId);

    if (!query.exec()) {
        qCritical() << "Unable to retrieve inventory history:" << query.lastError().text();

        return results;
    }

    while (query.next()) {
        InventoryHistoryResult result;

        result.movementId = query.value("id").toInt();

        result.movementType = query.value("movement_type").toString();

        result.quantityChange = query.value("quantity_change").toInt();

        if (!query.value("from_storage_location_id").isNull()) {
            result.fromStorageLocationId = query.value("from_storage_location_id").toInt();
        }

        if (!query.value("to_storage_location_id").isNull()) {
            result.toStorageLocationId = query.value("to_storage_location_id").toInt();
        }

        result.fromStoragePath = query.value("from_storage_name").toString();

        result.toStoragePath = query.value("to_storage_name").toString();

        result.condition = query.value("condition").toString();

        result.ownershipType = query.value("ownership_type").toString();

        result.referenceType = query.value("reference_type").toString();

        result.referenceId = query.value("reference_id").toString();

        result.notes = query.value("notes").toString();

        result.createdUtc = QDateTime::fromString(query.value("created_utc").toString(),
                                                  Qt::ISODateWithMs);

        results.append(result);
    }

    return results;
}