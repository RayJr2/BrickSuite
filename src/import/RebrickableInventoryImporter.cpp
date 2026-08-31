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

#include "RebrickableInventoryImporter.h"

#include "../database/DatabaseManager.h"
#include "../models/InventoryRecord.h"
#include "../repositories/InventoryRecordRepository.h"

#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QSet>
#include <QTextStream>
#include <QtGlobal>

RebrickableInventoryImporter::RebrickableInventoryImporter(QSqlDatabase& database)
    : m_database(database)
{}

QStringList RebrickableInventoryImporter::parseCsvLine(const QString& line) const
{
    QStringList fields;
    QString currentField;

    bool insideQuotes = false;

    for (qsizetype i = 0; i < line.length(); ++i) {
        const QChar character = line.at(i);

        if (character == '"') {
            if (insideQuotes && i + 1 < line.length() && line.at(i + 1) == '"') {
                currentField.append('"');
                ++i;
            } else {
                insideQuotes = !insideQuotes;
            }
        } else if (character == ',' && !insideQuotes) {
            fields.append(currentField);
            currentField.clear();
        } else {
            currentField.append(character);
        }
    }

    fields.append(currentField);

    return fields;
}

bool RebrickableInventoryImporter::importOwnedParts(const QString& filePath,
                                                    const InventoryImportOptions& options,
                                                    InventoryImportResult& result)
{
    result = {};

    if (options.workspaceId <= 0 || options.storageLocationId <= 0) {
        qCritical() << "Invalid Rebrickable inventory import options.";

        return false;
    }

    QFile file(filePath);

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qCritical() << "Unable to open Rebrickable inventory file:" << filePath
                    << file.errorString();

        return false;
    }

    const QString sourceFileName = QFileInfo(filePath).fileName();

    QTextStream stream(&file);

    if (stream.atEnd()) {
        qCritical() << "Rebrickable inventory file is empty:" << filePath;

        return false;
    }

    const QStringList header = parseCsvLine(stream.readLine());

    const int partIndex = header.indexOf("Part");

    const int colorIndex = header.indexOf("Color");

    const int quantityIndex = header.indexOf("Quantity");

    if (partIndex < 0 || colorIndex < 0 || quantityIndex < 0) {
        qCritical() << "Unsupported Rebrickable inventory CSV format.";

        return false;
    }

    if (!m_database.transaction()) {
        qCritical() << "Unable to begin Rebrickable inventory import transaction:"
                    << m_database.lastError().text();

        return false;
    }

    InventoryRecordRepository inventoryRepository;

    while (!stream.atEnd()) {
        const QString line = stream.readLine();

        if (line.trimmed().isEmpty())
            continue;

        ++result.rowsProcessed;

        const QStringList fields = parseCsvLine(line);

        if (fields.size() != header.size()) {
            ++result.rowsFailed;
            continue;
        }

        const QString rebrickablePartId = fields.at(partIndex).trimmed();

        bool colorValid = false;
        bool quantityValid = false;

        const int rebrickableColorId = fields.at(colorIndex).trimmed().toInt(&colorValid);

        const int quantity = fields.at(quantityIndex).trimmed().toInt(&quantityValid);

        if (rebrickablePartId.isEmpty() || !colorValid || !quantityValid || quantity <= 0) {
            ++result.rowsFailed;
            continue;
        }

        // Resolve BrickSuite part ID.
        int partId = 0;

        {
            QSqlQuery query(m_database);

            query.prepare(R"(
                SELECT id
                FROM part
                WHERE rebrickable_part_id = :part_id
                LIMIT 1
            )");

            query.bindValue(":part_id", rebrickablePartId);

            if (!query.exec() || !query.next()) {
                ++result.rowsFailed;

                qWarning() << "Unable to resolve Rebrickable part:" << rebrickablePartId;

                continue;
            }

            partId = query.value(0).toInt();
        }

        // Resolve BrickSuite color ID.
        int colorId = 0;

        {
            QSqlQuery query(m_database);

            query.prepare(R"(
                SELECT id
                FROM color
                WHERE rebrickable_id = :color_id
                LIMIT 1
            )");

            query.bindValue(":color_id", rebrickableColorId);

            if (!query.exec() || !query.next()) {
                ++result.rowsFailed;

                qWarning() << "Unable to resolve Rebrickable color:" << rebrickableColorId;

                continue;
            }

            colorId = query.value(0).toInt();
        }

        InventoryRecord record;

        record.setWorkspaceId(options.workspaceId);

        record.setPartId(partId);

        record.setColorId(colorId);

        record.setStorageLocationId(options.storageLocationId);

        record.setCondition(options.condition);

        record.setOwnershipType(options.ownershipType);

        record.setQuantity(quantity);

        if (!inventoryRepository.addOrIncreaseQuantity(record,
                                                       "CSVImport",
                                                       "RebrickableCSV",
                                                       sourceFileName,
                                                       "Imported from Rebrickable owned-parts CSV.",
                                                       false)) {
            ++result.rowsFailed;
            continue;
        }

        ++result.rowsImported;

        result.totalQuantityImported += quantity;
    }

    if (result.rowsFailed > 0) {
        qCritical() << "Rebrickable inventory import encountered" << result.rowsFailed
                    << "failed rows.";

        m_database.rollback();

        return false;
    }

    if (!m_database.commit()) {
        qCritical() << "Unable to commit Rebrickable inventory import:"
                    << m_database.lastError().text();

        m_database.rollback();

        return false;
    }

    qInfo() << "Rebrickable inventory import completed."
            << "File:" << filePath
            << "WorkspaceId:" << options.workspaceId
            << "StorageLocationId:" << options.storageLocationId
            << "RowsProcessed:" << result.rowsProcessed
            << "RowsImported:" << result.rowsImported
            << "RowsFailed:" << result.rowsFailed
            << "TotalQuantityImported:" << result.totalQuantityImported;

    return true;
}

