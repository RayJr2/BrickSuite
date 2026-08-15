#include "PartRepository.h"

#include "../database/DatabaseManager.h"

#include <QDateTime>
#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>
#include <QtGlobal>

bool PartRepository::create(Part& part)
{
    if (part.partNumber().trimmed().isEmpty() || part.name().trimmed().isEmpty()) {
        return false;
    }

    QSqlDatabase database = DatabaseManager::instance().database();

    const QDateTime now = QDateTime::currentDateTimeUtc();

    QSqlQuery query(database);

    query.prepare(R"(
    INSERT INTO part
    (
        part_number,
        name,
        part_category_id,
        rebrickable_part_id,
        material,
        is_active,
        created_utc,
        modified_utc
    )
    VALUES
    (
        :part_number,
        :name,
        :part_category_id,
        :rebrickable_part_id,
        :material,
        :is_active,
        :created_utc,
        :modified_utc
    )
)");

    query.bindValue(":part_number", part.partNumber().trimmed());

    query.bindValue(":name", part.name().trimmed());

    if (part.partCategoryId() > 0)
        query.bindValue(":part_category_id", part.partCategoryId());
    else
        query.bindValue(":part_category_id", QVariant());

    if (!part.rebrickablePartId().trimmed().isEmpty())
        query.bindValue(":rebrickable_part_id", part.rebrickablePartId().trimmed());
    else
        query.bindValue(":rebrickable_part_id", QVariant());

    query.bindValue(":material", part.material().trimmed());

    query.bindValue(":is_active", part.isActive() ? 1 : 0);

    query.bindValue(":created_utc", now.toString(Qt::ISODateWithMs));

    query.bindValue(":modified_utc", now.toString(Qt::ISODateWithMs));

    if (!query.exec()) {
        qCritical() << "Unable to create part:" << query.lastError().text();

        return false;
    }

    part.setId(query.lastInsertId().toInt());

    part.setCreatedUtc(now);
    part.setModifiedUtc(now);

    return true;
}

QList<Part> PartRepository::getAll() const
{
    QList<Part> parts;

    QSqlDatabase database = DatabaseManager::instance().database();

    QSqlQuery query(database);

    if (!query.exec(R"(
        SELECT
            id,
            part_number,
            name,
            part_category_id,
            rebrickable_part_id,
            material,
            is_active,
            created_utc,
            modified_utc
        FROM part
        WHERE is_active = 1
        ORDER BY part_number
    )")) {
        qCritical() << "Unable to retrieve parts:" << query.lastError().text();

        return parts;
    }

    while (query.next())
        parts.append(partFromQuery(query));

    return parts;
}

std::optional<Part> PartRepository::getById(int id) const
{
    QSqlDatabase database = DatabaseManager::instance().database();

    QSqlQuery query(database);

    query.prepare(R"(
        SELECT
            id,
            part_number,
            name,
            part_category_id,
            rebrickable_part_id,
            material,
            is_active,
            created_utc,
            modified_utc
        FROM part
        WHERE id = :id
    )");

    query.bindValue(":id", id);

    if (!query.exec()) {
        qCritical() << "Unable to retrieve part:" << query.lastError().text();

        return std::nullopt;
    }

    if (!query.next())
        return std::nullopt;

    return partFromQuery(query);
}

std::optional<Part> PartRepository::getByPartNumber(const QString& partNumber) const
{
    QSqlDatabase database = DatabaseManager::instance().database();

    QSqlQuery query(database);

    query.prepare(R"(
        SELECT
            id,
            part_number,
            name,
            part_category_id,
            rebrickable_part_id,
            material,
            is_active,
            created_utc,
            modified_utc
        FROM part
        WHERE part_number = :part_number
    )");

    query.bindValue(":part_number", partNumber.trimmed());

    if (!query.exec()) {
        qCritical() << "Unable to retrieve part by number:" << query.lastError().text();

        return std::nullopt;
    }

    if (!query.next())
        return std::nullopt;

    return partFromQuery(query);
}

bool PartRepository::update(Part& part)
{
    if (part.id() <= 0 || part.partNumber().trimmed().isEmpty() || part.name().trimmed().isEmpty()) {
        return false;
    }

    QSqlDatabase database = DatabaseManager::instance().database();

    const QDateTime now = QDateTime::currentDateTimeUtc();

    QSqlQuery query(database);

    query.prepare(R"(
        UPDATE part
        SET
            part_number = :part_number,
            name = :name,
            part_category_id = :part_category_id,
            rebrickable_part_id = :rebrickable_part_id,
            material = :material,
            is_active = :is_active,
            modified_utc = :modified_utc
        WHERE id = :id
    )");

    query.bindValue(":part_number", part.partNumber().trimmed());
    query.bindValue(":name", part.name().trimmed());

    if (part.partCategoryId() > 0)
        query.bindValue(":part_category_id", part.partCategoryId());
    else
        query.bindValue(":part_category_id", QVariant());

    if (!part.rebrickablePartId().trimmed().isEmpty())
        query.bindValue(":rebrickable_part_id", part.rebrickablePartId().trimmed());
    else
        query.bindValue(":rebrickable_part_id", QVariant());

    query.bindValue(":material", part.material().trimmed());
    query.bindValue(":is_active", part.isActive() ? 1 : 0);
    query.bindValue(":modified_utc", now.toString(Qt::ISODateWithMs));
    query.bindValue(":id", part.id());

    if (!query.exec()) {
        qCritical() << "Unable to update part:" << query.lastError().text();

        return false;
    }

    if (query.numRowsAffected() <= 0)
        return false;

    part.setModifiedUtc(now);

    return true;
}

