/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 */
#include "PartAliasRepository.h"

#include "../database/DatabaseManager.h"

#include <QDateTime>
#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>

namespace
{
PartAlias aliasFromQuery(const QSqlQuery& query)
{
    PartAlias alias;

    alias.id = query.value(QStringLiteral("id")).toInt();
    alias.partId = query.value(QStringLiteral("part_id")).toInt();
    alias.aliasPartNumber =
        query.value(QStringLiteral("alias_part_number")).toString();
    alias.aliasType =
        partAliasTypeFromString(
            query.value(QStringLiteral("alias_type")).toString());
    alias.source = query.value(QStringLiteral("source")).toString();
    alias.isActive = query.value(QStringLiteral("is_active")).toInt() != 0;
    alias.notes = query.value(QStringLiteral("notes")).toString();
    alias.createdUtc = query.value(QStringLiteral("created_utc")).toString();
    alias.modifiedUtc = query.value(QStringLiteral("modified_utc")).toString();

    return alias;
}

QString selectColumns()
{
    return QStringLiteral(
        "id, part_id, alias_part_number, alias_type, source, "
        "is_active, notes, created_utc, modified_utc");
}
}

std::optional<PartAlias>
PartAliasRepository::getByAliasPartNumber(
    const QString& aliasPartNumber,
    bool activeOnly) const
{
    const QString normalizedAlias = aliasPartNumber.trimmed();

    if (normalizedAlias.isEmpty())
        return std::nullopt;

    QSqlQuery query(DatabaseManager::instance().database());

    QString sql =
        QStringLiteral("SELECT %1 FROM part_alias "
                       "WHERE alias_part_number = :alias COLLATE NOCASE")
            .arg(selectColumns());

    if (activeOnly)
        sql += QStringLiteral(" AND is_active = 1");

    sql += QStringLiteral(" LIMIT 1");

    query.prepare(sql);
    query.bindValue(":alias", normalizedAlias);

    if (!query.exec()) {
        qCritical() << "Unable to retrieve part alias:"
                    << query.lastError().text();
        return std::nullopt;
    }

    if (!query.next())
        return std::nullopt;

    return aliasFromQuery(query);
}

QList<PartAlias>
PartAliasRepository::getByPartId(
    int partId,
    bool activeOnly) const
{
    QList<PartAlias> aliases;

    QSqlQuery query(DatabaseManager::instance().database());

    QString sql =
        QStringLiteral("SELECT %1 FROM part_alias WHERE part_id = :part_id")
            .arg(selectColumns());

    if (activeOnly)
        sql += QStringLiteral(" AND is_active = 1");

    sql += QStringLiteral(" ORDER BY alias_part_number COLLATE NOCASE");

    query.prepare(sql);
    query.bindValue(":part_id", partId);

    if (!query.exec()) {
        qCritical() << "Unable to retrieve aliases for part:"
                    << query.lastError().text();
        return aliases;
    }

    while (query.next())
        aliases.append(aliasFromQuery(query));

    return aliases;
}

bool PartAliasRepository::upsert(const PartAlias& alias) const
{
    const QString normalizedAlias = alias.aliasPartNumber.trimmed();
    const QString normalizedSource = alias.source.trimmed();

    if (alias.partId <= 0
        || normalizedAlias.isEmpty()
        || normalizedSource.isEmpty()) {
        return false;
    }

    //
    // Provider-derived data must never replace an explicit user-confirmed
    // alias. Preserve the user's decision at the repository boundary.
    //
    const std::optional<PartAlias> existing =
        getByAliasPartNumber(normalizedAlias, false);

    if (existing
        && existing->source.compare(QStringLiteral("User"),
                                    Qt::CaseInsensitive) == 0
        && normalizedSource.compare(QStringLiteral("User"),
                                    Qt::CaseInsensitive) != 0) {
        return true;
    }

    QSqlQuery query(DatabaseManager::instance().database());

    query.prepare(R"(
        INSERT INTO part_alias
        (
            part_id,
            alias_part_number,
            alias_type,
            source,
            is_active,
            notes,
            created_utc,
            modified_utc
        )
        VALUES
        (
            :part_id,
            :alias_part_number,
            :alias_type,
            :source,
            :is_active,
            :notes,
            :created_utc,
            :modified_utc
        )
        ON CONFLICT(alias_part_number)
        DO UPDATE SET
            part_id = excluded.part_id,
            alias_type = excluded.alias_type,
            source = excluded.source,
            is_active = excluded.is_active,
            notes = excluded.notes,
            modified_utc = excluded.modified_utc
    )");

    const QString now =
        QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);

    query.bindValue(":part_id", alias.partId);
    query.bindValue(":alias_part_number", normalizedAlias);
    query.bindValue(":alias_type", partAliasTypeToString(alias.aliasType));
    query.bindValue(":source", normalizedSource);
    query.bindValue(":is_active", alias.isActive ? 1 : 0);
    query.bindValue(":notes", alias.notes);
    query.bindValue(":created_utc", now);
    query.bindValue(":modified_utc", now);

    if (!query.exec()) {
        qCritical() << "Unable to save part alias:"
                    << query.lastError().text();
        return false;
    }

    return true;
}

bool PartAliasRepository::setActive(
    int aliasId,
    bool active) const
{
    QSqlQuery query(DatabaseManager::instance().database());

    query.prepare(R"(
        UPDATE part_alias
        SET is_active = :is_active,
            modified_utc = :modified_utc
        WHERE id = :id
    )");

    query.bindValue(":is_active", active ? 1 : 0);
    query.bindValue(":modified_utc",
                    QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    query.bindValue(":id", aliasId);

    if (!query.exec()) {
        qCritical() << "Unable to update part alias active state:"
                    << query.lastError().text();
        return false;
    }

    return query.numRowsAffected() > 0;
}