bool RebrickableInventoryImporter::previewOwnedParts(const QString& filePath,
                                                     const InventoryImportOptions& options,
                                                     InventoryImportPreview& preview)
{
    preview = {};

    preview.source = InventoryImportSource::RebrickableCsv;
    preview.sourceFilePath = filePath;

    preview.sourceFileName = QFileInfo(filePath).fileName();
    preview.operation = options.operation;

    if (options.workspaceId <= 0 || options.storageLocationId <= 0) {
        qCritical() << "Invalid Rebrickable inventory preview options.";

        return false;
    }

    QFile file(filePath);

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qCritical() << "Unable to open Rebrickable inventory file:" << filePath
                    << file.errorString();

        return false;
    }

    QTextStream stream(&file);

    if (stream.atEnd()) {
        qCritical() << "Rebrickable inventory file is empty:" << filePath;

        return false;
    }

    const QStringList header = parseCsvLine(stream.readLine());

    const int partIndex = header.indexOf("Part");

    const int colorIndex = header.indexOf("Color");

    const int quantityIndex = header.indexOf("Quantity");

    if (partIndex < 0 || colorIndex < 0 || quantityIndex < 0) {
        qCritical() << "Unsupported Rebrickable inventory CSV format.";

        return false;
    }

    while (!stream.atEnd()) {
        const QString line = stream.readLine();

        if (line.trimmed().isEmpty())
            continue;

        ++preview.rowsProcessed;

        InventoryImportPreviewRow previewRow;
        previewRow.presentInSource = true;

        const QStringList fields = parseCsvLine(line);

        if (fields.size() != header.size()) {
            ++preview.failedRows;

            previewRow.status = "Error";

            previewRow.errorMessage = "Invalid CSV column count.";

            preview.rows.append(previewRow);

            continue;
        }

        previewRow.sourcePartNumber = fields.at(partIndex).trimmed();
        previewRow.partNumber = previewRow.sourcePartNumber;

        bool colorValid = false;
        bool quantityValid = false;

        previewRow.sourceColorIdentifier = fields.at(colorIndex).trimmed();
        previewRow.rebrickableColorId =
            previewRow.sourceColorIdentifier.toInt(&colorValid);

        previewRow.sourceQuantity = fields.at(quantityIndex).trimmed().toInt(&quantityValid);

        if (previewRow.partNumber.isEmpty() || !colorValid || !quantityValid
            || previewRow.sourceQuantity <= 0) {
            ++preview.failedRows;

            previewRow.status = "Error";

            previewRow.errorMessage = "Invalid Part, Color, or Quantity.";

            preview.rows.append(previewRow);

            continue;
        }

        //
        // Resolve part.
        //
        {
            QSqlQuery query(m_database);

            query.prepare(R"(
                SELECT
                    id,
                    name
                FROM part
                WHERE rebrickable_part_id = :part_id
                LIMIT 1
            )");

            query.bindValue(":part_id", previewRow.partNumber);

            if (!query.exec() || !query.next()) {
                ++preview.failedRows;

                previewRow.status = "Error";

                previewRow.errorMessage = "Part not found in BrickSuite.";

                preview.rows.append(previewRow);

                continue;
            }

            previewRow.partId = query.value("id").toInt();

            previewRow.partName = query.value("name").toString();
        }

        //
        // Resolve color.
        //
        {
            QSqlQuery query(m_database);

            query.prepare(R"(
                SELECT
                    id,
                    name
                FROM color
                WHERE rebrickable_id = :color_id
                LIMIT 1
            )");

            query.bindValue(":color_id", previewRow.rebrickableColorId);

            if (!query.exec() || !query.next()) {
                ++preview.failedRows;

                previewRow.status = "Error";

                previewRow.errorMessage = "Color not found in BrickSuite.";

                preview.rows.append(previewRow);

                continue;
            }

            previewRow.colorId = query.value("id").toInt();

            previewRow.colorName = query.value("name").toString();
        }

        //
        // Find current BrickSuite quantity for this
        // exact target inventory combination.
        //
        {
            QSqlQuery query(m_database);

            query.prepare(R"(
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

            query.bindValue(":workspace_id", options.workspaceId);

            query.bindValue(":part_id", previewRow.partId);

            query.bindValue(":color_id", previewRow.colorId);

            query.bindValue(":storage_location_id", options.storageLocationId);

            query.bindValue(":condition", options.condition.trimmed());

            query.bindValue(":ownership_type", options.ownershipType.trimmed());

            if (!query.exec()) {
                ++preview.failedRows;

                previewRow.status = "Error";

                previewRow.errorMessage = "Unable to query current inventory.";

                preview.rows.append(previewRow);

                continue;
            }

            if (query.next()) {
                previewRow.inventoryRecordId = query.value("id").toInt();
                previewRow.currentQuantity = query.value("quantity").toInt();
                previewRow.presentInBrickSuite = previewRow.currentQuantity > 0;
            }
        }

        switch (options.operation) {
        case InventoryCsvOperation::Append:
            previewRow.resultingQuantity =
                previewRow.currentQuantity + previewRow.sourceQuantity;
            previewRow.difference = previewRow.sourceQuantity;
            previewRow.status =
                previewRow.currentQuantity == 0
                    ? QStringLiteral("New Inventory")
                    : QStringLiteral("Append");
            break;

        case InventoryCsvOperation::Replace:
            previewRow.resultingQuantity = previewRow.sourceQuantity;
            previewRow.difference =
                previewRow.resultingQuantity - previewRow.currentQuantity;

            if (previewRow.currentQuantity == 0) {
                previewRow.status = QStringLiteral("New Inventory");
            } else if (previewRow.difference == 0) {
                previewRow.status = QStringLiteral("No Change");
            } else if (previewRow.difference > 0) {
                previewRow.status = QStringLiteral("Replace Increase");
            } else {
                previewRow.status = QStringLiteral("Replace Decrease");
            }
            break;

        case InventoryCsvOperation::Subtract:
            if (previewRow.currentQuantity < previewRow.sourceQuantity) {
                previewRow.resultingQuantity = previewRow.currentQuantity;
                previewRow.difference = 0;
                previewRow.status = QStringLiteral("Needs Review");
                previewRow.errorMessage =
                    QStringLiteral("Cannot subtract %1; BrickSuite has only %2 in the selected storage.")
                        .arg(previewRow.sourceQuantity)
                        .arg(previewRow.currentQuantity);
                ++preview.failedRows;
            } else {
                previewRow.resultingQuantity =
                    previewRow.currentQuantity - previewRow.sourceQuantity;
                previewRow.difference = -previewRow.sourceQuantity;
                previewRow.status =
                    previewRow.sourceQuantity == 0
                        ? QStringLiteral("No Change")
                        : QStringLiteral("Subtract");
            }
            break;

        case InventoryCsvOperation::CompareOnly:
            previewRow.resultingQuantity = previewRow.currentQuantity;
            previewRow.difference =
                previewRow.currentQuantity - previewRow.sourceQuantity;

            if (previewRow.difference == 0) {
                previewRow.status = QStringLiteral("Match");
            } else if (previewRow.difference > 0) {
                previewRow.status = QStringLiteral("Append to Rebrickable");
            } else {
                previewRow.status = QStringLiteral("Subtract from Rebrickable");
            }
            break;
        }

        if (previewRow.status != QStringLiteral("Needs Review")) {
            ++preview.validRows;
        }

        preview.totalSourceQuantity += previewRow.sourceQuantity;

        preview.rows.append(previewRow);
    }

    //
    // Compare Only and Replace reconcile the complete union of:
    //   - rows represented by the Rebrickable CSV
    //   - rows currently stored in the selected BrickSuite location
    //
    // Compare Only needs the union to detect BrickSuite-only inventory.
    // Replace needs the same union so inventory absent from the CSV can be
    // reduced to zero, making the selected storage match the CSV exactly.
    //
    if (options.operation == InventoryCsvOperation::CompareOnly
        || options.operation == InventoryCsvOperation::Replace) {
        QSet<QString> representedKeys;

        for (const InventoryImportPreviewRow& row : preview.rows) {
            if (row.partId <= 0 || row.colorId <= 0)
                continue;

            representedKeys.insert(
                QStringLiteral("%1:%2").arg(row.partId).arg(row.colorId));
        }

        QSqlQuery inventoryQuery(m_database);

        inventoryQuery.prepare(R"(
            SELECT
                ir.id AS inventory_record_id,
                ir.part_id,
                ir.color_id,
                ir.quantity,
                p.rebrickable_part_id,
                p.name AS part_name,
                c.rebrickable_id AS rebrickable_color_id,
                c.name AS color_name
            FROM inventory_record ir
            INNER JOIN part p
                ON p.id = ir.part_id
            INNER JOIN color c
                ON c.id = ir.color_id
            WHERE ir.workspace_id = :workspace_id
              AND ir.storage_location_id = :storage_location_id
              AND ir.condition = :condition
              AND ir.ownership_type = :ownership_type
              AND ir.quantity > 0
            ORDER BY p.rebrickable_part_id, c.name
        )");

        inventoryQuery.bindValue(":workspace_id", options.workspaceId);
        inventoryQuery.bindValue(":storage_location_id", options.storageLocationId);
        inventoryQuery.bindValue(":condition", options.condition.trimmed());
        inventoryQuery.bindValue(":ownership_type", options.ownershipType.trimmed());

        if (!inventoryQuery.exec()) {
            qCritical() << "Unable to query selected BrickSuite storage for comparison:"
                        << inventoryQuery.lastError().text();
            return false;
        }

        while (inventoryQuery.next()) {
            const int partId = inventoryQuery.value("part_id").toInt();
            const int colorId = inventoryQuery.value("color_id").toInt();

            const QString key =
                QStringLiteral("%1:%2").arg(partId).arg(colorId);

            if (representedKeys.contains(key))
                continue;

            InventoryImportPreviewRow row;

            row.partId = partId;
            row.colorId = colorId;
            row.inventoryRecordId =
                inventoryQuery.value("inventory_record_id").toInt();

            row.partNumber =
                inventoryQuery.value("rebrickable_part_id").toString();
            row.sourcePartNumber = row.partNumber;
            row.partName =
                inventoryQuery.value("part_name").toString();

            row.rebrickableColorId =
                inventoryQuery.value("rebrickable_color_id").toInt();
            row.sourceColorIdentifier =
                QString::number(row.rebrickableColorId);
            row.colorName =
                inventoryQuery.value("color_name").toString();
            row.sourceColorName = row.colorName;

            row.presentInSource = false;
            row.presentInBrickSuite = true;

            row.sourceQuantity = 0;
            row.currentQuantity =
                inventoryQuery.value("quantity").toInt();

            if (options.operation == InventoryCsvOperation::CompareOnly) {
                row.resultingQuantity = row.currentQuantity;
                row.difference = row.currentQuantity;
                row.status = QStringLiteral("Append to Rebrickable");
            } else {
                row.resultingQuantity = 0;
                row.difference = -row.currentQuantity;
                row.status = QStringLiteral("Replace Remove");
            }

            preview.rows.append(row);
            ++preview.validRows;

            representedKeys.insert(key);
        }
    }

    qInfo() << "Rebrickable inventory preview completed."
            << "File:" << filePath
            << "WorkspaceId:" << options.workspaceId
            << "StorageLocationId:" << options.storageLocationId
            << "RowsProcessed:" << preview.rowsProcessed
            << "FailedRows:" << preview.failedRows
            << "TotalSourceQuantity:" << preview.totalSourceQuantity;

    return true;
}