Part PartRepository::partFromQuery(const QSqlQuery& query) const
{
    Part part;

    part.setId(query.value("id").toInt());

    part.setPartNumber(query.value("part_number").toString());

    part.setName(query.value("name").toString());

    if (!query.value("part_category_id").isNull())
        part.setPartCategoryId(query.value("part_category_id").toInt());

    part.setRebrickablePartId(query.value("rebrickable_part_id").toString());

    part.setMaterial(query.value("material").toString());

    part.setIsActive(query.value("is_active").toInt() != 0);

    part.setCreatedUtc(
        QDateTime::fromString(query.value("created_utc").toString(), Qt::ISODateWithMs));

    part.setModifiedUtc(
        QDateTime::fromString(query.value("modified_utc").toString(), Qt::ISODateWithMs));

    return part;
}

QList<PartSearchResult> PartRepository::search(const PartSearchCriteria& criteria) const
{
    QList<PartSearchResult> results;

    QSqlDatabase database = DatabaseManager::instance().database();

    QString sql = R"(
        SELECT
            p.id,
            p.part_number,
            p.name,
            p.part_category_id,
            p.rebrickable_part_id,
            p.material,
            p.is_active,
            p.created_utc,
            p.modified_utc,

            pc.name AS category_name

        FROM part p

        LEFT JOIN part_category pc
            ON pc.id = p.part_category_id

        WHERE p.is_active = 1
    )";

    const QString searchText = criteria.searchText.trimmed();

    if (!searchText.isEmpty()) {
        sql += R"(
            AND
            (
                p.part_number LIKE :search
                OR p.name LIKE :search
            )
        )";
    }

    if (criteria.categoryId > 0) {
        sql += R"(
            AND p.part_category_id = :category_id
        )";
    }

    sql += R"(
        ORDER BY
            p.part_number

        LIMIT :limit
        OFFSET :offset
    )";

    QSqlQuery query(database);

    if (!query.prepare(sql)) {
        qCritical() << "Unable to prepare part search:" << query.lastError().text();

        return results;
    }

    if (!searchText.isEmpty()) {
        query.bindValue(":search", "%" + searchText + "%");
    }

    if (criteria.categoryId > 0) {
        query.bindValue(":category_id", criteria.categoryId);
    }

    const int safeLimit = qBound(1, criteria.limit, 500);

    const int safeOffset = qMax(0, criteria.offset);

    query.bindValue(":limit", safeLimit);

    query.bindValue(":offset", safeOffset);

    if (!query.exec()) {
        qCritical() << "Unable to search parts:" << query.lastError().text();

        return results;
    }

    while (query.next()) {
        PartSearchResult result;

        result.part = partFromQuery(query);

        result.categoryName = query.value("category_name").toString();

        results.append(result);
    }

    return results;
}

QList<Part> PartRepository::searchForInventoryEntry(const QString& searchText, int limit) const
{
    QList<Part> parts;

    const QString trimmed = searchText.trimmed();

    if (trimmed.isEmpty())
        return parts;

    QSqlDatabase database = DatabaseManager::instance().database();

    QSqlQuery query(database);

    query.prepare(R"(
        SELECT
            id,
            part_number,
            name,
            part_category_id,
            rebrickable_part_id,
            material,
            is_active,
            created_utc,
            modified_utc
        FROM part
        WHERE is_active = 1
          AND
          (
              part_number LIKE :search
              OR name LIKE :search
          )
        ORDER BY
            CASE
                WHEN part_number = :exact THEN 0
                WHEN part_number LIKE :prefix THEN 1
                ELSE 2
            END,
            part_number
        LIMIT :limit
    )");

    query.bindValue(":search", "%" + trimmed + "%");

    query.bindValue(":exact", trimmed);

    query.bindValue(":prefix", trimmed + "%");

    query.bindValue(":limit", qBound(1, limit, 50));

    if (!query.exec()) {
        qCritical() << "Unable to search parts for inventory entry:" << query.lastError().text();

        return parts;
    }

    while (query.next()) {
        parts.append(partFromQuery(query));
    }

    return parts;
}