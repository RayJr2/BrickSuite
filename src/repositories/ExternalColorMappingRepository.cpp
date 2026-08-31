/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 */
#include "ExternalColorMappingRepository.h"

#include "../database/DatabaseManager.h"

#include <QDateTime>
#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace
{
ExternalColorMapping mappingFromQuery(const QSqlQuery& query)
{
    ExternalColorMapping mapping;
    mapping.id = query.value(QStringLiteral("id")).toInt();
    mapping.colorId = query.value(QStringLiteral("color_id")).toInt();
    mapping.provider = query.value(QStringLiteral("provider")).toString();
    mapping.externalId = query.value(QStringLiteral("external_id")).toString();
    mapping.status =
        externalMappingStatusFromString(query.value(QStringLiteral("mapping_status")).toString());
    mapping.source = query.value(QStringLiteral("source")).toString();
    mapping.notes = query.value(QStringLiteral("notes")).toString();
    return mapping;
}
}

std::optional<ExternalColorMapping>
ExternalColorMappingRepository::getByColorAndProvider(
    int colorId,
    const QString& provider) const
{
    QSqlQuery query(DatabaseManager::instance().database());

    query.prepare(R"(
        SELECT
            id,
            color_id,
            provider,
            external_id,
            mapping_status,
            source,
            notes
        FROM external_color_mapping
        WHERE color_id = :color_id
          AND provider = :provider
    )");

    query.bindValue(":color_id", colorId);
    query.bindValue(":provider", provider);

    if (!query.exec()) {
        qCritical() << "Unable to retrieve external color mapping:"
                    << query.lastError().text();
        return std::nullopt;
    }

    if (!query.next())
        return std::nullopt;

    return mappingFromQuery(query);
}

QList<ExternalColorMapping>
ExternalColorMappingRepository::getByProvider(const QString& provider) const
{
    QList<ExternalColorMapping> mappings;

    QSqlQuery query(DatabaseManager::instance().database());

    query.prepare(R"(
        SELECT
            id,
            color_id,
            provider,
            external_id,
            mapping_status,
            source,
            notes
        FROM external_color_mapping
        WHERE provider = :provider
        ORDER BY color_id
    )");

    query.bindValue(":provider", provider);

    if (!query.exec()) {
        qCritical() << "Unable to retrieve provider color mappings:"
                    << query.lastError().text();
        return mappings;
    }

    while (query.next())
        mappings.append(mappingFromQuery(query));

    return mappings;
}


QList<ExternalColorMapping>
ExternalColorMappingRepository::findByProviderAndExternalId(
    const QString& provider,
    const QString& externalId) const
{
    QList<ExternalColorMapping> mappings;

    const QString providerValue = provider.trimmed();
    const QString externalIdValue = externalId.trimmed();

    if (providerValue.isEmpty() || externalIdValue.isEmpty())
        return mappings;

    QSqlQuery query(DatabaseManager::instance().database());

    query.prepare(R"(
        SELECT
            id,
            color_id,
            provider,
            external_id,
            mapping_status,
            source,
            notes
        FROM external_color_mapping
        WHERE provider = :provider COLLATE NOCASE
          AND external_id = :external_id COLLATE NOCASE
          AND mapping_status = 'Mapped'
        ORDER BY color_id
    )");

    query.bindValue(":provider", providerValue);
    query.bindValue(":external_id", externalIdValue);

    if (!query.exec()) {
        qCritical() << "Unable to reverse-resolve external color mapping:"
                    << query.lastError().text();
        return mappings;
    }

    while (query.next())
        mappings.append(mappingFromQuery(query));

    return mappings;
}

int ExternalColorMappingRepository::countByProviderAndStatus(
    const QString& provider,
    ExternalMappingStatus status) const
{
    QSqlQuery query(DatabaseManager::instance().database());

    query.prepare(R"(
        SELECT COUNT(*)
        FROM external_color_mapping
        WHERE provider = :provider
          AND mapping_status = :mapping_status
    )");

    query.bindValue(":provider", provider);
    query.bindValue(":mapping_status", externalMappingStatusToString(status));

    if (!query.exec() || !query.next()) {
        qCritical() << "Unable to count external color mappings:"
                    << query.lastError().text();
        return 0;
    }

    return query.value(0).toInt();
}

bool ExternalColorMappingRepository::upsert(
    const ExternalColorMapping& mapping) const
{
    QSqlQuery query(DatabaseManager::instance().database());

    query.prepare(R"(
        INSERT INTO external_color_mapping
        (
            color_id,
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
            :color_id,
            :provider,
            :external_id,
            :mapping_status,
            :source,
            :notes,
            :created_utc,
            :modified_utc
        )
        ON CONFLICT(color_id, provider)
        DO UPDATE SET
            external_id = excluded.external_id,
            mapping_status = excluded.mapping_status,
            source = excluded.source,
            notes = excluded.notes,
            modified_utc = excluded.modified_utc
    )");

    const QString now = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);

    query.bindValue(":color_id", mapping.colorId);
    query.bindValue(":provider", mapping.provider);
    query.bindValue(":external_id",
                    mapping.externalId.isEmpty() ? QVariant() : QVariant(mapping.externalId));
    query.bindValue(":mapping_status", externalMappingStatusToString(mapping.status));
    query.bindValue(":source", mapping.source);
    query.bindValue(":notes", mapping.notes);
    query.bindValue(":created_utc", now);
    query.bindValue(":modified_utc", now);

    if (!query.exec()) {
        qCritical() << "Unable to save external color mapping:"
                    << query.lastError().text();
        return false;
    }

    return true;
}
