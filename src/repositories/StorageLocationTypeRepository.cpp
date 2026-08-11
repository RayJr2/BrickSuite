#include "StorageLocationTypeRepository.h"

#include "../database/DatabaseManager.h"

#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>

QList<StorageLocationType> StorageLocationTypeRepository::getAll() const
{
    QList<StorageLocationType> types;

    QSqlDatabase database = DatabaseManager::instance().database();

    QSqlQuery query(database);

    if (!query.exec(R"(
        SELECT
            id,
            name,
            description,
            is_system,
            is_active,
            sort_order
        FROM storage_location_type
        ORDER BY sort_order, name
    )")) {
        qCritical() << "Unable to retrieve storage location types:" << query.lastError().text();

        return types;
    }

    while (query.next())
        types.append(locationTypeFromQuery(query));

    return types;
}

QList<StorageLocationType> StorageLocationTypeRepository::getActive() const
{
    QList<StorageLocationType> types;

    QSqlDatabase database = DatabaseManager::instance().database();

    QSqlQuery query(database);

    if (!query.exec(R"(
        SELECT
            id,
            name,
            description,
            is_system,
            is_active,
            sort_order
        FROM storage_location_type
        WHERE is_active = 1
        ORDER BY sort_order, name
    )")) {
        qCritical() << "Unable to retrieve active storage location types:"
                    << query.lastError().text();

        return types;
    }

    while (query.next())
        types.append(locationTypeFromQuery(query));

    return types;
}

std::optional<StorageLocationType> StorageLocationTypeRepository::getById(int id) const
{
    QSqlDatabase database = DatabaseManager::instance().database();

    QSqlQuery query(database);

    query.prepare(R"(
        SELECT
            id,
            name,
            description,
            is_system,
            is_active,
            sort_order
        FROM storage_location_type
        WHERE id = :id
    )");

    query.bindValue(":id", id);

    if (!query.exec()) {
        qCritical() << "Unable to retrieve storage location type:" << query.lastError().text();

        return std::nullopt;
    }

    if (!query.next())
        return std::nullopt;

    return locationTypeFromQuery(query);
}

StorageLocationType StorageLocationTypeRepository::locationTypeFromQuery(const QSqlQuery& query) const
{
    StorageLocationType type;

    type.setId(query.value("id").toInt());

    type.setName(query.value("name").toString());

    type.setDescription(query.value("description").toString());

    type.setIsSystem(query.value("is_system").toInt() != 0);

    type.setIsActive(query.value("is_active").toInt() != 0);

    type.setSortOrder(query.value("sort_order").toInt());

    return type;
}