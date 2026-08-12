#include "ColorRepository.h"

#include "../database/DatabaseManager.h"

#include <QDateTime>
#include <QDebug>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

QList<Color> ColorRepository::getAll() const
{
    QList<Color> colors;

    QSqlDatabase database = DatabaseManager::instance().database();

    QSqlQuery query(database);

    if (!query.exec(R"(
        SELECT
            id,
            name,
            rgb,
            is_transparent,
            rebrickable_id,
            created_utc,
            modified_utc
        FROM color
        ORDER BY name
    )")) {
        qCritical() << "Unable to retrieve colors:" << query.lastError().text();

        return colors;
    }

    while (query.next()) {
        colors.append(colorFromQuery(query));
    }

    return colors;
}

std::optional<Color> ColorRepository::getById(int id) const
{
    QSqlDatabase database = DatabaseManager::instance().database();

    QSqlQuery query(database);

    query.prepare(R"(
        SELECT
            id,
            name,
            rgb,
            is_transparent,
            rebrickable_id,
            created_utc,
            modified_utc
        FROM color
        WHERE id = :id
    )");

    query.bindValue(":id", id);

    if (!query.exec()) {
        qCritical() << "Unable to retrieve color:" << query.lastError().text();

        return std::nullopt;
    }

    if (!query.next())
        return std::nullopt;

    return colorFromQuery(query);
}

std::optional<Color> ColorRepository::getByRebrickableId(int rebrickableId) const
{
    QSqlDatabase database = DatabaseManager::instance().database();

    QSqlQuery query(database);

    query.prepare(R"(
        SELECT
            id,
            name,
            rgb,
            is_transparent,
            rebrickable_id,
            created_utc,
            modified_utc
        FROM color
        WHERE rebrickable_id = :rebrickable_id
    )");

    query.bindValue(":rebrickable_id", rebrickableId);

    if (!query.exec()) {
        qCritical() << "Unable to retrieve color by Rebrickable ID:" << query.lastError().text();

        return std::nullopt;
    }

    if (!query.next())
        return std::nullopt;

    return colorFromQuery(query);
}

Color ColorRepository::colorFromQuery(const QSqlQuery& query) const
{
    Color color;

    color.setId(query.value("id").toInt());

    color.setName(query.value("name").toString());

    color.setRgb(query.value("rgb").toString());

    color.setIsTransparent(query.value("is_transparent").toInt() != 0);

    color.setRebrickableId(query.value("rebrickable_id").toInt());

    color.setCreatedUtc(
        QDateTime::fromString(query.value("created_utc").toString(), Qt::ISODateWithMs));

    color.setModifiedUtc(
        QDateTime::fromString(query.value("modified_utc").toString(), Qt::ISODateWithMs));

    return color;
}