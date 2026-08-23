/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 */
#include "ManufacturerRepository.h"

#include "../database/DatabaseManager.h"

#include <QDateTime>
#include <QDebug>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

Manufacturer ManufacturerRepository::fromQuery(const QSqlQuery& query)
{
    Manufacturer manufacturer;
    manufacturer.setId(query.value("id").toInt());
    manufacturer.setCode(query.value("code").toString());
    manufacturer.setName(query.value("name").toString());
    manufacturer.setWebsiteUrl(query.value("website_url").toString());
    manufacturer.setSupportsLegoElementIds(
        query.value("supports_lego_element_ids").toInt() != 0);
    manufacturer.setIsActive(query.value("is_active").toInt() != 0);
    manufacturer.setNotes(query.value("notes").toString());
    manufacturer.setCreatedUtc(
        QDateTime::fromString(query.value("created_utc").toString(), Qt::ISODateWithMs));
    manufacturer.setModifiedUtc(
        QDateTime::fromString(query.value("modified_utc").toString(), Qt::ISODateWithMs));
    return manufacturer;
}

QList<Manufacturer> ManufacturerRepository::getAll(bool activeOnly) const
{
    QList<Manufacturer> values;
    QSqlQuery query(DatabaseManager::instance().database());

    QString sql = R"(
        SELECT id, code, name, website_url, supports_lego_element_ids,
               is_active, notes, created_utc, modified_utc
        FROM manufacturer
    )";

    if (activeOnly)
        sql += QStringLiteral(" WHERE is_active = 1");

    sql += QStringLiteral(" ORDER BY name COLLATE NOCASE");

    if (!query.exec(sql)) {
        qCritical() << "Unable to retrieve manufacturers:"
                    << query.lastError().text();
        return values;
    }

    while (query.next())
        values.append(fromQuery(query));

    return values;
}

std::optional<Manufacturer> ManufacturerRepository::getById(int id) const
{
    QSqlQuery query(DatabaseManager::instance().database());
    query.prepare(R"(
        SELECT id, code, name, website_url, supports_lego_element_ids,
               is_active, notes, created_utc, modified_utc
        FROM manufacturer
        WHERE id = :id
    )");
    query.bindValue(":id", id);

    if (!query.exec() || !query.next())
        return std::nullopt;

    return fromQuery(query);
}

std::optional<Manufacturer> ManufacturerRepository::getByCode(const QString& code) const
{
    QSqlQuery query(DatabaseManager::instance().database());
    query.prepare(R"(
        SELECT id, code, name, website_url, supports_lego_element_ids,
               is_active, notes, created_utc, modified_utc
        FROM manufacturer
        WHERE code = :code COLLATE NOCASE
    )");
    query.bindValue(":code", code.trimmed());

    if (!query.exec() || !query.next())
        return std::nullopt;

    return fromQuery(query);
}

std::optional<Manufacturer> ManufacturerRepository::getByName(const QString& name) const
{
    QSqlQuery query(DatabaseManager::instance().database());
    query.prepare(R"(
        SELECT id, code, name, website_url, supports_lego_element_ids,
               is_active, notes, created_utc, modified_utc
        FROM manufacturer
        WHERE name = :name COLLATE NOCASE
    )");
    query.bindValue(":name", name.trimmed());

    if (!query.exec() || !query.next())
        return std::nullopt;

    return fromQuery(query);
}

bool ManufacturerRepository::codeExists(
    const QString& code,
    int excludeManufacturerId) const
{
    const QString normalizedCode = code.trimmed().toUpper();

    if (normalizedCode.isEmpty())
        return false;

    QSqlQuery query(DatabaseManager::instance().database());

    QString sql = R"(
        SELECT 1
        FROM manufacturer
        WHERE code = :code COLLATE NOCASE
    )";

    if (excludeManufacturerId > 0)
        sql += QStringLiteral(" AND id <> :exclude_id");

    sql += QStringLiteral(" LIMIT 1");

    query.prepare(sql);
    query.bindValue(":code", normalizedCode);

    if (excludeManufacturerId > 0)
        query.bindValue(":exclude_id", excludeManufacturerId);

    if (!query.exec()) {
        qCritical() << "Unable to validate manufacturer code uniqueness:"
                    << query.lastError().text();
        return false;
    }

    return query.next();
}

