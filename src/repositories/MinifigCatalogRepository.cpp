#include "MinifigCatalogRepository.h"

#include "../database/DatabaseManager.h"

#include <QDateTime>
#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>
#include <QtGlobal>

namespace {
QString selectColumns()
{
    return QStringLiteral("mc.id, mc.name, mc.num_parts, mc.image_url, "
                          "mc.is_active, mc.created_utc, mc.modified_utc");
}

void appendCriteria(QString& sql,
                    const MinifigCatalogSearchCriteria& criteria,
                    const QString& searchText,
                    const QString& provider)
{
    if (!criteria.includeInactive)
        sql += QStringLiteral(" AND mc.is_active = 1");

    if (!searchText.isEmpty()) {
        sql += QStringLiteral(R"(
            AND (
                mc.name LIKE :search
                OR EXISTS (
                    SELECT 1
                    FROM minifig_external_identifier mei_search
                    WHERE mei_search.minifig_catalog_id = mc.id
                      AND mei_search.external_id LIKE :search
                      AND (:include_inactive = 1 OR mei_search.is_active = 1)
                )
            )
        )");
    }

    if (!provider.isEmpty()) {
        sql += QStringLiteral(R"(
            AND EXISTS (
                SELECT 1
                FROM minifig_external_identifier mei_provider
                WHERE mei_provider.minifig_catalog_id = mc.id
                  AND mei_provider.provider = :provider
                  AND (:include_inactive = 1 OR mei_provider.is_active = 1)
            )
        )");
    }
}

void bindCriteria(QSqlQuery& query,
                  const MinifigCatalogSearchCriteria& criteria,
                  const QString& searchText,
                  const QString& provider)
{
    if (!searchText.isEmpty())
        query.bindValue(":search", "%" + searchText + "%");

    if (!searchText.isEmpty() || !provider.isEmpty())
        query.bindValue(":include_inactive", criteria.includeInactive ? 1 : 0);

    if (!provider.isEmpty())
        query.bindValue(":provider", provider);
}
} // namespace

std::optional<MinifigCatalogItem> MinifigCatalogRepository::getById(int id) const
{
    if (id <= 0)
        return std::nullopt;

    QSqlQuery query(DatabaseManager::instance().database());
    query.prepare(QStringLiteral("SELECT %1 FROM minifig_catalog mc WHERE mc.id = :id")
                      .arg(selectColumns()));
    query.bindValue(":id", id);

    if (!query.exec()) {
        qCritical() << "Unable to retrieve Minifig Catalog item:"
                    << query.lastError().text();
        return std::nullopt;
    }

    return query.next() ? std::optional<MinifigCatalogItem>(itemFromQuery(query))
                        : std::nullopt;
}

