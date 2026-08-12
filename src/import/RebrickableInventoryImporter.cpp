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
#include <QTextStream>

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
                                                    const ImportOptions& options,
                                                    ImportResult& result)
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

    qInfo() << "Rebrickable inventory imported."
            << "Rows:" << result.rowsImported << "Quantity:" << result.totalQuantityImported;

    return true;
}

bool RebrickableInventoryImporter::previewOwnedParts(const QString& filePath,
                                                     const ImportOptions& options,
                                                     RebrickableInventoryImportPreview& preview)
{
    preview = {};

    preview.sourceFilePath = filePath;

    preview.sourceFileName = QFileInfo(filePath).fileName();

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

        RebrickableInventoryImportPreviewRow previewRow;

        const QStringList fields = parseCsvLine(line);

        if (fields.size() != header.size()) {
            ++preview.failedRows;

            previewRow.status = "Error";

            previewRow.errorMessage = "Invalid CSV column count.";

            preview.rows.append(previewRow);

            continue;
        }

        previewRow.partNumber = fields.at(partIndex).trimmed();

        bool colorValid = false;
        bool quantityValid = false;

        previewRow.rebrickableColorId = fields.at(colorIndex).trimmed().toInt(&colorValid);

        previewRow.csvQuantity = fields.at(quantityIndex).trimmed().toInt(&quantityValid);

        if (previewRow.partNumber.isEmpty() || !colorValid || !quantityValid
            || previewRow.csvQuantity <= 0) {
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
                SELECT quantity
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
                previewRow.currentQuantity = query.value("quantity").toInt();
            }
        }

        if (previewRow.currentQuantity == 0) {
            previewRow.resultingQuantity = previewRow.csvQuantity;

            previewRow.status = "New Inventory";
        } else if (previewRow.csvQuantity == previewRow.currentQuantity) {
            previewRow.resultingQuantity = previewRow.currentQuantity;

            previewRow.status = "No Change";
        } else if (previewRow.csvQuantity > previewRow.currentQuantity) {
            previewRow.resultingQuantity = previewRow.csvQuantity;

            previewRow.status = "Increase Quantity";
        } else {
            previewRow.resultingQuantity = previewRow.currentQuantity;

            previewRow.status = "Review - CSV Quantity Lower";

            previewRow.errorMessage = QString("CSV quantity is %1 lower than BrickSuite quantity.")
                                          .arg(previewRow.currentQuantity - previewRow.csvQuantity);
        }

        ++preview.validRows;

        preview.totalCsvQuantity += previewRow.csvQuantity;

        preview.rows.append(previewRow);
    }

    return true;
}

bool RebrickableInventoryImporter::importPreview(const RebrickableInventoryImportPreview& preview,
                                                 const ImportOptions& options,
                                                 ImportResult& result)
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

    if (!m_database.transaction()) {
        qCritical() << "Unable to begin Rebrickable inventory import transaction:"
                    << m_database.lastError().text();

        return false;
    }

    InventoryRecordRepository inventoryRepository;

    for (const RebrickableInventoryImportPreviewRow& previewRow : preview.rows) {
        ++result.rowsProcessed;

        //
        // Invalid preview rows should never be imported.
        //
        if (previewRow.status == "Error") {
            ++result.rowsFailed;

            qWarning() << "Skipping invalid Rebrickable inventory row:" << previewRow.partNumber
                       << previewRow.errorMessage;

            continue;
        }

        //
        // Snapshot already matches BrickSuite.
        //
        if (previewRow.status == "No Change") {
            continue;
        }

        //
        // CSV quantity is lower than BrickSuite.
        // Version 1 does not automatically subtract.
        //
        if (previewRow.status == "Review - CSV Quantity Lower") {
            continue;
        }

        int quantityToAdd = 0;

        if (previewRow.status == "New Inventory") {
            quantityToAdd = previewRow.csvQuantity;
        } else if (previewRow.status == "Increase Quantity") {
            quantityToAdd = previewRow.csvQuantity - previewRow.currentQuantity;
        } else {
            ++result.rowsFailed;

            qWarning() << "Unsupported Rebrickable preview status:" << previewRow.status;

            continue;
        }

        if (quantityToAdd <= 0) {
            continue;
        }

        InventoryRecord record;

        record.setWorkspaceId(options.workspaceId);

        record.setPartId(previewRow.partId);

        record.setColorId(previewRow.colorId);

        record.setStorageLocationId(options.storageLocationId);

        record.setCondition(options.condition);

        record.setOwnershipType(options.ownershipType);

        record.setQuantity(quantityToAdd);

        const QString notes = QString("Rebrickable synchronization. "
                                      "CSV quantity: %1, "
                                      "BrickSuite quantity before sync: %2, "
                                      "quantity added: %3.")
                                  .arg(previewRow.csvQuantity)
                                  .arg(previewRow.currentQuantity)
                                  .arg(quantityToAdd);

        if (!inventoryRepository.addOrIncreaseQuantity(record,
                                                       "CSVImport",
                                                       "RebrickableCSV",
                                                       preview.sourceFileName,
                                                       notes,
                                                       false)) {
            ++result.rowsFailed;

            qCritical() << "Unable to import Rebrickable inventory row:" << previewRow.partNumber;

            continue;
        }

        ++result.rowsImported;

        result.totalQuantityImported += quantityToAdd;
    }

    //
    // Keep the import atomic.
    //
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

    qInfo() << "Rebrickable inventory synchronization completed."
            << "Rows changed:" << result.rowsImported
            << "Quantity added:" << result.totalQuantityImported;

    return true;
}