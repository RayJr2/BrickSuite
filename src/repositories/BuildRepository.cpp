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

#include "BuildRepository.h"
#include "ManufacturerRepository.h"

#include "../database/DatabaseManager.h"

#include <QDateTime>
#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace
{
int normalizedManufacturerId(int manufacturerId)
{
    if (manufacturerId > 0)
        return manufacturerId;

    ManufacturerRepository repository;
    return repository.legoManufacturerId();
}
}

bool BuildRepository::create(Build& build)
{
    if (build.workspaceId() <= 0 || build.name().trimmed().isEmpty()) {
        qWarning() << "Build create rejected due to invalid workspace/name."
                   << "WorkspaceId:" << build.workspaceId()
                   << "Name:" << build.name();
        return false;
    }

    const QString buildType = build.buildType().trimmed();

    const QString inventoryMode = build.inventoryMode().trimmed();

    if (inventoryMode != "Stock" && inventoryMode != "CompleteSet") {
        qWarning() << "Build create rejected due to invalid inventory mode:"
                   << inventoryMode;
        return false;
    }

    if (buildType != "Set" && buildType != "MOC") {
        qWarning() << "Build create rejected due to invalid build type:"
                   << buildType;
        return false;
    }

    const QString status = build.status().trimmed();

    if (status != "Planned" && status != "Pulling" && status != "Complete"
        && status != "Disassembled" && status != "Cancelled") {
        qWarning() << "Invalid Build status:" << status;
        return false;
    }

    const int manufacturerId = normalizedManufacturerId(build.manufacturerId());

    if (manufacturerId <= 0) {
        qCritical() << "Build create failed: default LEGO manufacturer unavailable.";
        return false;
    }

    build.setManufacturerId(manufacturerId);

    QSqlDatabase database = DatabaseManager::instance().database();

    const QDateTime now = QDateTime::currentDateTimeUtc();

    QSqlQuery query(database);

    query.prepare(R"(
        INSERT INTO build
        (
            workspace_id,
            build_type,
            name,
            set_number,
            inventory_mode,
            manufacturer_id,
            status,
            is_active,
            notes,
            created_utc,
            modified_utc
        )
        VALUES
        (
            :workspace_id,
            :build_type,
            :name,
            :set_number,
            :inventory_mode,
            :manufacturer_id,
            :status,
            :is_active,
            :notes,
            :created_utc,
            :modified_utc
        )
    )");

    query.bindValue(":workspace_id", build.workspaceId());

    query.bindValue(":build_type", buildType);

    query.bindValue(":name", build.name().trimmed());

    if (!build.setNumber().trimmed().isEmpty()) {
        query.bindValue(":set_number", build.setNumber().trimmed());
    } else {
        query.bindValue(":set_number", QVariant());
    }

    query.bindValue(":inventory_mode", inventoryMode);

    query.bindValue(":manufacturer_id", manufacturerId);

    query.bindValue(":status", status);

    query.bindValue(":is_active", build.isActive() ? 1 : 0);

    if (!build.notes().trimmed().isEmpty()) {
        query.bindValue(":notes", build.notes().trimmed());
    } else {
        query.bindValue(":notes", QVariant());
    }

    query.bindValue(":created_utc", now.toString(Qt::ISODateWithMs));

    query.bindValue(":modified_utc", now.toString(Qt::ISODateWithMs));

    if (!query.exec()) {
        qCritical() << "Unable to create build:" << query.lastError().text();

        return false;
    }

    build.setId(query.lastInsertId().toInt());

    build.setCreatedUtc(now);
    build.setModifiedUtc(now);

    return true;
}

std::optional<Build> BuildRepository::getById(int id) const
{
    if (id <= 0)
        return std::nullopt;

    QSqlDatabase database = DatabaseManager::instance().database();

    QSqlQuery query(database);

    query.prepare(R"(
        SELECT
            id,
            workspace_id,
            build_type,
            name,
            set_number,
            inventory_mode,
            manufacturer_id,
            status,
            is_active,
            notes,
            created_utc,
            modified_utc
        FROM build
        WHERE id = :id
    )");

    query.bindValue(":id", id);

    if (!query.exec()) {
        qCritical() << "Unable to retrieve build:" << query.lastError().text();

        return std::nullopt;
    }

    if (!query.next())
        return std::nullopt;

    return buildFromQuery(query);
}

QList<Build> BuildRepository::getByWorkspace(int workspaceId, bool includeArchived) const
{
    QList<Build> builds;

    if (workspaceId <= 0)
        return builds;

    QSqlDatabase database = DatabaseManager::instance().database();

    QString sql = R"(
        SELECT
            id,
            workspace_id,
            build_type,
            name,
            set_number,
            inventory_mode,
            manufacturer_id,
            status,
            is_active,
            notes,
            created_utc,
            modified_utc
        FROM build
        WHERE workspace_id = :workspace_id
    )";

    if (!includeArchived) {
        sql += " AND is_active = 1";
    }

    sql += " ORDER BY is_active DESC, status, name";

    QSqlQuery query(database);

    query.prepare(sql);
    query.bindValue(":workspace_id", workspaceId);

    if (!query.exec()) {
        qCritical() << "Unable to retrieve builds:" << query.lastError().text();
        return builds;
    }

    while (query.next()) {
        builds.append(buildFromQuery(query));
    }

    return builds;
}

