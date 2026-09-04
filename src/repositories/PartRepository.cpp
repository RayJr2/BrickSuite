/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 *
 * This file is part of BrickSuite.
 *
 * BrickSuite is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, version 3 of the License.
 *
 * BrickSuite is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with BrickSuite. If not, see <https://www.gnu.org/licenses/>.
 */

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
        qWarning() << "Part create rejected due to missing number/name."
                   << "PartNumber:" << part.partNumber()
                   << "Name:" << part.name();
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

QList<Part> PartRepository::findActiveDecoratedByBasePrefix(
    const QString& basePartNumber,
    int limit) const
{
    QList<Part> parts;
    QString base = basePartNumber.trimmed();
    if (base.isEmpty() || limit <= 0)
        return parts;

    // LIKE metacharacters are valid identifier characters, so escape them
    // before constructing the two deliberately constrained prefix patterns.
    base.replace(QStringLiteral("\\"), QStringLiteral("\\\\"));
    base.replace(QStringLiteral("%"), QStringLiteral("\\%"));
    base.replace(QStringLiteral("_"), QStringLiteral("\\_"));

    QSqlQuery query(DatabaseManager::instance().database());
    query.prepare(R"(
        SELECT
            id, part_number, name, part_category_id, rebrickable_part_id,
            material, is_active, created_utc, modified_utc
        FROM part
        WHERE is_active = 1
          AND
          (
              part_number LIKE :print_prefix ESCAPE '\' COLLATE NOCASE
              OR part_number LIKE :pattern_prefix ESCAPE '\' COLLATE NOCASE
          )
        ORDER BY part_number COLLATE NOCASE
        LIMIT :limit
    )");
    query.bindValue(QStringLiteral(":print_prefix"), base + QStringLiteral("pr%"));
    query.bindValue(QStringLiteral(":pattern_prefix"), base + QStringLiteral("pat%"));
    query.bindValue(QStringLiteral(":limit"), limit);

    if (!query.exec()) {
        qCritical() << "Unable to retrieve bounded decorated Part candidates:"
                    << query.lastError().text();
        return parts;
    }

    while (query.next())
        parts.append(partFromQuery(query));

    return parts;
}

