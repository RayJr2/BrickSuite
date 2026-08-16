#include "SetCatalogRepository.h"

#include "../database/DatabaseManager.h"

#include <QDateTime>
#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>
#include <QtGlobal>

std::optional<SetCatalogItem> SetCatalogRepository::getById(int id) const
{
    if (id <= 0)
        return std::nullopt;

    QSqlDatabase database = DatabaseManager::instance().database();

    QSqlQuery query(database);

    query.prepare(R"(
        SELECT
            id,
            set_number,
            name,
            year,
            theme_id,
            num_parts,
            image_url,
            created_utc,
            modified_utc
        FROM set_catalog
        WHERE id = :id
    )");

    query.bindValue(":id", id);

    if (!query.exec()) {
        qCritical() << "Unable to retrieve set catalog item:" << query.lastError().text();

        return std::nullopt;
    }

    if (!query.next())
        return std::nullopt;

    return setFromQuery(query);
}

std::optional<SetCatalogItem> SetCatalogRepository::getBySetNumber(const QString& setNumber) const
{
    const QString trimmed = setNumber.trimmed();

    if (trimmed.isEmpty())
        return std::nullopt;

    QSqlDatabase database = DatabaseManager::instance().database();

    QSqlQuery query(database);

    query.prepare(R"(
        SELECT
            id,
            set_number,
            name,
            year,
            theme_id,
            num_parts,
            image_url,
            created_utc,
            modified_utc
        FROM set_catalog
        WHERE set_number = :set_number
        LIMIT 1
    )");

    query.bindValue(":set_number", trimmed);

    if (!query.exec()) {
        qCritical() << "Unable to retrieve set by number:" << query.lastError().text();

        return std::nullopt;
    }

    if (!query.next())
        return std::nullopt;

    return setFromQuery(query);
}

QList<SetCatalogItem> SetCatalogRepository::search(const SetCatalogSearchCriteria& criteria) const
{
    QList<SetCatalogItem> results;

    QSqlDatabase database = DatabaseManager::instance().database();

    QString sql = R"(
        SELECT
            id,
            set_number,
            name,
            year,
            theme_id,
            num_parts,
            image_url,
            created_utc,
            modified_utc
        FROM set_catalog
        WHERE 1 = 1
    )";

    const QString searchText = criteria.searchText.trimmed();

    if (!searchText.isEmpty()) {
        sql += R"(
            AND
            (
                set_number LIKE :search
                OR name LIKE :search
            )
        )";
    }

    if (criteria.year > 0) {
        sql += R"(
            AND year = :year
        )";
    }

    if (criteria.themeId > 0) {
        sql += R"(
            AND theme_id = :theme_id
        )";
    }

    sql += R"(
        ORDER BY
            year DESC,
            set_number

        LIMIT :limit
        OFFSET :offset
    )";

    QSqlQuery query(database);

    if (!query.prepare(sql)) {
        qCritical() << "Unable to prepare set catalog search:" << query.lastError().text();

        return results;
    }

    if (!searchText.isEmpty()) {
        query.bindValue(":search", "%" + searchText + "%");
    }

    if (criteria.year > 0) {
        query.bindValue(":year", criteria.year);
    }

    if (criteria.themeId > 0) {
        query.bindValue(":theme_id", criteria.themeId);
    }

    query.bindValue(":limit", qBound(1, criteria.limit, 500));

    query.bindValue(":offset", qMax(0, criteria.offset));

    if (!query.exec()) {
        qCritical() << "Unable to search set catalog:" << query.lastError().text();

        return results;
    }

    while (query.next()) {
        results.append(setFromQuery(query));
    }

    return results;
}


int SetCatalogRepository::count(const SetCatalogSearchCriteria& criteria) const
{
    QSqlDatabase database = DatabaseManager::instance().database();

    QString sql = R"(
        SELECT COUNT(*)
        FROM set_catalog
        WHERE 1 = 1
    )";

    const QString searchText = criteria.searchText.trimmed();

    if (!searchText.isEmpty()) {
        sql += R"(
            AND
            (
                set_number LIKE :search
                OR name LIKE :search
            )
        )";
    }

    if (criteria.year > 0) {
        sql += R"(
            AND year = :year
        )";
    }

    if (criteria.themeId > 0) {
        sql += R"(
            AND theme_id = :theme_id
        )";
    }

    QSqlQuery query(database);

    if (!query.prepare(sql)) {
        qCritical() << "Unable to prepare set catalog count:" << query.lastError().text();
        return 0;
    }

    if (!searchText.isEmpty()) {
        query.bindValue(":search", "%" + searchText + "%");
    }

    if (criteria.year > 0) {
        query.bindValue(":year", criteria.year);
    }

    if (criteria.themeId > 0) {
        query.bindValue(":theme_id", criteria.themeId);
    }

    if (!query.exec()) {
        qCritical() << "Unable to count matching sets:" << query.lastError().text();
        return 0;
    }

    if (!query.next())
        return 0;

    return query.value(0).toInt();
}

QList<int> SetCatalogRepository::getYears() const
{
    QList<int> years;

    QSqlDatabase database = DatabaseManager::instance().database();

    QSqlQuery query(database);

    if (!query.exec(R"(
        SELECT DISTINCT year
        FROM set_catalog
        WHERE year > 0
        ORDER BY year DESC
    )")) {
        qCritical() << "Unable to retrieve set catalog years:" << query.lastError().text();

        return years;
    }

    while (query.next()) {
        years.append(query.value(0).toInt());
    }

    return years;
}

int SetCatalogRepository::count() const
{
    QSqlDatabase database = DatabaseManager::instance().database();

    QSqlQuery query(database);

    if (!query.exec("SELECT COUNT(*) FROM set_catalog")) {
        return 0;
    }

    if (!query.next())
        return 0;

    return query.value(0).toInt();
}

SetCatalogItem SetCatalogRepository::setFromQuery(const QSqlQuery& query) const
{
    SetCatalogItem item;

    item.setId(query.value("id").toInt());

    item.setSetNumber(query.value("set_number").toString());

    item.setName(query.value("name").toString());

    item.setYear(query.value("year").toInt());

    item.setThemeId(query.value("theme_id").toInt());

    item.setNumberOfParts(query.value("num_parts").toInt());

    item.setImageUrl(query.value("image_url").toString());

    item.setCreatedUtc(
        QDateTime::fromString(query.value("created_utc").toString(), Qt::ISODateWithMs));

    item.setModifiedUtc(
        QDateTime::fromString(query.value("modified_utc").toString(), Qt::ISODateWithMs));

    return item;
}