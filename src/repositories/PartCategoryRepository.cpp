#include "PartCategoryRepository.h"

#include "../database/DatabaseManager.h"

#include <QDateTime>
#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>

QList<PartCategory> PartCategoryRepository::getAll() const
{
    QList<PartCategory> categories;

    QSqlDatabase database = DatabaseManager::instance().database();

    QSqlQuery query(database);

    if (!query.exec(R"(
        SELECT
            id,
            name,
            rebrickable_id,
            created_utc,
            modified_utc
        FROM part_category
        ORDER BY name
    )")) {
        qCritical() << "Unable to retrieve part categories:" << query.lastError().text();

        return categories;
    }

    while (query.next())
        categories.append(partCategoryFromQuery(query));

    return categories;
}

std::optional<PartCategory> PartCategoryRepository::getById(int id) const
{
    QSqlDatabase database = DatabaseManager::instance().database();

    QSqlQuery query(database);

    query.prepare(R"(
        SELECT
            id,
            name,
            rebrickable_id,
            created_utc,
            modified_utc
        FROM part_category
        WHERE id = :id
    )");

    query.bindValue(":id", id);

    if (!query.exec()) {
        qCritical() << "Unable to retrieve part category:" << query.lastError().text();

        return std::nullopt;
    }

    if (!query.next())
        return std::nullopt;

    return partCategoryFromQuery(query);
}

std::optional<PartCategory> PartCategoryRepository::getByRebrickableId(int rebrickableId) const
{
    QSqlDatabase database = DatabaseManager::instance().database();

    QSqlQuery query(database);

    query.prepare(R"(
        SELECT
            id,
            name,
            rebrickable_id,
            created_utc,
            modified_utc
        FROM part_category
        WHERE rebrickable_id = :rebrickable_id
    )");

    query.bindValue(":rebrickable_id", rebrickableId);

    if (!query.exec()) {
        qCritical() << "Unable to retrieve part category by Rebrickable ID:"
                    << query.lastError().text();

        return std::nullopt;
    }

    if (!query.next())
        return std::nullopt;

    return partCategoryFromQuery(query);
}

PartCategory PartCategoryRepository::partCategoryFromQuery(const QSqlQuery& query) const
{
    PartCategory category;

    category.setId(query.value("id").toInt());

    category.setName(query.value("name").toString());

    category.setRebrickableId(query.value("rebrickable_id").toInt());

    category.setCreatedUtc(
        QDateTime::fromString(query.value("created_utc").toString(), Qt::ISODateWithMs));

    category.setModifiedUtc(
        QDateTime::fromString(query.value("modified_utc").toString(), Qt::ISODateWithMs));

    return category;
}