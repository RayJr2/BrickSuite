#include "MinifigCatalogPartRepository.h"

#include "../database/DatabaseManager.h"

#include <QDateTime>
#include <QSqlError>
#include <QSqlQuery>

QList<MinifigCatalogPart> MinifigCatalogPartRepository::listForMinifig(
    int minifigCatalogId) const
{
    QList<MinifigCatalogPart> parts;
    QSqlQuery query(DatabaseManager::instance().database());
    query.prepare(R"(
        SELECT mcp.id, mcp.minifig_catalog_id, mcp.part_id, mcp.color_id,
               mcp.quantity_required, mcp.is_spare, p.part_number, p.name,
               c.name, c.rebrickable_id, mcp.provider, mcp.source,
               mcp.created_utc, mcp.modified_utc
        FROM minifig_catalog_part mcp
        JOIN part p ON p.id = mcp.part_id
        JOIN color c ON c.id = mcp.color_id
        WHERE mcp.minifig_catalog_id = :minifig_catalog_id
        ORDER BY mcp.is_spare, p.part_number COLLATE NOCASE,
                 c.name COLLATE NOCASE, mcp.id
    )");
    query.bindValue(":minifig_catalog_id", minifigCatalogId);
    if (!query.exec())
        return parts;

    while (query.next()) {
        MinifigCatalogPart part;
        part.id = query.value(0).toInt();
        part.minifigCatalogId = query.value(1).toInt();
        part.partId = query.value(2).toInt();
        part.colorId = query.value(3).toInt();
        part.quantityRequired = query.value(4).toInt();
        part.isSpare = query.value(5).toBool();
        part.partNumber = query.value(6).toString();
        part.partName = query.value(7).toString();
        part.colorName = query.value(8).toString();
        part.rebrickableColorId = query.value(9).toInt();
        part.provider = query.value(10).toString();
        part.source = query.value(11).toString();
        part.createdUtc = QDateTime::fromString(query.value(12).toString(), Qt::ISODateWithMs);
        part.modifiedUtc = QDateTime::fromString(query.value(13).toString(), Qt::ISODateWithMs);
        parts.append(part);
    }
    return parts;
}

bool MinifigCatalogPartRepository::replaceForMinifig(
    int minifigCatalogId,
    const QList<MinifigCatalogPart>& parts,
    QString& errorMessage) const
{
    QSqlDatabase database = DatabaseManager::instance().database();
    QSqlQuery remove(database);
    remove.prepare("DELETE FROM minifig_catalog_part WHERE minifig_catalog_id = :id");
    remove.bindValue(":id", minifigCatalogId);
    if (!remove.exec()) {
        errorMessage = remove.lastError().text();
        return false;
    }

    const QString now = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    QSqlQuery insert(database);
    if (!insert.prepare(R"(
        INSERT INTO minifig_catalog_part
            (minifig_catalog_id, part_id, color_id, quantity_required, is_spare,
             provider, source, created_utc, modified_utc)
        VALUES (:minifig_catalog_id, :part_id, :color_id, :quantity_required,
                :is_spare, :provider, :source, :created_utc, :modified_utc)
    )")) {
        errorMessage = insert.lastError().text();
        return false;
    }

    for (const MinifigCatalogPart& part : parts) {
        insert.bindValue(":minifig_catalog_id", minifigCatalogId);
        insert.bindValue(":part_id", part.partId);
        insert.bindValue(":color_id", part.colorId);
        insert.bindValue(":quantity_required", part.quantityRequired);
        insert.bindValue(":is_spare", part.isSpare ? 1 : 0);
        insert.bindValue(":provider", part.provider);
        insert.bindValue(":source", part.source);
        insert.bindValue(":created_utc", now);
        insert.bindValue(":modified_utc", now);
        if (!insert.exec()) {
            errorMessage = insert.lastError().text();
            return false;
        }
    }
    return true;
}