bool ManufacturerRepository::nameExists(
    const QString& name,
    int excludeManufacturerId) const
{
    const QString normalizedName = name.trimmed();

    if (normalizedName.isEmpty())
        return false;

    QSqlQuery query(DatabaseManager::instance().database());

    QString sql = R"(
        SELECT 1
        FROM manufacturer
        WHERE name = :name COLLATE NOCASE
    )";

    if (excludeManufacturerId > 0)
        sql += QStringLiteral(" AND id <> :exclude_id");

    sql += QStringLiteral(" LIMIT 1");

    query.prepare(sql);
    query.bindValue(":name", normalizedName);

    if (excludeManufacturerId > 0)
        query.bindValue(":exclude_id", excludeManufacturerId);

    if (!query.exec()) {
        qCritical() << "Unable to validate manufacturer name uniqueness:"
                    << query.lastError().text();
        return false;
    }

    return query.next();
}

ManufacturerUsage ManufacturerRepository::usage(int manufacturerId) const
{
    ManufacturerUsage result;

    if (manufacturerId <= 0)
        return result;

    QSqlDatabase database = DatabaseManager::instance().database();

    auto countReferences =
        [&database, manufacturerId](const QString& sql) -> int
        {
            QSqlQuery query(database);
            query.prepare(sql);
            query.bindValue(":manufacturer_id", manufacturerId);

            if (!query.exec() || !query.next())
                return 0;

            return query.value(0).toInt();
        };

    result.inventoryRecordCount = countReferences(R"(
        SELECT COUNT(*)
        FROM inventory_record
        WHERE manufacturer_id = :manufacturer_id
    )");

    result.buildCount = countReferences(R"(
        SELECT COUNT(*)
        FROM build
        WHERE manufacturer_id = :manufacturer_id
    )");

    result.provenanceCount = countReferences(R"(
        SELECT COUNT(*)
        FROM build_part_provenance
        WHERE manufacturer_id = :manufacturer_id
          AND quantity_pulled > 0
    )");

    return result;
}

bool ManufacturerRepository::create(Manufacturer& manufacturer) const
{
    const QString code = manufacturer.code().trimmed().toUpper();
    const QString name = manufacturer.name().trimmed();

    if (code.isEmpty() || name.isEmpty())
        return false;

    if (codeExists(code) || nameExists(name)) {
        qWarning() << "Manufacturer create rejected due to duplicate identity."
                   << "Code:" << code
                   << "Name:" << name;
        return false;
    }

    const QString now =
        QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);

    QSqlQuery query(DatabaseManager::instance().database());
    query.prepare(R"(
        INSERT INTO manufacturer
        (
            code, name, website_url, supports_lego_element_ids,
            is_active, notes, created_utc, modified_utc
        )
        VALUES
        (
            :code, :name, :website_url, :supports_lego_element_ids,
            :is_active, :notes, :created_utc, :modified_utc
        )
    )");

    query.bindValue(":code", code);
    query.bindValue(":name", name);
    query.bindValue(":website_url",
                    manufacturer.websiteUrl().trimmed().isEmpty()
                        ? QVariant()
                        : QVariant(manufacturer.websiteUrl().trimmed()));
    query.bindValue(":supports_lego_element_ids",
                    manufacturer.supportsLegoElementIds() ? 1 : 0);
    query.bindValue(":is_active", manufacturer.isActive() ? 1 : 0);
    query.bindValue(":notes",
                    manufacturer.notes().trimmed().isEmpty()
                        ? QVariant()
                        : QVariant(manufacturer.notes().trimmed()));
    query.bindValue(":created_utc", now);
    query.bindValue(":modified_utc", now);

    if (!query.exec()) {
        qCritical() << "Unable to create manufacturer:"
                    << query.lastError().text();
        return false;
    }

    manufacturer.setId(query.lastInsertId().toInt());
    manufacturer.setCode(code);
    manufacturer.setName(name);
    manufacturer.setCreatedUtc(QDateTime::fromString(now, Qt::ISODateWithMs));
    manufacturer.setModifiedUtc(QDateTime::fromString(now, Qt::ISODateWithMs));
    return true;
}