std::optional<MinifigCatalogItem> MinifigCatalogRepository::getByExternalIdentifier(
    const QString& provider,
    const QString& externalId) const
{
    const QString trimmedProvider = provider.trimmed();
    const QString trimmedExternalId = externalId.trimmed();

    if (trimmedProvider.isEmpty() || trimmedExternalId.isEmpty())
        return std::nullopt;

    QSqlQuery query(DatabaseManager::instance().database());
    query.prepare(QStringLiteral(R"(
        SELECT %1
        FROM minifig_catalog mc
        INNER JOIN minifig_external_identifier mei
            ON mei.minifig_catalog_id = mc.id
        WHERE mei.provider = :provider
          AND mei.external_id = :external_id
        LIMIT 1
    )").arg(selectColumns()));
    query.bindValue(":provider", trimmedProvider);
    query.bindValue(":external_id", trimmedExternalId);

    if (!query.exec()) {
        qCritical() << "Unable to retrieve Minifig by external identifier:"
                    << query.lastError().text();
        return std::nullopt;
    }

    return query.next() ? std::optional<MinifigCatalogItem>(itemFromQuery(query))
                        : std::nullopt;
}

QList<MinifigExternalIdentifier> MinifigCatalogRepository::identifiersForMinifig(
    int minifigCatalogId,
    bool activeOnly) const
{
    QList<MinifigExternalIdentifier> identifiers;

    if (minifigCatalogId <= 0)
        return identifiers;

    QSqlQuery query(DatabaseManager::instance().database());
    QString sql = QStringLiteral(R"(
        SELECT id, minifig_catalog_id, provider, external_id, source, is_active
        FROM minifig_external_identifier
        WHERE minifig_catalog_id = :minifig_catalog_id
    )");
    if (activeOnly)
        sql += QStringLiteral(" AND is_active = 1");
    sql += QStringLiteral(" ORDER BY provider, external_id");

    query.prepare(sql);
    query.bindValue(":minifig_catalog_id", minifigCatalogId);

    if (!query.exec()) {
        qCritical() << "Unable to retrieve Minifig external identifiers:"
                    << query.lastError().text();
        return identifiers;
    }

    while (query.next()) {
        MinifigExternalIdentifier identifier;
        identifier.id = query.value("id").toInt();
        identifier.minifigCatalogId = query.value("minifig_catalog_id").toInt();
        identifier.provider = query.value("provider").toString();
        identifier.externalId = query.value("external_id").toString();
        identifier.source = query.value("source").toString();
        identifier.isActive = query.value("is_active").toBool();
        identifiers.append(identifier);
    }

    return identifiers;
}

QList<MinifigCatalogSearchResult> MinifigCatalogRepository::search(
    const MinifigCatalogSearchCriteria& criteria) const
{
    QList<MinifigCatalogSearchResult> results;
    const QString searchText = criteria.searchText.trimmed();
    const QString provider = criteria.provider.trimmed();
    QString sql = QStringLiteral(R"(
        SELECT %1,
               (
                   SELECT mei_result.external_id
                   FROM minifig_external_identifier mei_result
                   WHERE mei_result.minifig_catalog_id = mc.id
                     AND mei_result.provider = 'Rebrickable'
                     AND (:include_inactive_result = 1 OR mei_result.is_active = 1)
                   ORDER BY mei_result.is_active DESC, mei_result.id
                   LIMIT 1
               ) AS rebrickable_external_id
        FROM minifig_catalog mc
        WHERE 1 = 1
    )").arg(selectColumns());
    appendCriteria(sql, criteria, searchText, provider);
    sql += QStringLiteral(" ORDER BY mc.name COLLATE NOCASE, mc.id LIMIT :limit OFFSET :offset");

    QSqlQuery query(DatabaseManager::instance().database());
    if (!query.prepare(sql)) {
        qCritical() << "Unable to prepare Minifig Catalog search:"
                    << query.lastError().text();
        return results;
    }

    bindCriteria(query, criteria, searchText, provider);
    query.bindValue(":include_inactive_result", criteria.includeInactive ? 1 : 0);
    query.bindValue(":limit", qBound(1, criteria.limit, 500));
    query.bindValue(":offset", qMax(0, criteria.offset));

    if (!query.exec()) {
        qCritical() << "Unable to search Minifig Catalog:" << query.lastError().text();
        return results;
    }

    while (query.next()) {
        MinifigCatalogSearchResult result;
        result.minifig = itemFromQuery(query);
        result.rebrickableExternalId = query.value("rebrickable_external_id").toString();
        results.append(result);
    }

    return results;
}

int MinifigCatalogRepository::count(const MinifigCatalogSearchCriteria& criteria) const
{
    const QString searchText = criteria.searchText.trimmed();
    const QString provider = criteria.provider.trimmed();
    QString sql = QStringLiteral("SELECT COUNT(*) FROM minifig_catalog mc WHERE 1 = 1");
    appendCriteria(sql, criteria, searchText, provider);

    QSqlQuery query(DatabaseManager::instance().database());
    if (!query.prepare(sql)) {
        qCritical() << "Unable to prepare Minifig Catalog count:"
                    << query.lastError().text();
        return 0;
    }

    bindCriteria(query, criteria, searchText, provider);

    if (!query.exec() || !query.next()) {
        qCritical() << "Unable to count Minifig Catalog records:"
                    << query.lastError().text();
        return 0;
    }

    return query.value(0).toInt();
}

int MinifigCatalogRepository::count(bool includeInactive) const
{
    MinifigCatalogSearchCriteria criteria;
    criteria.includeInactive = includeInactive;
    return count(criteria);
}

MinifigCatalogItem MinifigCatalogRepository::itemFromQuery(const QSqlQuery& query)
{
    MinifigCatalogItem item;
    item.setId(query.value("id").toInt());
    item.setName(query.value("name").toString());
    item.setNumberOfParts(query.value("num_parts").toInt());
    item.setImageUrl(query.value("image_url").toString());
    item.setIsActive(query.value("is_active").toBool());
    item.setCreatedUtc(QDateTime::fromString(query.value("created_utc").toString(),
                                             Qt::ISODateWithMs));
    item.setModifiedUtc(QDateTime::fromString(query.value("modified_utc").toString(),
                                              Qt::ISODateWithMs));
    return item;
}
