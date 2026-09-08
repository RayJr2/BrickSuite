#include "SetInventoryRevisionRepository.h"

#include "../database/DatabaseManager.h"

#include <QSqlQuery>

namespace {
SetInventoryRevision revisionFromQuery(const QSqlQuery& query)
{
    return {query.value(0).toInt(), query.value(1).toString(),
            query.value(2).toString(), query.value(3).toInt(),
            query.value(4).toInt(), query.value(5).toBool(),
            query.value(6).toBool()};
}
}

SetInventoryRevisionRepository::SetInventoryRevisionRepository(QSqlDatabase database)
    : m_database(database.isValid() ? database
                                    : DatabaseManager::instance().database())
{
}

QList<SetInventoryRevision> SetInventoryRevisionRepository::revisionsForSet(
    int setCatalogId, const QString& provider) const
{
    QSqlQuery query(m_database);
    QString sql = QStringLiteral(
        "SELECT id,provider,external_inventory_id,set_catalog_id,version,is_active,is_preferred "
        "FROM set_inventory_revision WHERE set_catalog_id=:set_id");
    if (!provider.isEmpty())
        sql += QStringLiteral(" AND provider=:provider");
    sql += QStringLiteral(" ORDER BY version DESC,external_inventory_id");
    query.prepare(sql);
    query.bindValue(":set_id", setCatalogId);
    if (!provider.isEmpty()) query.bindValue(":provider", provider);
    QList<SetInventoryRevision> result;
    if (query.exec()) while (query.next()) result.append(revisionFromQuery(query));
    return result;
}

SetInventoryRevision SetInventoryRevisionRepository::preferredRevisionForSet(
    int setCatalogId, const QString& provider) const
{
    QSqlQuery query(m_database);
    query.prepare("SELECT id,provider,external_inventory_id,set_catalog_id,version,is_active,is_preferred "
                  "FROM set_inventory_revision WHERE set_catalog_id=:set_id AND provider=:provider "
                  "AND is_active=1 AND is_preferred=1 LIMIT 1");
    query.bindValue(":set_id", setCatalogId);
    query.bindValue(":provider", provider);
    return query.exec() && query.next() ? revisionFromQuery(query) : SetInventoryRevision{};
}

QList<SetInventoryPart> SetInventoryRevisionRepository::partsForRevision(
    int revisionId, bool includeSpares) const
{
    QSqlQuery query(m_database);
    query.prepare("SELECT part_id,color_id,quantity,is_spare,image_url FROM set_inventory_part "
                  "WHERE set_inventory_revision_id=:id AND (:spares=1 OR is_spare=0) "
                  "ORDER BY is_spare,part_id,color_id");
    query.bindValue(":id", revisionId); query.bindValue(":spares", includeSpares);
    QList<SetInventoryPart> result;
    if (query.exec()) while (query.next()) result.append(
        {query.value(0).toInt(), query.value(1).toInt(), query.value(2).toLongLong(),
         query.value(3).toBool(), query.value(4).toString()});
    return result;
}

QList<SetInventoryMinifig> SetInventoryRevisionRepository::minifigsForRevision(int id) const
{
    QSqlQuery query(m_database);
    query.prepare("SELECT minifig_catalog_id,quantity FROM set_inventory_minifig "
                  "WHERE set_inventory_revision_id=:id ORDER BY minifig_catalog_id");
    query.bindValue(":id", id); QList<SetInventoryMinifig> result;
    if (query.exec()) while (query.next()) result.append(
        {query.value(0).toInt(), query.value(1).toLongLong()});
    return result;
}

QList<SetInventoryContainedSet> SetInventoryRevisionRepository::containedSetsForRevision(int id) const
{
    QSqlQuery query(m_database);
    query.prepare("SELECT contained_set_catalog_id,quantity FROM set_inventory_contained_set "
                  "WHERE set_inventory_revision_id=:id ORDER BY contained_set_catalog_id");
    query.bindValue(":id", id); QList<SetInventoryContainedSet> result;
    if (query.exec()) while (query.next()) result.append(
        {query.value(0).toInt(), query.value(1).toLongLong()});
    return result;
}

static QList<int> reverseIds(QSqlDatabase database, const QString& sql,
                             int first, int second = -1)
{
    QSqlQuery query(database); query.prepare(sql);
    query.bindValue(":first", first); if (second >= 0) query.bindValue(":second", second);
    QList<int> result; if (query.exec()) while (query.next()) result.append(query.value(0).toInt());
    return result;
}

QList<int> SetInventoryRevisionRepository::revisionIdsContainingPart(int partId, int colorId) const
{ return reverseIds(m_database,"SELECT set_inventory_revision_id FROM set_inventory_part WHERE part_id=:first AND color_id=:second", partId, colorId); }
QList<int> SetInventoryRevisionRepository::revisionIdsContainingMinifig(int id) const
{ return reverseIds(m_database,"SELECT set_inventory_revision_id FROM set_inventory_minifig WHERE minifig_catalog_id=:first", id); }
QList<int> SetInventoryRevisionRepository::revisionIdsContainingSet(int id) const
{ return reverseIds(m_database,"SELECT set_inventory_revision_id FROM set_inventory_contained_set WHERE contained_set_catalog_id=:first", id); }
