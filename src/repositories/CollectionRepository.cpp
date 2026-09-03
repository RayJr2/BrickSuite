#include "CollectionRepository.h"

#include "../database/DatabaseManager.h"

#include <QDateTime>
#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>
#include <QtGlobal>

namespace {
QString itemColumns()
{
    return QStringLiteral("ci.id, ci.workspace_id, ci.item_type, ci.set_catalog_id, "
                          "ci.minifig_catalog_id, ci.state, ci.storage_location_id, "
                          "ci.source_build_id, ci.nickname, ci.notes, ci.allow_parts_source, "
                          "ci.is_active, ci.created_utc, ci.modified_utc");
}

void appendCriteria(QString& sql, const CollectionSearchCriteria& criteria)
{
    if (criteria.activeState >= 0) sql += " AND ci.is_active = :active";
    if (criteria.type != CollectionItemType::Invalid) sql += " AND ci.item_type = :type";
    if (criteria.state != CollectionItemState::Invalid) sql += " AND ci.state = :state";
    if (criteria.storageLocationId > 0) {
        sql += R"( AND ci.storage_location_id IN (
            WITH RECURSIVE locations(id) AS (
                SELECT id FROM storage_location WHERE id = :location_id
                UNION ALL
                SELECT sl.id FROM storage_location sl
                JOIN locations parent ON sl.parent_location_id = parent.id
            ) SELECT candidate.id FROM locations candidate
              JOIN storage_location eligible ON eligible.id = candidate.id
              WHERE eligible.is_active = 1 AND eligible.allows_collection = 1))";
    }
    if (criteria.storageLocationId == -1) sql += " AND ci.storage_location_id IS NULL";
    if (!criteria.searchText.trimmed().isEmpty()) {
        sql += R"( AND (ci.nickname LIKE :search OR sc.set_number LIKE :search
            OR sc.name LIKE :search OR mc.name LIKE :search OR b.name LIKE :search
            OR b.set_number LIKE :search OR EXISTS (
                SELECT 1 FROM minifig_external_identifier mei_search
                WHERE mei_search.minifig_catalog_id = ci.minifig_catalog_id
                  AND mei_search.external_id LIKE :search)))";
    }
}

void bindCriteria(QSqlQuery& query, const CollectionSearchCriteria& criteria)
{
    query.bindValue(":workspace_id", criteria.workspaceId);
    if (criteria.activeState >= 0) query.bindValue(":active", criteria.activeState);
    if (criteria.type != CollectionItemType::Invalid)
        query.bindValue(":type", collectionItemTypeToString(criteria.type));
    if (criteria.state != CollectionItemState::Invalid)
        query.bindValue(":state", collectionItemStateToString(criteria.state));
    if (criteria.storageLocationId > 0)
        query.bindValue(":location_id", criteria.storageLocationId);
    if (!criteria.searchText.trimmed().isEmpty())
        query.bindValue(":search", "%" + criteria.searchText.trimmed() + "%");
}
}

bool CollectionRepository::create(CollectionItem& item)
{
    const QDateTime now = QDateTime::currentDateTimeUtc();
    QSqlQuery query(DatabaseManager::instance().database());
    query.prepare(R"(INSERT INTO collection_item
        (workspace_id,item_type,set_catalog_id,minifig_catalog_id,state,storage_location_id,
         source_build_id,nickname,notes,allow_parts_source,is_active,created_utc,modified_utc)
        VALUES (:workspace_id,:item_type,:set_catalog_id,:minifig_catalog_id,:state,
         :storage_location_id,:source_build_id,:nickname,:notes,:allow_parts_source,
         :is_active,:created_utc,:modified_utc))");
    query.bindValue(":workspace_id", item.workspaceId);
    query.bindValue(":item_type", collectionItemTypeToString(item.type));
    query.bindValue(":set_catalog_id", item.setCatalogId > 0 ? QVariant(item.setCatalogId) : QVariant());
    query.bindValue(":minifig_catalog_id", item.minifigCatalogId > 0 ? QVariant(item.minifigCatalogId) : QVariant());
    query.bindValue(":state", collectionItemStateToString(item.state));
    query.bindValue(":storage_location_id", item.storageLocationId > 0 ? QVariant(item.storageLocationId) : QVariant());
    query.bindValue(":source_build_id", item.sourceBuildId > 0 ? QVariant(item.sourceBuildId) : QVariant());
    query.bindValue(":nickname", item.nickname.trimmed());
    query.bindValue(":notes", item.notes);
    query.bindValue(":allow_parts_source", item.allowPartsSource ? 1 : 0);
    query.bindValue(":is_active", item.isActive ? 1 : 0);
    query.bindValue(":created_utc", now.toString(Qt::ISODateWithMs));
    query.bindValue(":modified_utc", now.toString(Qt::ISODateWithMs));
    if (!query.exec()) {
        qCritical() << "Unable to create Collection item:" << query.lastError().text();
        return false;
    }
    item.id = query.lastInsertId().toInt();
    item.createdUtc = now;
    item.modifiedUtc = now;
    return true;
}

