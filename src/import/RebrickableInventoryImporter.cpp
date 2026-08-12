#include "RebrickableInventoryImporter.h"

#include "../database/DatabaseManager.h"
#include "../models/InventoryRecord.h"
#include "../repositories/InventoryRecordRepository.h"

#include <QDebug>
#include <QFile>
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

        if (!inventoryRepository.addOrIncreaseQuantity(record)) {
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