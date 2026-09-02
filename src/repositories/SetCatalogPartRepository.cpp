#include "SetCatalogPartRepository.h"

#include "../database/DatabaseManager.h"

#include <QDateTime>
#include <QSqlError>
#include <QSqlQuery>

QList<SetCatalogPart> SetCatalogPartRepository::listForSet(int setCatalogId) const
{
    QList<SetCatalogPart> parts;
    QSqlQuery query(DatabaseManager::instance().database());
    query.prepare(R"(
        SELECT scp.id, scp.set_catalog_id, scp.part_id, scp.color_id,
               scp.quantity_required, scp.is_spare, p.part_number, p.name,
               c.name, c.rebrickable_id, scp.provider, scp.source,
               scp.created_utc, scp.modified_utc
        FROM set_catalog_part scp
        JOIN part p ON p.id = scp.part_id
        JOIN color c ON c.id = scp.color_id
        WHERE scp.set_catalog_id = :set_catalog_id
        ORDER BY scp.is_spare, p.part_number COLLATE NOCASE,
                 c.name COLLATE NOCASE, scp.id
    )");
    query.bindValue(":set_catalog_id", setCatalogId);
    if (!query.exec())
        return parts;
    while (query.next()) {
        SetCatalogPart part;
        part.id = query.value(0).toInt();
        part.setCatalogId = query.value(1).toInt();
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

SetCatalogPartRepository::Counts SetCatalogPartRepository::countsForSet(int setCatalogId) const
{
    Counts counts;
    QSqlQuery query(DatabaseManager::instance().database());
    query.prepare("SELECT COUNT(*), "
                  "COALESCE(SUM(CASE WHEN is_spare=0 THEN quantity_required ELSE 0 END),0), "
                  "COALESCE(SUM(CASE WHEN is_spare=1 THEN quantity_required ELSE 0 END),0) "
                  "FROM set_catalog_part WHERE set_catalog_id=:id");
    query.bindValue(":id", setCatalogId);
    if (query.exec() && query.next()) {
        counts.rows = query.value(0).toInt();
        counts.requiredPieces = query.value(1).toInt();
        counts.sparePieces = query.value(2).toInt();
    }
    return counts;
}

bool SetCatalogPartRepository::replaceForSet(int setCatalogId,
                                             const QList<SetCatalogPart>& parts,
                                             QString& errorMessage) const
{
    QSqlDatabase database = DatabaseManager::instance().database();
    QSqlQuery remove(database);
    remove.prepare("DELETE FROM set_catalog_part WHERE set_catalog_id=:id");
    remove.bindValue(":id", setCatalogId);
    if (!remove.exec()) { errorMessage = remove.lastError().text(); return false; }

    const QString now = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    QSqlQuery insert(database);
    if (!insert.prepare(R"(
        INSERT INTO set_catalog_part
          (set_catalog_id,part_id,color_id,quantity_required,is_spare,
           provider,source,created_utc,modified_utc)
        VALUES (:set_catalog_id,:part_id,:color_id,:quantity_required,:is_spare,
                :provider,:source,:created_utc,:modified_utc)
    )")) { errorMessage = insert.lastError().text(); return false; }
    for (const SetCatalogPart& part : parts) {
        insert.bindValue(":set_catalog_id", setCatalogId);
        insert.bindValue(":part_id", part.partId);
        insert.bindValue(":color_id", part.colorId);
        insert.bindValue(":quantity_required", part.quantityRequired);
        insert.bindValue(":is_spare", part.isSpare ? 1 : 0);
        insert.bindValue(":provider", part.provider);
        insert.bindValue(":source", part.source);
        insert.bindValue(":created_utc", now);
        insert.bindValue(":modified_utc", now);
        if (!insert.exec()) { errorMessage = insert.lastError().text(); return false; }
    }
    return true;
}