std::optional<CollectionItem> CollectionRepository::getById(int id) const
{
    if (id <= 0) return std::nullopt;
    QSqlQuery query(DatabaseManager::instance().database());
    query.prepare(QString("SELECT %1 FROM collection_item ci WHERE ci.id=:id").arg(itemColumns()));
    query.bindValue(":id", id);
    if (!query.exec()) {
        qCritical() << "Unable to retrieve Collection item:" << query.lastError().text();
        return std::nullopt;
    }
    return query.next() ? std::optional<CollectionItem>(itemFromQuery(query)) : std::nullopt;
}

std::optional<CollectionItem> CollectionRepository::getBySourceBuild(int buildId) const
{
    if (buildId <= 0) return std::nullopt;
    QSqlQuery query(DatabaseManager::instance().database());
    query.prepare(QString("SELECT %1 FROM collection_item ci WHERE ci.source_build_id=:id")
                      .arg(itemColumns()));
    query.bindValue(":id", buildId);
    if (!query.exec()) {
        qCritical() << "Unable to retrieve Collection item by Build:" << query.lastError().text();
        return std::nullopt;
    }
    return query.next() ? std::optional<CollectionItem>(itemFromQuery(query)) : std::nullopt;
}

bool CollectionRepository::hasSourceBuild(int buildId) const
{
    return getBySourceBuild(buildId).has_value();
}

QList<CollectionSearchResult> CollectionRepository::search(
    const CollectionSearchCriteria& criteria) const
{
    QList<CollectionSearchResult> results;
    if (criteria.workspaceId <= 0) return results;
    QString sql = QString(R"(SELECT %1, sc.set_number,
        CASE ci.item_type WHEN 'Set' THEN sc.name WHEN 'Minifig' THEN mc.name ELSE b.name END display_name,
        CASE ci.item_type WHEN 'Set' THEN sc.image_url WHEN 'Minifig' THEN mc.image_url ELSE NULL END image_url,
        sl.name location_name, b.name source_build_name, b.set_number source_build_reference,
        CASE ci.item_type WHEN 'Set' THEN sc.set_number WHEN 'Minifig' THEN (
            SELECT mei.external_id FROM minifig_external_identifier mei
            WHERE mei.minifig_catalog_id=ci.minifig_catalog_id AND mei.provider='Rebrickable'
            ORDER BY mei.is_active DESC, mei.id LIMIT 1)
            ELSE b.set_number END display_reference
        FROM collection_item ci
        LEFT JOIN set_catalog sc ON sc.id=ci.set_catalog_id
        LEFT JOIN minifig_catalog mc ON mc.id=ci.minifig_catalog_id
        LEFT JOIN build b ON b.id=ci.source_build_id
        LEFT JOIN storage_location sl ON sl.id=ci.storage_location_id
        WHERE ci.workspace_id=:workspace_id)").arg(itemColumns());
    appendCriteria(sql, criteria);
    sql += " ORDER BY ci.is_active DESC, display_name COLLATE NOCASE, ci.id LIMIT :limit OFFSET :offset";
    QSqlQuery query(DatabaseManager::instance().database());
    query.prepare(sql);
    bindCriteria(query, criteria);
    query.bindValue(":limit", qBound(1, criteria.limit, 500));
    query.bindValue(":offset", qMax(0, criteria.offset));
    if (!query.exec()) {
        qCritical() << "Unable to search Collection:" << query.lastError().text();
        return results;
    }
    while (query.next()) {
        CollectionSearchResult result;
        result.item = itemFromQuery(query);
        result.displayReference = query.value("display_reference").toString();
        result.displayName = query.value("display_name").toString();
        result.imageUrl = query.value("image_url").toString();
        result.locationName = query.value("location_name").toString();
        result.sourceBuildName = query.value("source_build_name").toString();
        result.sourceBuildReference = query.value("source_build_reference").toString();
        results.append(result);
    }
    return results;
}

