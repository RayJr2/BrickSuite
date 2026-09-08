#include "PartElementIdentifierRepository.h"

#include "../database/DatabaseManager.h"

#include <QSqlQuery>

namespace {
PartElementIdentifier fromQuery(const QSqlQuery& query)
{
    return {query.value(0).toInt(), query.value(1).toString(),
            query.value(2).toString(), query.value(3).toInt(),
            query.value(4).toInt(), query.value(5).toString(),
            query.value(6).toBool()};
}
}

PartElementIdentifierRepository::PartElementIdentifierRepository(QSqlDatabase database)
    : m_database(database.isValid() ? database : DatabaseManager::instance().database())
{
}

PartElementIdentifier PartElementIdentifierRepository::findByElementId(
    const QString& provider, const QString& elementId) const
{
    QSqlQuery query(m_database);
    query.prepare("SELECT id,provider,element_id,part_id,color_id,design_id,is_active "
                  "FROM part_element_identifier WHERE provider=:provider "
                  "AND element_id=:element AND is_active=1 LIMIT 1");
    query.bindValue(":provider", provider);
    query.bindValue(":element", elementId);
    return query.exec() && query.next() ? fromQuery(query) : PartElementIdentifier{};
}

QList<PartElementIdentifier> PartElementIdentifierRepository::findByPartColor(
    int partId, int colorId, const QString& provider) const
{
    QSqlQuery query(m_database);
    QString sql = "SELECT id,provider,element_id,part_id,color_id,design_id,is_active "
                  "FROM part_element_identifier WHERE part_id=:part AND color_id=:color "
                  "AND is_active=1";
    if (!provider.isEmpty()) sql += " AND provider=:provider";
    sql += " ORDER BY provider COLLATE NOCASE,element_id COLLATE NOCASE";
    query.prepare(sql); query.bindValue(":part", partId); query.bindValue(":color", colorId);
    if (!provider.isEmpty()) query.bindValue(":provider", provider);
    QList<PartElementIdentifier> result;
    if (query.exec()) while (query.next()) result.append(fromQuery(query));
    return result;
}