bool RebrickableInventoryImporter::importPreview(
    const InventoryImportPreview& preview,
    const InventoryImportOptions& options,
    InventoryImportResult& result)
{
    result = {};

    if (options.workspaceId <= 0 || options.storageLocationId <= 0) {
        qCritical() << "Invalid Rebrickable inventory import options.";
        return false;
    }

    if (preview.rows.isEmpty()) {
        qWarning() << "Rebrickable inventory preview contains no rows.";
        return false;
    }

    if (options.operation == InventoryCsvOperation::CompareOnly) {
        result.rowsProcessed = preview.rowsProcessed;
        return true;
    }

    if (preview.failedRows > 0) {
        qWarning() << "Rebrickable inventory preview contains rows that require review.";
        return false;
    }

    if (!m_database.transaction()) {
        qCritical() << "Unable to begin Rebrickable inventory import transaction:"
                    << m_database.lastError().text();
        return false;
    }

    InventoryRecordRepository inventoryRepository;

    for (const InventoryImportPreviewRow& previewRow : preview.rows) {
        ++result.rowsProcessed;

        if (previewRow.status == QStringLiteral("Error")
            || previewRow.status == QStringLiteral("Needs Review")) {
            ++result.rowsFailed;
            continue;
        }

        if (previewRow.status == QStringLiteral("No Change")
            || previewRow.status == QStringLiteral("Match")) {
            continue;
        }

        bool rowApplied = false;
        int quantityChanged = 0;

        if (options.operation == InventoryCsvOperation::Append) {
            InventoryRecord record;
            record.setWorkspaceId(options.workspaceId);
            record.setPartId(previewRow.partId);
            record.setColorId(previewRow.colorId);
            record.setStorageLocationId(options.storageLocationId);
            record.setCondition(options.condition);
            record.setOwnershipType(options.ownershipType);
            record.setQuantity(previewRow.sourceQuantity);

            const QString notes =
                QStringLiteral("Rebrickable CSV Append. Quantity appended: %1.")
                    .arg(previewRow.sourceQuantity);

            rowApplied =
                inventoryRepository.addOrIncreaseQuantity(
                    record,
                    QStringLiteral("CSVImport"),
                    QStringLiteral("RebrickableCSV"),
                    preview.sourceFileName,
                    notes,
                    false);

            quantityChanged = previewRow.sourceQuantity;
        } else if (options.operation == InventoryCsvOperation::Replace) {
            if (previewRow.currentQuantity == 0) {
                InventoryRecord record;
                record.setWorkspaceId(options.workspaceId);
                record.setPartId(previewRow.partId);
                record.setColorId(previewRow.colorId);
                record.setStorageLocationId(options.storageLocationId);
                record.setCondition(options.condition);
                record.setOwnershipType(options.ownershipType);
                record.setQuantity(previewRow.resultingQuantity);

                rowApplied =
                    inventoryRepository.addOrIncreaseQuantity(
                        record,
                        QStringLiteral("CSVImport"),
                        QStringLiteral("RebrickableCSV"),
                        preview.sourceFileName,
                        QStringLiteral("Rebrickable CSV Replace created inventory."),
                        false);
            } else {
                const QString movementType =
                    previewRow.difference > 0
                        ? QStringLiteral("QuantityIncrease")
                        : QStringLiteral("QuantityDecrease");

                const QString notes =
                    QStringLiteral("Rebrickable CSV Replace. Quantity %1 -> %2.")
                        .arg(previewRow.currentQuantity)
                        .arg(previewRow.resultingQuantity);

                rowApplied =
                    inventoryRepository.setQuantityWithMovement(
                        previewRow.inventoryRecordId,
                        previewRow.resultingQuantity,
                        movementType,
                        QStringLiteral("RebrickableCSV"),
                        preview.sourceFileName,
                        notes,
                        false);
            }

            quantityChanged = qAbs(previewRow.difference);
        } else if (options.operation == InventoryCsvOperation::Subtract) {
            if (previewRow.inventoryRecordId <= 0
                || previewRow.currentQuantity < previewRow.sourceQuantity) {
                rowApplied = false;
            } else {
                const QString notes =
                    QStringLiteral("Rebrickable CSV Subtract. Quantity %1 -> %2.")
                        .arg(previewRow.currentQuantity)
                        .arg(previewRow.resultingQuantity);

                rowApplied =
                    inventoryRepository.setQuantityWithMovement(
                        previewRow.inventoryRecordId,
                        previewRow.resultingQuantity,
                        QStringLiteral("QuantityDecrease"),
                        QStringLiteral("RebrickableCSV"),
                        preview.sourceFileName,
                        notes,
                        false);
            }

            quantityChanged = previewRow.sourceQuantity;
        }

        if (!rowApplied) {
            ++result.rowsFailed;
            qCritical() << "Unable to apply Rebrickable inventory row."
                        << "Part:" << previewRow.partNumber
                        << "Operation:" << inventoryCsvOperationName(options.operation);
            continue;
        }

        ++result.rowsImported;
        result.totalQuantityImported += quantityChanged;
    }

    if (result.rowsFailed > 0) {
        qCritical() << "Rebrickable inventory operation encountered"
                    << result.rowsFailed << "failed rows.";
        m_database.rollback();
        return false;
    }

    if (!m_database.commit()) {
        qCritical() << "Unable to commit Rebrickable inventory operation:"
                    << m_database.lastError().text();
        m_database.rollback();
        return false;
    }

    return true;
}