bool ManufacturerRepository::update(Manufacturer& manufacturer) const
{
    if (manufacturer.id() <= 0
        || manufacturer.code().trimmed().isEmpty()
        || manufacturer.name().trimmed().isEmpty()) {
        return false;
    }

    const QString code = manufacturer.code().trimmed().toUpper();
    const QString name = manufacturer.name().trimmed();

    if (codeExists(code, manufacturer.id())
        || nameExists(name, manufacturer.id())) {
        qWarning() << "Manufacturer update rejected due to duplicate identity."
                   << "ManufacturerId:" << manufacturer.id()
                   << "Code:" << code
                   << "Name:" << name;
        return false;
    }

    const std::optional<Manufacturer> existing =
        getById(manufacturer.id());

    if (!existing)
        return false;

    if (existing->code().compare(QStringLiteral("LEGO"),
                                 Qt::CaseInsensitive) == 0) {
        if (code.compare(QStringLiteral("LEGO"),
                         Qt::CaseInsensitive) != 0
            || !manufacturer.isActive()) {
            qWarning() << "System LEGO manufacturer cannot be renamed "
                          "or deactivated.";
            return false;
        }
    }

    const QString now =
        QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);

    QSqlQuery query(DatabaseManager::instance().database());
    query.prepare(R"(
        UPDATE manufacturer
        SET code = :code,
            name = :name,
            website_url = :website_url,
            supports_lego_element_ids = :supports_lego_element_ids,
            is_active = :is_active,
            notes = :notes,
            modified_utc = :modified_utc
        WHERE id = :id
    )");

    query.bindValue(":code", code);
    query.bindValue(":name", name);
    query.bindValue(":website_url",
                    manufacturer.websiteUrl().trimmed().isEmpty()
                        ? QVariant()
                        : QVariant(manufacturer.websiteUrl().trimmed()));
    query.bindValue(":supports_lego_element_ids",
                    manufacturer.supportsLegoElementIds() ? 1 : 0);
    query.bindValue(":is_active", manufacturer.isActive() ? 1 : 0);
    query.bindValue(":notes",
                    manufacturer.notes().trimmed().isEmpty()
                        ? QVariant()
                        : QVariant(manufacturer.notes().trimmed()));
    query.bindValue(":modified_utc", now);
    query.bindValue(":id", manufacturer.id());

    if (!query.exec()) {
        qCritical() << "Unable to update manufacturer:"
                    << query.lastError().text();
        return false;
    }

    manufacturer.setCode(code);
    manufacturer.setName(name);
    manufacturer.setModifiedUtc(QDateTime::fromString(now, Qt::ISODateWithMs));
    return query.numRowsAffected() > 0;
}

bool ManufacturerRepository::setActive(int id, bool active) const
{
    if (id <= 0)
        return false;

    const std::optional<Manufacturer> manufacturer = getById(id);

    if (!manufacturer)
        return false;

    if (!active
        && manufacturer->code().compare(QStringLiteral("LEGO"),
                                        Qt::CaseInsensitive) == 0) {
        qWarning() << "System LEGO manufacturer cannot be deactivated.";
        return false;
    }

    QSqlQuery query(DatabaseManager::instance().database());
    query.prepare(R"(
        UPDATE manufacturer
        SET is_active = :is_active,
            modified_utc = :modified_utc
        WHERE id = :id
    )");
    query.bindValue(":is_active", active ? 1 : 0);
    query.bindValue(":modified_utc",
                    QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    query.bindValue(":id", id);

    if (!query.exec()) {
        qCritical() << "Unable to update manufacturer active state:"
                    << query.lastError().text();
        return false;
    }

    return query.numRowsAffected() > 0;
}

int ManufacturerRepository::legoManufacturerId() const
{
    const auto lego = getByCode(QStringLiteral("LEGO"));
    return lego ? lego->id() : 0;
}