bool BuildRepository::setActive(int buildId, bool active)
{
    if (buildId <= 0) {
        qWarning() << "Build active-state update rejected."
                   << "BuildId:" << buildId
                   << "Active:" << active;
        return false;
    }

    QSqlDatabase database = DatabaseManager::instance().database();
    const QDateTime now = QDateTime::currentDateTimeUtc();

    QSqlQuery query(database);

    query.prepare(R"(
        UPDATE build
        SET
            is_active = :is_active,
            modified_utc = :modified_utc
        WHERE id = :id
    )");

    query.bindValue(":is_active", active ? 1 : 0);
    query.bindValue(":modified_utc", now.toString(Qt::ISODateWithMs));
    query.bindValue(":id", buildId);

    if (!query.exec()) {
        qCritical() << "Unable to update Build active state:" << query.lastError().text();
        return false;
    }

    if (query.numRowsAffected() <= 0) {
        qWarning() << "Build active-state update affected no rows."
                   << "BuildId:" << buildId
                   << "Active:" << active;
        return false;
    }

    return true;
}

bool BuildRepository::update(Build& build)
{
    if (build.id() <= 0 || build.workspaceId() <= 0 || build.name().trimmed().isEmpty()) {
        qWarning() << "Build update rejected due to invalid identity/workspace/name."
                   << "BuildId:" << build.id()
                   << "WorkspaceId:" << build.workspaceId()
                   << "Name:" << build.name();
        return false;
    }

    const QString buildType = build.buildType().trimmed();

    if (buildType != "Set" && buildType != "MOC") {
        qWarning() << "Build update rejected due to invalid build type:"
                   << buildType
                   << "BuildId:" << build.id();
        return false;
    }

    const QString inventoryMode = build.inventoryMode().trimmed();

    if (inventoryMode != "Stock" && inventoryMode != "CompleteSet") {
        qWarning() << "Build update rejected due to invalid inventory mode:"
                   << inventoryMode
                   << "BuildId:" << build.id();
        return false;
    }

    const QString status = build.status().trimmed();

    if (status != "Planned" && status != "Pulling" && status != "Complete"
        && status != "Disassembled" && status != "Cancelled") {
        qWarning() << "Invalid Build status:" << status;
        return false;
    }

    const int manufacturerId = normalizedManufacturerId(build.manufacturerId());

    if (manufacturerId <= 0) {
        qCritical() << "Build update failed: default LEGO manufacturer unavailable.";
        return false;
    }

    build.setManufacturerId(manufacturerId);

    QSqlDatabase database = DatabaseManager::instance().database();

    const QDateTime now = QDateTime::currentDateTimeUtc();

    QSqlQuery query(database);

    query.prepare(R"(
        UPDATE build
        SET
            workspace_id = :workspace_id,
            build_type = :build_type,
            name = :name,
            set_number = :set_number,
            inventory_mode = :inventory_mode,
            manufacturer_id = :manufacturer_id,
            status = :status,
            is_active = :is_active,
            notes = :notes,
            modified_utc = :modified_utc
        WHERE id = :id
    )");

    query.bindValue(":workspace_id", build.workspaceId());

    query.bindValue(":build_type", buildType);

    query.bindValue(":name", build.name().trimmed());

    if (!build.setNumber().trimmed().isEmpty()) {
        query.bindValue(":set_number", build.setNumber().trimmed());
    } else {
        query.bindValue(":set_number", QVariant());
    }

    query.bindValue(":inventory_mode", inventoryMode);

    query.bindValue(":manufacturer_id", manufacturerId);

    query.bindValue(":status", status);

    query.bindValue(":is_active", build.isActive() ? 1 : 0);

    if (!build.notes().trimmed().isEmpty()) {
        query.bindValue(":notes", build.notes().trimmed());
    } else {
        query.bindValue(":notes", QVariant());
    }

    query.bindValue(":modified_utc", now.toString(Qt::ISODateWithMs));

    query.bindValue(":id", build.id());

    if (!query.exec()) {
        qCritical() << "Unable to update build:" << query.lastError().text();

        return false;
    }

    if (query.numRowsAffected() <= 0) {
        qWarning() << "Build update affected no rows."
                   << "BuildId:" << build.id();
        return false;
    }

    build.setModifiedUtc(now);

    return true;
}

Build BuildRepository::buildFromQuery(const QSqlQuery& query) const
{
    Build build;

    build.setId(query.value("id").toInt());

    build.setWorkspaceId(query.value("workspace_id").toInt());

    build.setBuildType(query.value("build_type").toString());

    build.setName(query.value("name").toString());

    build.setSetNumber(query.value("set_number").toString());

    build.setInventoryMode(query.value("inventory_mode").toString());

    build.setManufacturerId(query.value("manufacturer_id").toInt());

    build.setStatus(query.value("status").toString());

    build.setIsActive(query.value("is_active").toInt() != 0);

    build.setNotes(query.value("notes").toString());

    build.setCreatedUtc(
        QDateTime::fromString(query.value("created_utc").toString(), Qt::ISODateWithMs));

    build.setModifiedUtc(
        QDateTime::fromString(query.value("modified_utc").toString(), Qt::ISODateWithMs));

    return build;
}