bool PartRepository::update(Part& part)
{
    if (part.id() <= 0 || part.partNumber().trimmed().isEmpty() || part.name().trimmed().isEmpty()) {
        qWarning() << "Part update rejected due to invalid identity/number/name."
                   << "PartId:" << part.id()
                   << "PartNumber:" << part.partNumber()
                   << "Name:" << part.name();
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

    if (query.numRowsAffected() <= 0) {
        qWarning() << "Part update affected no rows."
                   << "PartId:" << part.id()
                   << "PartNumber:" << part.partNumber();
        return false;
    }

    part.setModifiedUtc(now);

    return true;
}


QList<Part> PartRepository::getReferencePartsByCategoryIds(const QList<int>& categoryIds,
                                                           int limit) const
{
    QList<Part> parts;

    if (categoryIds.isEmpty() || limit <= 0)
        return parts;

    QSqlDatabase database = DatabaseManager::instance().database();
    QSqlQuery query(database);

    QStringList placeholders;
    placeholders.reserve(categoryIds.size());

    for (int i = 0; i < categoryIds.size(); ++i)
        placeholders.append(QString(":category_%1").arg(i));

    const QString sql = QString(R"(
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
          AND part_category_id IN (%1)
          AND LOWER(part_number) NOT LIKE '%%pr%%'
          AND LOWER(name) NOT LIKE '%%print%%'
        ORDER BY
            CASE
                WHEN part_number GLOB '[0-9]*' THEN 0
                ELSE 1
            END,
            LENGTH(part_number),
            part_number
        LIMIT :limit
    )").arg(placeholders.join(", "));

    query.prepare(sql);

    for (int i = 0; i < categoryIds.size(); ++i)
        query.bindValue(placeholders.at(i), categoryIds.at(i));

    query.bindValue(":limit", limit);

    if (!query.exec()) {
        qCritical() << "Unable to retrieve Part Reference category parts:"
                    << query.lastError().text();
        return parts;
    }

    while (query.next())
        parts.append(partFromQuery(query));

    return parts;
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

    const QString searchText = criteria.searchText.trimmed();

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
    )";

    if (!searchText.isEmpty()) {
        sql += R"(,
            (
                SELECT pa.alias_part_number
                FROM part_alias pa
                WHERE pa.part_id = p.id
                  AND pa.is_active = 1
                  AND pa.alias_part_number LIKE :search
                ORDER BY
                    CASE
                        WHEN pa.alias_part_number = :exact COLLATE NOCASE THEN 0
                        WHEN pa.alias_part_number LIKE :prefix THEN 1
                        ELSE 2
                    END,
                    pa.alias_part_number COLLATE NOCASE
                LIMIT 1
            ) AS matched_alias_part_number,

            (
                SELECT pa.alias_type
                FROM part_alias pa
                WHERE pa.part_id = p.id
                  AND pa.is_active = 1
                  AND pa.alias_part_number LIKE :search
                ORDER BY
                    CASE
                        WHEN pa.alias_part_number = :exact COLLATE NOCASE THEN 0
                        WHEN pa.alias_part_number LIKE :prefix THEN 1
                        ELSE 2
                    END,
                    pa.alias_part_number COLLATE NOCASE
                LIMIT 1
            ) AS matched_alias_type,

            (
                SELECT pa.source
                FROM part_alias pa
                WHERE pa.part_id = p.id
                  AND pa.is_active = 1
                  AND pa.alias_part_number LIKE :search
                ORDER BY
                    CASE
                        WHEN pa.alias_part_number = :exact COLLATE NOCASE THEN 0
                        WHEN pa.alias_part_number LIKE :prefix THEN 1
                        ELSE 2
                    END,
                    pa.alias_part_number COLLATE NOCASE
                LIMIT 1
            ) AS matched_alias_source
        )";
    }

    sql += R"(
        FROM part p

        LEFT JOIN part_category pc
            ON pc.id = p.part_category_id

        WHERE p.is_active = 1
    )";

    if (!searchText.isEmpty()) {
        sql += R"(
            AND
            (
                p.part_number LIKE :search
                OR p.name LIKE :search
                OR EXISTS
                (
                    SELECT 1
                    FROM part_alias pa_search
                    WHERE pa_search.part_id = p.id
                      AND pa_search.is_active = 1
                      AND pa_search.alias_part_number LIKE :search
                )
            )
        )";
    }

    if (criteria.categoryId > 0) {
        sql += R"(
            AND p.part_category_id = :category_id
        )";
    }

    if (!searchText.isEmpty()) {
        sql += R"(
            ORDER BY
                CASE
                    WHEN p.part_number = :exact COLLATE NOCASE THEN 0
                    WHEN EXISTS
                    (
                        SELECT 1
                        FROM part_alias pa_exact
                        WHERE pa_exact.part_id = p.id
                          AND pa_exact.is_active = 1
                          AND pa_exact.alias_part_number = :exact COLLATE NOCASE
                    ) THEN 1
                    WHEN p.part_number LIKE :prefix THEN 2
                    WHEN EXISTS
                    (
                        SELECT 1
                        FROM part_alias pa_prefix
                        WHERE pa_prefix.part_id = p.id
                          AND pa_prefix.is_active = 1
                          AND pa_prefix.alias_part_number LIKE :prefix
                    ) THEN 3
                    ELSE 4
                END,
                p.part_number
        )";
    } else {
        sql += R"(
            ORDER BY p.part_number
        )";
    }

    sql += R"(
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
        query.bindValue(":exact", searchText);
        query.bindValue(":prefix", searchText + "%");
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

        if (!searchText.isEmpty()) {
            result.matchedAliasPartNumber =
                query.value("matched_alias_part_number").toString();
            result.matchedAliasType =
                query.value("matched_alias_type").toString();
            result.matchedAliasSource =
                query.value("matched_alias_source").toString();
        }

        results.append(result);
    }

    return results;
}


int PartRepository::count(const PartSearchCriteria& criteria) const
{
    QSqlDatabase database = DatabaseManager::instance().database();

    QString sql = R"(
        SELECT COUNT(*)
        FROM part p
        WHERE p.is_active = 1
    )";

    const QString searchText = criteria.searchText.trimmed();

    if (!searchText.isEmpty()) {
        sql += R"(
            AND
            (
                p.part_number LIKE :search
                OR p.name LIKE :search
                OR EXISTS
                (
                    SELECT 1
                    FROM part_alias pa
                    WHERE pa.part_id = p.id
                      AND pa.is_active = 1
                      AND pa.alias_part_number LIKE :search
                )
            )
        )";
    }

    if (criteria.categoryId > 0) {
        sql += R"(
            AND p.part_category_id = :category_id
        )";
    }

    QSqlQuery query(database);

    if (!query.prepare(sql)) {
        qCritical() << "Unable to prepare part count:" << query.lastError().text();
        return 0;
    }

    if (!searchText.isEmpty()) {
        query.bindValue(":search", "%" + searchText + "%");
    }

    if (criteria.categoryId > 0) {
        query.bindValue(":category_id", criteria.categoryId);
    }

    if (!query.exec()) {
        qCritical() << "Unable to count matching parts:" << query.lastError().text();
        return 0;
    }

    if (!query.next())
        return 0;

    return query.value(0).toInt();
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
