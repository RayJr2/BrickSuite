/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 */
#include "ExternalPartMappingRepository.h"

#include "../database/DatabaseManager.h"

#include <QDateTime>
#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace
{
ExternalPartMapping mappingFromQuery(const QSqlQuery& query)
{
    ExternalPartMapping mapping;
    mapping.id = query.value(QStringLiteral("id")).toInt();
    mapping.partId = query.value(QStringLiteral("part_id")).toInt();
    mapping.provider = query.value(QStringLiteral("provider")).toString();
    mapping.externalId = query.value(QStringLiteral("external_id")).toString();
    mapping.status =
        externalMappingStatusFromString(query.value(QStringLiteral("mapping_status")).toString());
    mapping.source = query.value(QStringLiteral("source")).toString();
    mapping.notes = query.value(QStringLiteral("notes")).toString();
    return mapping;
}
}

std::optional<ExternalPartMapping>
ExternalPartMappingRepository::getByPartAndProvider(
    int partId,
    const QString& provider) const
{
    QSqlQuery query(DatabaseManager::instance().database());

    query.prepare(R"(
        SELECT
            id,
            part_id,
            provider,
            external_id,
            mapping_status,
            source,
            notes
        FROM external_part_mapping
        WHERE part_id = :part_id
          AND provider = :provider
    )");

    query.bindValue(":part_id", partId);
    query.bindValue(":provider", provider);

    if (!query.exec()) {
        qCritical() << "Unable to retrieve external part mapping:"
                    << query.lastError().text();
        return std::nullopt;
    }

    if (!query.next())
        return std::nullopt;

    return mappingFromQuery(query);
}

QList<ExternalPartMapping>
ExternalPartMappingRepository::findByProviderAndExternalId(
    const QString& provider,
    const QString& externalId) const
{
    QList<ExternalPartMapping> results;

    const QString providerValue = provider.trimmed();
    const QString externalIdValue = externalId.trimmed();

    if (providerValue.isEmpty() || externalIdValue.isEmpty())
        return results;

    QSqlQuery query(DatabaseManager::instance().database());

    query.prepare(R"(
        SELECT
            id,
            part_id,
            provider,
            external_id,
            mapping_status,
            source,
            notes
        FROM external_part_mapping
        WHERE provider = :provider COLLATE NOCASE
          AND external_id = :external_id COLLATE NOCASE
          AND mapping_status = 'Mapped'
        ORDER BY part_id
    )");

    query.bindValue(":provider", providerValue);
    query.bindValue(":external_id", externalIdValue);

    if (!query.exec()) {
        qCritical() << "Unable to reverse-search external part mapping:"
                    << query.lastError().text();
        return results;
    }

    while (query.next())
        results.append(mappingFromQuery(query));

    return results;
}

bool ExternalPartMappingRepository::upsert(
    const ExternalPartMapping& mapping) const
{
    QSqlQuery query(DatabaseManager::instance().database());

    query.prepare(R"(
        INSERT INTO external_part_mapping
        (
            part_id,
            provider,
            external_id,
            mapping_status,
            source,
            notes,
            created_utc,
            modified_utc
        )
        VALUES
        (
            :part_id,
            :provider,
            :external_id,
            :mapping_status,
            :source,
            :notes,
            :created_utc,
            :modified_utc
        )
        ON CONFLICT(part_id, provider)
        DO UPDATE SET
            external_id = excluded.external_id,
            mapping_status = excluded.mapping_status,
            source = excluded.source,
            notes = excluded.notes,
            modified_utc = excluded.modified_utc
    )");

    const QString now = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);

    query.bindValue(":part_id", mapping.partId);
    query.bindValue(":provider", mapping.provider);
    query.bindValue(":external_id",
                    mapping.externalId.isEmpty() ? QVariant() : QVariant(mapping.externalId));
    query.bindValue(":mapping_status", externalMappingStatusToString(mapping.status));
    query.bindValue(":source", mapping.source);
    query.bindValue(":notes", mapping.notes);
    query.bindValue(":created_utc", now);
    query.bindValue(":modified_utc", now);

    if (!query.exec()) {
        qCritical() << "Unable to save external part mapping:"
                    << query.lastError().text();
        return false;
    }

    return true;
}

bool ExternalPartMappingRepository::remove(int partId,
                                           const QString& provider) const
{
    QSqlQuery query(DatabaseManager::instance().database());

    query.prepare(R"(
        DELETE FROM external_part_mapping
        WHERE part_id = :part_id
          AND provider = :provider
    )");

    query.bindValue(":part_id", partId);
    query.bindValue(":provider", provider);

    if (!query.exec()) {
        qCritical() << "Unable to remove external part mapping:"
                    << query.lastError().text();
        return false;
    }

    return true;
}

