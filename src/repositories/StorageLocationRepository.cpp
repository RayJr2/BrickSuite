#include "StorageLocationRepository.h"

#include "../database/DatabaseManager.h"

#include <QDateTime>
#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

bool StorageLocationRepository::create(StorageLocation& location)
{
    if (location.workspaceId() <= 0 || location.locationTypeId() <= 0
        || location.name().trimmed().isEmpty()) {
        qCritical() << "Invalid storage location.";

        return false;
    }

    QSqlDatabase database = DatabaseManager::instance().database();

    if (!database.isOpen())
        return false;

    const QDateTime now = QDateTime::currentDateTimeUtc();

    QSqlQuery query(database);

    query.prepare(R"(
        INSERT INTO storage_location
        (
            workspace_id,
            parent_location_id,
            location_type_id,
            name,
            description,
            sort_order,
            is_active,
            created_utc,
            modified_utc
        )
        VALUES
        (
            :workspace_id,
            :parent_location_id,
            :location_type_id,
            :name,
            :description,
            :sort_order,
            :is_active,
            :created_utc,
            :modified_utc
        )
    )");

    query.bindValue(":workspace_id", location.workspaceId());

    if (location.parentLocationId() > 0) {
        query.bindValue(":parent_location_id", location.parentLocationId());
    } else {
        query.bindValue(":parent_location_id", QVariant());
    }

    query.bindValue(":location_type_id", location.locationTypeId());

    query.bindValue(":name", location.name().trimmed());

    query.bindValue(":description", location.description());

    query.bindValue(":sort_order", location.sortOrder());

    query.bindValue(":is_active", location.isActive() ? 1 : 0);

    query.bindValue(":created_utc", now.toString(Qt::ISODateWithMs));

    query.bindValue(":modified_utc", now.toString(Qt::ISODateWithMs));

    if (!query.exec()) {
        qCritical() << "Unable to create storage location:" << query.lastError().text();

        return false;
    }

    location.setId(query.lastInsertId().toInt());

    location.setCreatedUtc(now);
    location.setModifiedUtc(now);

    return true;
}

QList<StorageLocation> StorageLocationRepository::getByWorkspace(int workspaceId) const
{
    QList<StorageLocation> locations;

    QSqlDatabase database = DatabaseManager::instance().database();

    QSqlQuery query(database);

    query.prepare(R"(
        SELECT
            id,
            workspace_id,
            parent_location_id,
            location_type_id,
            name,
            description,
            sort_order,
            is_active,
            created_utc,
            modified_utc
        FROM storage_location
        WHERE workspace_id = :workspace_id
          AND is_active = 1
        ORDER BY sort_order, name
    )");

    query.bindValue(":workspace_id", workspaceId);

    if (!query.exec()) {
        qCritical() << "Unable to retrieve storage locations:" << query.lastError().text();

        return locations;
    }

    while (query.next())
        locations.append(locationFromQuery(query));

    return locations;
}

QList<StorageLocation> StorageLocationRepository::getChildren(int workspaceId,
                                                              int parentLocationId) const
{
    QList<StorageLocation> locations;

    QSqlDatabase database = DatabaseManager::instance().database();

    QSqlQuery query(database);

    if (parentLocationId > 0) {
        query.prepare(R"(
            SELECT
                id,
                workspace_id,
                parent_location_id,
                location_type_id,
                name,
                description,
                sort_order,
                is_active,
                created_utc,
                modified_utc
            FROM storage_location
            WHERE workspace_id = :workspace_id
              AND parent_location_id = :parent_location_id
              AND is_active = 1
            ORDER BY sort_order, name
        )");

        query.bindValue(":parent_location_id", parentLocationId);
    } else {
        query.prepare(R"(
            SELECT
                id,
                workspace_id,
                parent_location_id,
                location_type_id,
                name,
                description,
                sort_order,
                is_active,
                created_utc,
                modified_utc
            FROM storage_location
            WHERE workspace_id = :workspace_id
              AND parent_location_id IS NULL
              AND is_active = 1
            ORDER BY sort_order, name
        )");
    }

    query.bindValue(":workspace_id", workspaceId);

    if (!query.exec()) {
        qCritical() << "Unable to retrieve child storage locations:" << query.lastError().text();

        return locations;
    }

    while (query.next())
        locations.append(locationFromQuery(query));

    return locations;
}

std::optional<StorageLocation> StorageLocationRepository::getById(int id) const
{
    QSqlDatabase database = DatabaseManager::instance().database();

    QSqlQuery query(database);

    query.prepare(R"(
        SELECT
            id,
            workspace_id,
            parent_location_id,
            location_type_id,
            name,
            description,
            sort_order,
            is_active,
            created_utc,
            modified_utc
        FROM storage_location
        WHERE id = :id
    )");

    query.bindValue(":id", id);

    if (!query.exec()) {
        qCritical() << "Unable to retrieve storage location:" << query.lastError().text();

        return std::nullopt;
    }

    if (!query.next())
        return std::nullopt;

    return locationFromQuery(query);
}

