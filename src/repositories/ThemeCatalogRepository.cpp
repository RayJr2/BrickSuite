#include "ThemeCatalogRepository.h"

#include "../database/DatabaseManager.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QDebug>

QList<ThemeCatalogItem> ThemeCatalogRepository::activeFilterHierarchy() const
{
    QList<ThemeCatalogItem> themes;
    QSqlQuery query(DatabaseManager::instance().database());
    query.prepare(R"(
        WITH RECURSIVE used(id) AS (
            SELECT DISTINCT mt.theme_catalog_id
            FROM minifig_theme mt
            JOIN minifig_catalog mc ON mc.id = mt.minifig_catalog_id
            JOIN theme_catalog tc ON tc.id = mt.theme_catalog_id
            WHERE mt.provider = :provider AND mc.is_active = 1 AND tc.is_active = 1
            UNION
            SELECT tc.parent_theme_catalog_id
            FROM theme_catalog tc JOIN used u ON u.id = tc.id
            WHERE tc.parent_theme_catalog_id IS NOT NULL
        ), tree(id, name, parent_id, depth, path, qualified_name) AS (
            SELECT tc.id, tc.name, tc.parent_theme_catalog_id, 0,
                   printf('%08d', tc.id), tc.name
            FROM theme_catalog tc
            WHERE tc.parent_theme_catalog_id IS NULL AND tc.is_active = 1
              AND tc.id IN used
            UNION ALL
            SELECT tc.id, tc.name, tc.parent_theme_catalog_id, tree.depth + 1,
                   tree.path || '/' || printf('%08d', tc.id),
                   tree.qualified_name || ' → ' || tc.name
            FROM theme_catalog tc JOIN tree ON tc.parent_theme_catalog_id = tree.id
            WHERE tc.is_active = 1 AND tc.id IN used
        )
        SELECT tree.id, tree.name, COALESCE(tree.parent_id, 0), tree.depth,
               tei.provider, tei.external_id, tree.qualified_name
        FROM tree
        LEFT JOIN theme_external_identifier tei
          ON tei.theme_catalog_id = tree.id AND tei.provider = :provider
             AND tei.is_active = 1
        ORDER BY tree.path
    )");
    query.bindValue(":provider", QStringLiteral("Rebrickable"));
    if (!query.exec()) {
        qCritical() << "Unable to load Theme filter hierarchy:" << query.lastError().text();
        return themes;
    }
    while (query.next()) {
        ThemeCatalogItem item;
        item.id = query.value(0).toInt(); item.name = query.value(1).toString();
        item.parentThemeCatalogId = query.value(2).toInt(); item.depth = query.value(3).toInt();
        item.provider = query.value(4).toString(); item.externalId = query.value(5).toString();
        item.qualifiedName = query.value(6).toString();
        themes.append(item);
    }
    return themes;
}
