#include "EffectiveSetCompositionRepository.h"

#include "../database/DatabaseManager.h"

#include <QSqlError>
#include <QSqlQuery>

EffectiveSetCompositionRepository::EffectiveSetCompositionRepository(QSqlDatabase database)
    : m_database(database.isValid() ? database : DatabaseManager::instance().database())
{
}

EffectiveSetComposition EffectiveSetCompositionRepository::forSet(
    int setCatalogId, bool includeSpares) const
{
    EffectiveSetComposition result;
    QSqlQuery revision(m_database);
    revision.prepare("SELECT id,version,provider FROM set_inventory_revision "
                     "WHERE set_catalog_id=:set AND provider='Rebrickable' "
                     "AND is_active=1 AND is_preferred=1 LIMIT 1");
    revision.bindValue(":set", setCatalogId);
    if (!revision.exec()) { result.message=revision.lastError().text(); return result; }

    QSqlQuery parts(m_database);
    if (revision.next()) {
        result.source=EffectiveSetCompositionSource::PreferredRebrickableRevision;
        result.revisionId=revision.value(0).toInt();
        result.revisionVersion=revision.value(1).toInt();
        result.provider=revision.value(2).toString();
        parts.prepare("SELECT sip.id,sip.part_id,sip.color_id,sip.quantity,sip.is_spare,"
                      "p.part_number,p.name,c.name,c.rebrickable_id FROM set_inventory_part sip "
                      "JOIN part p ON p.id=sip.part_id JOIN color c ON c.id=sip.color_id "
                      "WHERE sip.set_inventory_revision_id=:id AND (:spares=1 OR sip.is_spare=0) "
                      "ORDER BY sip.is_spare,p.part_number COLLATE NOCASE,c.name COLLATE NOCASE,sip.id");
        parts.bindValue(":id",result.revisionId); parts.bindValue(":spares",includeSpares);
    } else {
        result.source=EffectiveSetCompositionSource::LegacyCatalogFallback;
        parts.prepare("SELECT scp.id,scp.part_id,scp.color_id,scp.quantity_required,scp.is_spare,"
                      "p.part_number,p.name,c.name,c.rebrickable_id FROM set_catalog_part scp "
                      "JOIN part p ON p.id=scp.part_id JOIN color c ON c.id=scp.color_id "
                      "WHERE scp.set_catalog_id=:id AND (:spares=1 OR scp.is_spare=0) "
                      "ORDER BY scp.is_spare,p.part_number COLLATE NOCASE,c.name COLLATE NOCASE,scp.id");
        parts.bindValue(":id",setCatalogId); parts.bindValue(":spares",includeSpares);
    }
    if(!parts.exec()){result.message=parts.lastError().text();return result;}
    while(parts.next()) result.parts.append({parts.value(0).toInt(),parts.value(1).toInt(),
                                              parts.value(2).toInt(),parts.value(3).toLongLong(),
                                              parts.value(4).toBool(),parts.value(5).toString(),
                                              parts.value(6).toString(),parts.value(7).toString(),
                                              parts.value(8).toInt()});
    result.success=true;
    if(result.source==EffectiveSetCompositionSource::PreferredRebrickableRevision&&result.parts.isEmpty())
        result.message=QStringLiteral("The preferred Rebrickable revision contains no Part rows.");
    return result;
}