bool StorageLocationRepository::update(StorageLocation& location)
{
    if (location.id() <= 0 || location.workspaceId() <= 0 || location.locationTypeId() <= 0
        || location.name().trimmed().isEmpty()) {
        return false;
    }

    QSqlDatabase database = DatabaseManager::instance().database();

    const QDateTime now = QDateTime::currentDateTimeUtc();

    QSqlQuery query(database);

    query.prepare(R"(
        UPDATE storage_location
        SET
            parent_location_id = :parent_location_id,
            location_type_id = :location_type_id,
            name = :name,
            description = :description,
            sort_order = :sort_order,
            is_active = :is_active,
            modified_utc = :modified_utc
        WHERE id = :id
          AND workspace_id = :workspace_id
    )");

    if (location.parentLocationId() > 0) {
        query.bindValue(":parent_location_id", location.parentLocationId());
    } else {
        query.bindValue(":parent_location_id", QVariant());
    }

    query.bindValue(":location_type_id", location.locationTypeId());

    query.bindValue(":name", location.name().trimmed());

    query.bindValue(":description", location.description());

    query.bindValue(":sort_order", location.sortOrder());

    query.bindValue(":is_active", location.isActive() ? 1 : 0);

    query.bindValue(":modified_utc", now.toString(Qt::ISODateWithMs));

    query.bindValue(":id", location.id());

    query.bindValue(":workspace_id", location.workspaceId());

    if (!query.exec()) {
        qCritical() << "Unable to update storage location:" << query.lastError().text();

        return false;
    }

    if (query.numRowsAffected() <= 0)
        return false;

    location.setModifiedUtc(now);

    return true;
}

StorageLocation StorageLocationRepository::locationFromQuery(const QSqlQuery& query) const
{
    StorageLocation location;

    location.setId(query.value("id").toInt());

    location.setWorkspaceId(query.value("workspace_id").toInt());

    if (!query.value("parent_location_id").isNull()) {
        location.setParentLocationId(query.value("parent_location_id").toInt());
    }

    location.setLocationTypeId(query.value("location_type_id").toInt());

    location.setName(query.value("name").toString());

    location.setDescription(query.value("description").toString());

    location.setSortOrder(query.value("sort_order").toInt());

    location.setIsActive(query.value("is_active").toInt() != 0);

    location.setCreatedUtc(
        QDateTime::fromString(query.value("created_utc").toString(), Qt::ISODateWithMs));

    location.setModifiedUtc(
        QDateTime::fromString(query.value("modified_utc").toString(), Qt::ISODateWithMs));

    return location;
}

bool StorageLocationRepository::hasChildren(int locationId) const
{
    QSqlDatabase database = DatabaseManager::instance().database();

    QSqlQuery query(database);

    query.prepare(R"(
        SELECT COUNT(*)
        FROM storage_location
        WHERE parent_location_id = :location_id
          AND is_active = 1
    )");

    query.bindValue(":location_id", locationId);

    if (!query.exec()) {
        qCritical() << "Unable to check storage location children:" << query.lastError().text();

        return false;
    }

    if (!query.next())
        return false;

    return query.value(0).toInt() > 0;
}

bool StorageLocationRepository::isDescendant(int locationId, int possibleDescendantId) const
{
    if (locationId <= 0 || possibleDescendantId <= 0) {
        return false;
    }

    if (locationId == possibleDescendantId)
        return true;

    QSqlDatabase database = DatabaseManager::instance().database();

    QSqlQuery query(database);

    query.prepare(R"(
        WITH RECURSIVE descendants(id) AS
        (
            SELECT id
            FROM storage_location
            WHERE parent_location_id = :location_id

            UNION ALL

            SELECT sl.id
            FROM storage_location sl
            INNER JOIN descendants d
                ON sl.parent_location_id = d.id
        )
        SELECT 1
        FROM descendants
        WHERE id = :possible_descendant_id
        LIMIT 1
    )");

    query.bindValue(":location_id", locationId);

    query.bindValue(":possible_descendant_id", possibleDescendantId);

    if (!query.exec()) {
        qCritical() << "Unable to check storage hierarchy:" << query.lastError().text();

        return false;
    }

    return query.next();
}

bool StorageLocationRepository::deactivate(int locationId)
{
    if (locationId <= 0)
        return false;

    QSqlDatabase database = DatabaseManager::instance().database();

    const QString now = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);

    QSqlQuery query(database);

    query.prepare(R"(
        UPDATE storage_location
        SET
            is_active = 0,
            modified_utc = :modified_utc
        WHERE id = :id
          AND is_active = 1
    )");

    query.bindValue(":modified_utc", now);

    query.bindValue(":id", locationId);

    if (!query.exec()) {
        qCritical() << "Unable to deactivate storage location:" << query.lastError().text();

        return false;
    }

    return query.numRowsAffected() > 0;
}