std::optional<CollectionSearchResult> CollectionRepository::displayById(int id) const
{
    if (id <= 0) return std::nullopt;
    QString sql = QString(R"(SELECT %1,
        CASE ci.item_type WHEN 'Set' THEN sc.name WHEN 'Minifig' THEN mc.name ELSE b.name END display_name,
        CASE ci.item_type WHEN 'Set' THEN sc.image_url WHEN 'Minifig' THEN mc.image_url ELSE NULL END image_url,
        sl.name location_name, b.name source_build_name, b.set_number source_build_reference,
        CASE ci.item_type WHEN 'Set' THEN sc.set_number WHEN 'Minifig' THEN (
            SELECT mei.external_id FROM minifig_external_identifier mei
            WHERE mei.minifig_catalog_id=ci.minifig_catalog_id AND mei.provider='Rebrickable'
            ORDER BY mei.is_active DESC, mei.id LIMIT 1)
            ELSE b.set_number END display_reference
        FROM collection_item ci
        LEFT JOIN set_catalog sc ON sc.id=ci.set_catalog_id
        LEFT JOIN minifig_catalog mc ON mc.id=ci.minifig_catalog_id
        LEFT JOIN build b ON b.id=ci.source_build_id
        LEFT JOIN storage_location sl ON sl.id=ci.storage_location_id
        WHERE ci.id=:id)").arg(itemColumns());
    QSqlQuery query(DatabaseManager::instance().database());
    query.prepare(sql);
    query.bindValue(":id", id);
    if (!query.exec() || !query.next()) return std::nullopt;
    CollectionSearchResult result;
    result.item = itemFromQuery(query);
    result.displayReference = query.value("display_reference").toString();
    result.displayName = query.value("display_name").toString();
    result.imageUrl = query.value("image_url").toString();
    result.locationName = query.value("location_name").toString();
    result.sourceBuildName = query.value("source_build_name").toString();
    result.sourceBuildReference = query.value("source_build_reference").toString();
    return result;
}

int CollectionRepository::count(const CollectionSearchCriteria& criteria) const
{
    if (criteria.workspaceId <= 0) return 0;
    QString sql = R"(SELECT COUNT(*) FROM collection_item ci
        LEFT JOIN set_catalog sc ON sc.id=ci.set_catalog_id
        LEFT JOIN minifig_catalog mc ON mc.id=ci.minifig_catalog_id
        LEFT JOIN build b ON b.id=ci.source_build_id
        WHERE ci.workspace_id=:workspace_id)";
    appendCriteria(sql, criteria);
    QSqlQuery query(DatabaseManager::instance().database());
    query.prepare(sql);
    bindCriteria(query, criteria);
    if (!query.exec() || !query.next()) {
        qCritical() << "Unable to count Collection items:" << query.lastError().text();
        return 0;
    }
    return query.value(0).toInt();
}

bool CollectionRepository::update(CollectionItem& item)
{
    const QDateTime now = QDateTime::currentDateTimeUtc();
    QSqlQuery query(DatabaseManager::instance().database());
    query.prepare(R"(UPDATE collection_item SET state=:state,
        storage_location_id=:storage_location_id,nickname=:nickname,notes=:notes,
        allow_parts_source=:allow_parts_source,modified_utc=:modified_utc
        WHERE id=:id AND workspace_id=:workspace_id)");
    query.bindValue(":state", collectionItemStateToString(item.state));
    query.bindValue(":storage_location_id", item.storageLocationId > 0 ? QVariant(item.storageLocationId) : QVariant());
    query.bindValue(":nickname", item.nickname.trimmed());
    query.bindValue(":notes", item.notes);
    query.bindValue(":allow_parts_source", item.allowPartsSource ? 1 : 0);
    query.bindValue(":modified_utc", now.toString(Qt::ISODateWithMs));
    query.bindValue(":id", item.id);
    query.bindValue(":workspace_id", item.workspaceId);
    if (!query.exec() || query.numRowsAffected() != 1) {
        qCritical() << "Unable to update Collection item:" << query.lastError().text();
        return false;
    }
    item.modifiedUtc = now;
    return true;
}

bool CollectionRepository::setActive(int itemId, bool active)
{
    QSqlQuery query(DatabaseManager::instance().database());
    query.prepare("UPDATE collection_item SET is_active=:active,modified_utc=:modified WHERE id=:id");
    query.bindValue(":active", active ? 1 : 0);
    query.bindValue(":modified", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    query.bindValue(":id", itemId);
    if (!query.exec()) {
        qCritical() << "Unable to change Collection activity:" << query.lastError().text();
        return false;
    }
    return query.numRowsAffected() == 1;
}

CollectionItem CollectionRepository::itemFromQuery(const QSqlQuery& query)
{
    CollectionItem item;
    item.id = query.value("id").toInt();
    item.workspaceId = query.value("workspace_id").toInt();
    item.type = collectionItemTypeFromString(query.value("item_type").toString());
    item.setCatalogId = query.value("set_catalog_id").toInt();
    item.minifigCatalogId = query.value("minifig_catalog_id").toInt();
    item.state = collectionItemStateFromString(query.value("state").toString());
    item.storageLocationId = query.value("storage_location_id").toInt();
    item.sourceBuildId = query.value("source_build_id").toInt();
    item.nickname = query.value("nickname").toString();
    item.notes = query.value("notes").toString();
    item.allowPartsSource = query.value("allow_parts_source").toBool();
    item.isActive = query.value("is_active").toBool();
    item.createdUtc = QDateTime::fromString(query.value("created_utc").toString(), Qt::ISODateWithMs);
    item.modifiedUtc = QDateTime::fromString(query.value("modified_utc").toString(), Qt::ISODateWithMs);
    return item;
}
