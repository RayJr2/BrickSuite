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

#include "DatabaseSchema.h"

#include <QDateTime>
#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

bool DatabaseSchema::initialize(QSqlDatabase& database)
{
    if (!database.isOpen()) {
        qCritical() << "DatabaseSchema: database is not open.";
        return false;
    }

    //
    // Read the current schema version before opening the normal migration
    // transaction. Version 10 -> 11 may need to rebuild the Build table
    // because SQLite cannot ALTER an existing CHECK constraint. That rebuild
    // must temporarily disable foreign-key enforcement, which SQLite only
    // permits outside a transaction.
    //
    if (!createSchemaVersionTable(database)) {
        return false;
    }

    int version = 0;

    if (!getSchemaVersion(database, version)) {
        return false;
    }

    //
    // Existing Version 10 databases still have the original Build.status
    // CHECK constraint. Upgrade them before the normal migration transaction.
    //
    if (version == 10) {
        if (!migrateVersion10ToVersion11(database)) {
            return false;
        }

        version = 11;
    }

    //
    // Version 15 -> 16 rebuilds inventory_record so manufacturer_id becomes
    // NOT NULL and participates in the database-level uniqueness rule.
    // inventory_movement references inventory_record, so perform this rebuild
    // before the normal migration transaction while foreign keys can be
    // temporarily disabled.
    //
    if (version == 15) {
        if (!migrateVersion15ToVersion16(database)) {
            return false;
        }

        version = 16;
    }

    if (!database.transaction()) {
        qCritical() << "Unable to begin schema transaction:" << database.lastError().text();

        return false;
    }

    // Brand-new database.
    if (version == 0) {
        if (!createVersion1Schema(database)) {
            database.rollback();
            return false;
        }

        if (!setSchemaVersion(database, 1)) {
            database.rollback();
            return false;
        }

        version = 1;
    }

    // Version 1 -> Version 2.
    if (version == 1) {
        if (!migrateVersion1ToVersion2(database)) {
            database.rollback();
            return false;
        }

        if (!setSchemaVersion(database, 2)) {
            database.rollback();
            return false;
        }

        version = 2;
    }

    // Version 2 -> Version 3.
    if (version == 2) {
        if (!migrateVersion2ToVersion3(database)) {
            database.rollback();
            return false;
        }

        if (!setSchemaVersion(database, 3)) {
            database.rollback();
            return false;
        }

        version = 3;
    }

    // Version 3 -> Version 4.
    if (version == 3) {
        if (!migrateVersion3ToVersion4(database)) {
            database.rollback();
            return false;
        }

        if (!setSchemaVersion(database, 4)) {
            database.rollback();
            return false;
        }

        version = 4;
    }

    // Version 4 -> Version 5.
    if (version == 4) {
        if (!migrateVersion4ToVersion5(database)) {
            database.rollback();
            return false;
        }

        if (!setSchemaVersion(database, 5)) {
            database.rollback();
            return false;
        }

        version = 5;
    }

    // Version 5 -> Version 6.
    if (version == 5) {
        if (!migrateVersion5ToVersion6(database)) {
            database.rollback();
            return false;
        }

        if (!setSchemaVersion(database, 6)) {
            database.rollback();
            return false;
        }

        version = 6;
    }

    // Version 6 -> Version 7.
    if (version == 6) {
        if (!migrateVersion6ToVersion7(database)) {
            database.rollback();
            return false;
        }

        if (!setSchemaVersion(database, 7)) {
            database.rollback();
            return false;
        }

        version = 7;
    }

    // Version 7 -> Version 8.
    if (version == 7) {
        if (!migrateVersion7ToVersion8(database)) {
            database.rollback();
            return false;
        }

        if (!setSchemaVersion(database, 8)) {
            database.rollback();
            return false;
        }

        version = 8;
    }

    // Version 8 -> Version 9.
    if (version == 8) {
        if (!migrateVersion8ToVersion9(database)) {
            database.rollback();
            return false;
        }

        if (!setSchemaVersion(database, 9)) {
            database.rollback();
            return false;
        }

        version = 9;
    }

    // Version 9 -> Version 10.
    if (version == 9) {
        if (!migrateVersion9ToVersion10(database)) {
            database.rollback();
            return false;
        }

        if (!setSchemaVersion(database, 10)) {
            database.rollback();
            return false;
        }

        version = 10;
    }

    // Version 10 -> Version 11.
    //
    // A brand-new database created by this version already has Cancelled in
    // the Build.status CHECK constraint, so this migration simply advances
    // the schema version. Existing Version 10 databases are handled before
    // the normal migration transaction above.
    //
    if (version == 10) {
        if (!migrateVersion10ToVersion11(database)) {
            database.rollback();
            return false;
        }

        version = 11;
    }

    // Version 11 -> Version 12.
    if (version == 11) {
        if (!migrateVersion11ToVersion12(database)) {
            database.rollback();
            return false;
        }

        if (!setSchemaVersion(database, 12)) {
            database.rollback();
            return false;
        }

        version = 12;
    }

    // Version 12 -> Version 13.
    if (version == 12) {
        if (!migrateVersion12ToVersion13(database)) {
            database.rollback();
            return false;
        }

        if (!setSchemaVersion(database, 13)) {
            database.rollback();
            return false;
        }

        version = 13;
    }

    // Version 13 -> Version 14.
    if (version == 13) {
        if (!migrateVersion13ToVersion14(database)) {
            database.rollback();
            return false;
        }

        if (!setSchemaVersion(database, 14)) {
            database.rollback();
            return false;
        }

        version = 14;
    }

    // Version 14 -> Version 15.
    if (version == 14) {
        if (!migrateVersion14ToVersion15(database)) {
            database.rollback();
            return false;
        }

        if (!setSchemaVersion(database, 15)) {
            database.rollback();
            return false;
        }

        version = 15;
    }

    //
    // A brand-new/older database reaching Version 15 inside the normal
    // transaction cannot toggle foreign_keys for the required table rebuild.
    // Commit the completed migrations through 15, perform the Version 16
    // rebuild, then reopen the normal transaction for final validation.
    //
    if (version == 15) {
        if (!database.commit()) {
            qCritical() << "Unable to commit schema through Version 15:"
                        << database.lastError().text();
            database.rollback();
            return false;
        }

        if (!migrateVersion15ToVersion16(database)) {
            return false;
        }

        version = 16;

        if (!database.transaction()) {
            qCritical() << "Unable to resume schema transaction after Version 16 migration:"
                        << database.lastError().text();
            return false;
        }
    }

    // Version 16 -> Version 17.
    if (version == 16) {
        if (!migrateVersion16ToVersion17(database)) {
            database.rollback();
            return false;
        }

        if (!setSchemaVersion(database, 17)) {
            database.rollback();
            return false;
        }

        version = 17;
    }

    // Version 17 -> Version 18.
    if (version == 17) {
        if (!migrateVersion17ToVersion18(database)) {
            database.rollback();
            return false;
        }

        if (!setSchemaVersion(database, 18)) {
            database.rollback();
            return false;
        }

        version = 18;
    }

    // Version 18 -> Version 19.
    if (version == 18) {
        if (!migrateVersion18ToVersion19(database)) {
            database.rollback();
            return false;
        }

        if (!setSchemaVersion(database, 19)) {
            database.rollback();
            return false;
        }

        version = 19;
    }

    // Version 19 -> Version 20.
    if (version == 19) {
        if (!migrateVersion19ToVersion20(database)) {
            database.rollback();
            return false;
        }

        if (!setSchemaVersion(database, 20)) {
            database.rollback();
            return false;
        }

        version = 20;
    }

    // Version 20 -> Version 21.
    if (version == 20) {
        if (!migrateVersion20ToVersion21(database)) {
            database.rollback();
            return false;
        }

        if (!setSchemaVersion(database, 21)) {
            database.rollback();
            return false;
        }

        version = 21;
    }

    // Version 21 -> Version 22.
    if (version == 21) {
        if (!migrateVersion21ToVersion22(database)) {
            database.rollback();
            return false;
        }

        if (!setSchemaVersion(database, 22)) {
            database.rollback();
            return false;
        }

        version = 22;
    }

    // Version 22 -> Version 23.
    if (version == 22) {
        if (!migrateVersion22ToVersion23(database)) {
            database.rollback();
            return false;
        }

        if (!setSchemaVersion(database, 23)) {
            database.rollback();
            return false;
        }

        version = 23;
    }

    // Version 23 -> Version 24.
    if (version == 23) {
        if (!migrateVersion23ToVersion24(database)) {
            database.rollback();
            return false;
        }

        if (!setSchemaVersion(database, 24)) {
            database.rollback();
            return false;
        }

        version = 24;
    }

    // Version 24 -> Version 25.
    if (version == 24) {
        if (!migrateVersion24ToVersion25(database)) {
            database.rollback();
            return false;
        }
        if (!setSchemaVersion(database, 25)) {
            database.rollback();
            return false;
        }
        version = 25;
    }

    if (version != CurrentSchemaVersion) {
        qCritical() << "Unsupported BrickSuite database schema version:" << version;

        database.rollback();
        return false;
    }

    if (!database.commit()) {
        qCritical() << "Unable to commit schema transaction:" << database.lastError().text();

        database.rollback();
        return false;
    }

    qInfo() << "BrickSuite database schema initialized. Version:" << version;

    return true;
}

bool DatabaseSchema::createSchemaVersionTable(QSqlDatabase& database)
{
    QSqlQuery query(database);

    if (!query.exec(R"(
        CREATE TABLE IF NOT EXISTS schema_version
        (
            version INTEGER NOT NULL
        )
    )")) {
        qCritical() << "Unable to create schema_version table:" << query.lastError().text();

        return false;
    }

    return true;
}

bool DatabaseSchema::getSchemaVersion(QSqlDatabase& database, int& version)
{
    QSqlQuery query(database);

    if (!query.exec("SELECT version FROM schema_version LIMIT 1")) {
        qCritical() << "Unable to query schema version:" << query.lastError().text();

        return false;
    }

    if (!query.next()) {
        version = 0;
        return true;
    }

    version = query.value(0).toInt();

    return true;
}

bool DatabaseSchema::setSchemaVersion(QSqlDatabase& database, int version)
{
    QSqlQuery deleteQuery(database);

    if (!deleteQuery.exec("DELETE FROM schema_version")) {
        qCritical() << "Unable to clear schema version:" << deleteQuery.lastError().text();

        return false;
    }

    QSqlQuery insertQuery(database);

    insertQuery.prepare("INSERT INTO schema_version (version) "
                        "VALUES (:version)");

    insertQuery.bindValue(":version", version);

    if (!insertQuery.exec()) {
        qCritical() << "Unable to set schema version:" << insertQuery.lastError().text();

        return false;
    }

    return true;
}

bool DatabaseSchema::createVersion1Schema(QSqlDatabase& database)
{
    return createWorkspaceTable(database);
}

bool DatabaseSchema::migrateVersion1ToVersion2(QSqlDatabase& database)
{
    if (!createColorTable(database))
        return false;

    if (!createPartCategoryTable(database))
        return false;

    if (!createStorageLocationTypeTable(database))
        return false;

    if (!createStorageLocationTable(database))
        return false;

    if (!seedStorageLocationTypes(database))
        return false;

    return true;
}

bool DatabaseSchema::createWorkspaceTable(QSqlDatabase& database)
{
    QSqlQuery query(database);

    if (!query.exec(R"(
        CREATE TABLE IF NOT EXISTS workspace
        (
            id           INTEGER PRIMARY KEY AUTOINCREMENT,
            name         TEXT NOT NULL,
            description  TEXT,
            created_utc  TEXT NOT NULL,
            modified_utc TEXT NOT NULL,
            is_active    INTEGER NOT NULL DEFAULT 1
        )
    )")) {
        qCritical() << "Unable to create workspace table:" << query.lastError().text();

        return false;
    }

    return true;
}

bool DatabaseSchema::createColorTable(QSqlDatabase& database)
{
    QSqlQuery query(database);

    if (!query.exec(R"(
        CREATE TABLE IF NOT EXISTS color
        (
            id               INTEGER PRIMARY KEY AUTOINCREMENT,
            name             TEXT NOT NULL,
            rgb              TEXT,
            is_transparent   INTEGER NOT NULL DEFAULT 0,
            rebrickable_id   INTEGER,
            created_utc      TEXT NOT NULL,
            modified_utc     TEXT NOT NULL,

            UNIQUE(rebrickable_id)
        )
    )")) {
        qCritical() << "Unable to create color table:" << query.lastError().text();

        return false;
    }

    return true;
}

bool DatabaseSchema::createPartCategoryTable(QSqlDatabase& database)
{
    QSqlQuery query(database);

    if (!query.exec(R"(
        CREATE TABLE IF NOT EXISTS part_category
        (
            id               INTEGER PRIMARY KEY AUTOINCREMENT,
            name             TEXT NOT NULL,
            rebrickable_id   INTEGER,
            created_utc      TEXT NOT NULL,
            modified_utc     TEXT NOT NULL,

            UNIQUE(rebrickable_id)
        )
    )")) {
        qCritical() << "Unable to create part_category table:" << query.lastError().text();

        return false;
    }

    return true;
}

bool DatabaseSchema::createStorageLocationTypeTable(QSqlDatabase& database)
{
    QSqlQuery query(database);

    if (!query.exec(R"(
        CREATE TABLE IF NOT EXISTS storage_location_type
        (
            id           INTEGER PRIMARY KEY AUTOINCREMENT,
            name         TEXT NOT NULL UNIQUE,
            description  TEXT,
            is_system    INTEGER NOT NULL DEFAULT 0,
            is_active    INTEGER NOT NULL DEFAULT 1,
            sort_order   INTEGER NOT NULL DEFAULT 0
        )
    )")) {
        qCritical() << "Unable to create storage_location_type table:" << query.lastError().text();

        return false;
    }

    return true;
}

bool DatabaseSchema::createStorageLocationTable(QSqlDatabase& database)
{
    QSqlQuery query(database);

    if (!query.exec(R"(
        CREATE TABLE IF NOT EXISTS storage_location
        (
            id                  INTEGER PRIMARY KEY AUTOINCREMENT,
            workspace_id        INTEGER NOT NULL,
            parent_location_id  INTEGER,
            location_type_id    INTEGER NOT NULL,
            name                TEXT NOT NULL,
            description         TEXT,
            sort_order          INTEGER NOT NULL DEFAULT 0,
            is_active           INTEGER NOT NULL DEFAULT 1,
            created_utc         TEXT NOT NULL,
            modified_utc        TEXT NOT NULL,

            FOREIGN KEY (workspace_id)
                REFERENCES workspace(id),

            FOREIGN KEY (parent_location_id)
                REFERENCES storage_location(id),

            FOREIGN KEY (location_type_id)
                REFERENCES storage_location_type(id)
        )
    )")) {
        qCritical() << "Unable to create storage_location table:" << query.lastError().text();

        return false;
    }

    return true;
}

bool DatabaseSchema::seedStorageLocationTypes(QSqlDatabase& database)
{
    struct LocationType
    {
        const char* name;
        const char* description;
        int sortOrder;
    };

    const LocationType types[] = {{"Area", "A physical workshop or storage area.", 10},
                                  {"Cabinet", "A cabinet or storage unit.", 20},
                                  {"Shelf", "A shelf within a storage area or storage unit.", 30},
                                  {"Case", "A portable or fixed storage case.", 40},
                                  {"Drawer", "A drawer within a storage unit.", 50},
                                  {"Bin", "A removable or fixed storage bin.", 60},
                                  {"Tray", "A storage tray.", 70},
                                  {"Bag", "A bag used to contain or group stored parts.", 75},
                                  {"Compartment", "A compartment within another location.", 80},
                                  {"Divider", "A divided section of another location.", 90}};

    for (const LocationType& type : types) {
        QSqlQuery query(database);

        query.prepare(R"(
            INSERT OR IGNORE INTO storage_location_type
            (
                name,
                description,
                is_system,
                is_active,
                sort_order
            )
            VALUES
            (
                :name,
                :description,
                1,
                1,
                :sort_order
            )
        )");

        query.bindValue(":name", type.name);

        query.bindValue(":description", type.description);

        query.bindValue(":sort_order", type.sortOrder);

        if (!query.exec()) {
            qCritical() << "Unable to seed storage location type:" << type.name
                        << query.lastError().text();

            return false;
        }
    }

    return true;
}

bool DatabaseSchema::migrateVersion2ToVersion3(QSqlDatabase& database)
{
    if (!createPartTable(database))
        return false;

    if (!createInventoryRecordTable(database))
        return false;

    if (!createInventoryIndexes(database))
        return false;

    return true;
}

bool DatabaseSchema::createPartTable(QSqlDatabase& database)
{
    QSqlQuery query(database);

    if (!query.exec(R"(
        CREATE TABLE IF NOT EXISTS part
        (
            id                  INTEGER PRIMARY KEY AUTOINCREMENT,
            part_number         TEXT NOT NULL,
            name                TEXT NOT NULL,
            part_category_id    INTEGER,
            rebrickable_part_id TEXT,
            is_active           INTEGER NOT NULL DEFAULT 1,
            created_utc         TEXT NOT NULL,
            modified_utc        TEXT NOT NULL,

            FOREIGN KEY (part_category_id)
                REFERENCES part_category(id),

            UNIQUE(part_number),
            UNIQUE(rebrickable_part_id)
        )
    )")) {
        qCritical() << "Unable to create part table:" << query.lastError().text();

        return false;
    }

    return true;
}

bool DatabaseSchema::createInventoryRecordTable(QSqlDatabase& database)
{
    QSqlQuery query(database);

    if (!query.exec(R"(
        CREATE TABLE IF NOT EXISTS inventory_record
        (
            id                  INTEGER PRIMARY KEY AUTOINCREMENT,
            workspace_id        INTEGER NOT NULL,
            part_id             INTEGER NOT NULL,
            color_id            INTEGER NOT NULL,
            storage_location_id INTEGER NOT NULL,

            condition           TEXT NOT NULL DEFAULT 'Used',
            ownership_type      TEXT NOT NULL DEFAULT 'Owned',

            quantity            INTEGER NOT NULL DEFAULT 0,

            created_utc         TEXT NOT NULL,
            modified_utc        TEXT NOT NULL,

            FOREIGN KEY (workspace_id)
                REFERENCES workspace(id),

            FOREIGN KEY (part_id)
                REFERENCES part(id),

            FOREIGN KEY (color_id)
                REFERENCES color(id),

            FOREIGN KEY (storage_location_id)
                REFERENCES storage_location(id),

            CHECK(quantity >= 0),

            UNIQUE
            (
                workspace_id,
                part_id,
                color_id,
                storage_location_id,
                condition,
                ownership_type
            )
        )
    )")) {
        qCritical() << "Unable to create inventory_record table:" << query.lastError().text();

        return false;
    }

    return true;
}

bool DatabaseSchema::createInventoryIndexes(QSqlDatabase& database)
{
    QSqlQuery query(database);

    if (!query.exec(R"(
        CREATE INDEX IF NOT EXISTS
            idx_part_category
        ON part(part_category_id)
    )")) {
        qCritical() << "Unable to create part category index:" << query.lastError().text();

        return false;
    }

    if (!query.exec(R"(
        CREATE INDEX IF NOT EXISTS
            idx_inventory_workspace
        ON inventory_record(workspace_id)
    )")) {
        qCritical() << "Unable to create inventory workspace index:" << query.lastError().text();

        return false;
    }

    if (!query.exec(R"(
        CREATE INDEX IF NOT EXISTS
            idx_inventory_part
        ON inventory_record(part_id)
    )")) {
        qCritical() << "Unable to create inventory part index:" << query.lastError().text();

        return false;
    }

    if (!query.exec(R"(
        CREATE INDEX IF NOT EXISTS
            idx_inventory_storage_location
        ON inventory_record(storage_location_id)
    )")) {
        qCritical() << "Unable to create inventory storage-location index:"
                    << query.lastError().text();

        return false;
    }

    return true;
}

bool DatabaseSchema::migrateVersion3ToVersion4(QSqlDatabase& database)
{
    QSqlQuery query(database);

    if (!query.exec(R"(
        ALTER TABLE part
        ADD COLUMN material TEXT
    )")) {
        qCritical() << "Unable to add material column to part table:" << query.lastError().text();

        return false;
    }

    return true;
}

bool DatabaseSchema::migrateVersion4ToVersion5(QSqlDatabase& database)
{
    if (!createInventoryMovementTable(database))
        return false;

    if (!createInventoryMovementIndexes(database))
        return false;

    return true;
}

bool DatabaseSchema::createInventoryMovementTable(QSqlDatabase& database)
{
    QSqlQuery query(database);

    if (!query.exec(R"(
        CREATE TABLE IF NOT EXISTS inventory_movement
        (
            id                       INTEGER PRIMARY KEY AUTOINCREMENT,

            workspace_id             INTEGER NOT NULL,
            inventory_record_id      INTEGER,

            part_id                  INTEGER NOT NULL,
            color_id                 INTEGER NOT NULL,

            movement_type            TEXT NOT NULL,

            quantity_change          INTEGER NOT NULL,

            from_storage_location_id INTEGER,
            to_storage_location_id   INTEGER,

            condition                TEXT,
            ownership_type           TEXT,

            reference_type           TEXT,
            reference_id             TEXT,

            notes                    TEXT,

            created_utc              TEXT NOT NULL,

            FOREIGN KEY (workspace_id)
                REFERENCES workspace(id),

            FOREIGN KEY (inventory_record_id)
                REFERENCES inventory_record(id),

            FOREIGN KEY (part_id)
                REFERENCES part(id),

            FOREIGN KEY (color_id)
                REFERENCES color(id),

            FOREIGN KEY (from_storage_location_id)
                REFERENCES storage_location(id),

            FOREIGN KEY (to_storage_location_id)
                REFERENCES storage_location(id)
        )
    )")) {
        qCritical() << "Unable to create inventory_movement table:" << query.lastError().text();

        return false;
    }

    return true;
}

bool DatabaseSchema::createInventoryMovementIndexes(QSqlDatabase& database)
{
    QSqlQuery query(database);

    if (!query.exec(R"(
        CREATE INDEX IF NOT EXISTS
            idx_inventory_movement_workspace
        ON inventory_movement(workspace_id)
    )")) {
        qCritical() << "Unable to create movement workspace index:" << query.lastError().text();

        return false;
    }

    if (!query.exec(R"(
        CREATE INDEX IF NOT EXISTS
            idx_inventory_movement_part
        ON inventory_movement(part_id)
    )")) {
        qCritical() << "Unable to create movement part index:" << query.lastError().text();

        return false;
    }

    if (!query.exec(R"(
        CREATE INDEX IF NOT EXISTS
            idx_inventory_movement_record
        ON inventory_movement(inventory_record_id)
    )")) {
        qCritical() << "Unable to create movement inventory-record index:"
                    << query.lastError().text();

        return false;
    }

    if (!query.exec(R"(
        CREATE INDEX IF NOT EXISTS
            idx_inventory_movement_created
        ON inventory_movement(created_utc)
    )")) {
        qCritical() << "Unable to create movement date index:" << query.lastError().text();

        return false;
    }

    return true;
}

bool DatabaseSchema::migrateVersion5ToVersion6(QSqlDatabase& database)
{
    if (!createBuildTable(database))
        return false;

    if (!createBuildRequirementTable(database))
        return false;

    if (!createBuildAllocationTable(database))
        return false;

    if (!createBuildIndexes(database))
        return false;

    return true;
}

bool DatabaseSchema::createBuildTable(QSqlDatabase& database)
{
    QSqlQuery query(database);

    if (!query.exec(R"(
        CREATE TABLE IF NOT EXISTS build
        (
            id            INTEGER PRIMARY KEY AUTOINCREMENT,

            workspace_id  INTEGER NOT NULL,

            build_type    TEXT NOT NULL,
            name          TEXT NOT NULL,

            set_number    TEXT,

            status        TEXT NOT NULL DEFAULT 'Planned',

            notes         TEXT,

            created_utc   TEXT NOT NULL,
            modified_utc  TEXT NOT NULL,

            FOREIGN KEY (workspace_id)
                REFERENCES workspace(id),

            CHECK
            (
                build_type IN
                (
                    'Set',
                    'MOC'
                )
            ),

            CHECK
            (
                status IN
                (
                    'Planned',
                    'Pulling',
                    'Complete',
                    'Disassembled',
                    'Cancelled'
                )
            )
        )
    )")) {
        qCritical() << "Unable to create build table:" << query.lastError().text();

        return false;
    }

    return true;
}

bool DatabaseSchema::createBuildRequirementTable(QSqlDatabase& database)
{
    QSqlQuery query(database);

    if (!query.exec(R"(
        CREATE TABLE IF NOT EXISTS build_requirement
        (
            id                 INTEGER PRIMARY KEY AUTOINCREMENT,

            build_id           INTEGER NOT NULL,

            part_id            INTEGER NOT NULL,
            color_id           INTEGER NOT NULL,

            quantity_required  INTEGER NOT NULL,

            is_spare           INTEGER NOT NULL DEFAULT 0,

            created_utc        TEXT NOT NULL,
            modified_utc       TEXT NOT NULL,

            FOREIGN KEY (build_id)
                REFERENCES build(id),

            FOREIGN KEY (part_id)
                REFERENCES part(id),

            FOREIGN KEY (color_id)
                REFERENCES color(id),

            CHECK(quantity_required > 0),

            UNIQUE
            (
                build_id,
                part_id,
                color_id,
                is_spare
            )
        )
    )")) {
        qCritical() << "Unable to create build_requirement table:" << query.lastError().text();

        return false;
    }

    return true;
}

bool DatabaseSchema::createBuildAllocationTable(QSqlDatabase& database)
{
    QSqlQuery query(database);

    if (!query.exec(R"(
        CREATE TABLE IF NOT EXISTS build_allocation
        (
            id                   INTEGER PRIMARY KEY AUTOINCREMENT,

            build_id             INTEGER NOT NULL,

            inventory_record_id  INTEGER NOT NULL,

            part_id              INTEGER NOT NULL,
            color_id             INTEGER NOT NULL,

            storage_location_id  INTEGER NOT NULL,

            quantity_allocated   INTEGER NOT NULL,

            created_utc          TEXT NOT NULL,
            modified_utc         TEXT NOT NULL,

            FOREIGN KEY (build_id)
                REFERENCES build(id),

            FOREIGN KEY (inventory_record_id)
                REFERENCES inventory_record(id),

            FOREIGN KEY (part_id)
                REFERENCES part(id),

            FOREIGN KEY (color_id)
                REFERENCES color(id),

            FOREIGN KEY (storage_location_id)
                REFERENCES storage_location(id),

            CHECK(quantity_allocated > 0),

            UNIQUE
            (
                build_id,
                inventory_record_id
            )
        )
    )")) {
        qCritical() << "Unable to create build_allocation table:" << query.lastError().text();

        return false;
    }

    return true;
}

bool DatabaseSchema::createBuildIndexes(QSqlDatabase& database)
{
    QSqlQuery query(database);

    if (!query.exec(R"(
        CREATE INDEX IF NOT EXISTS
            idx_build_workspace
        ON build(workspace_id)
    )")) {
        qCritical() << "Unable to create build workspace index:" << query.lastError().text();

        return false;
    }

    if (!query.exec(R"(
        CREATE INDEX IF NOT EXISTS
            idx_build_requirement_build
        ON build_requirement(build_id)
    )")) {
        qCritical() << "Unable to create build requirement index:" << query.lastError().text();

        return false;
    }

    if (!query.exec(R"(
        CREATE INDEX IF NOT EXISTS
            idx_build_requirement_part_color
        ON build_requirement(part_id, color_id)
    )")) {
        qCritical() << "Unable to create build requirement part/color index:"
                    << query.lastError().text();

        return false;
    }

    if (!query.exec(R"(
        CREATE INDEX IF NOT EXISTS
            idx_build_allocation_build
        ON build_allocation(build_id)
    )")) {
        qCritical() << "Unable to create build allocation index:" << query.lastError().text();

        return false;
    }

    if (!query.exec(R"(
        CREATE INDEX IF NOT EXISTS
            idx_build_allocation_inventory
        ON build_allocation(inventory_record_id)
    )")) {
        qCritical() << "Unable to create build allocation inventory index:"
                    << query.lastError().text();

        return false;
    }

    return true;
}

bool DatabaseSchema::migrateVersion6ToVersion7(QSqlDatabase& database)
{
    QSqlQuery query(database);

    if (!query.exec(R"(
        ALTER TABLE build
        ADD COLUMN inventory_mode TEXT NOT NULL DEFAULT 'Stock'
        CHECK
        (
            inventory_mode IN
            (
                'Stock',
                'CompleteSet'
            )
        )
    )")) {
        qCritical() << "Unable to add inventory_mode column to build table:"
                    << query.lastError().text();

        return false;
    }

    return true;
}

bool DatabaseSchema::migrateVersion7ToVersion8(QSqlDatabase& database)
{
    QSqlQuery query(database);

    //
    // Persist how many pieces of each Build Requirement
    // have actually been physically pulled.
    //
    if (!query.exec(R"(
        ALTER TABLE build_requirement
        ADD COLUMN quantity_pulled INTEGER NOT NULL DEFAULT 0
        CHECK(quantity_pulled >= 0)
    )")) {
        qCritical() << "Unable to add quantity_pulled to build_requirement:"
                    << query.lastError().text();

        return false;
    }

    //
    // Backfill previously reconciled BuildPull movements.
    //
    // This preserves pulls already performed before schema v8.
    // Spare requirements are excluded because BrickSuite does
    // not allocate/pull spares by default.
    //
    if (!query.exec(R"(
        UPDATE build_requirement
        SET quantity_pulled =
            MIN
            (
                quantity_required,

                COALESCE
                (
                    (
                        SELECT
                            SUM(-im.quantity_change)

                        FROM inventory_movement im

                        WHERE im.movement_type = 'BuildPull'
                          AND im.reference_type = 'Build'
                          AND CAST(im.reference_id AS INTEGER)
                              = build_requirement.build_id
                          AND im.part_id
                              = build_requirement.part_id
                          AND im.color_id
                              = build_requirement.color_id
                          AND im.quantity_change < 0
                    ),
                    0
                )
            )
        WHERE is_spare = 0
    )")) {
        qCritical() << "Unable to backfill Build Requirement pulled quantities:"
                    << query.lastError().text();

        return false;
    }

    return true;
}

bool DatabaseSchema::migrateVersion8ToVersion9(QSqlDatabase& database)
{
    return createSetCatalogTable(database);
}

bool DatabaseSchema::migrateVersion9ToVersion10(QSqlDatabase& database)
{
    QSqlQuery query(database);

    //
    // Builds are never deleted. Version 10 adds an active/archive
    // lifecycle flag so completed historical Builds can be retired
    // from the normal working list without changing their IDs or
    // breaking requirements, allocations, lost/found history, or
    // other references.
    //
    if (!query.exec(R"(
        ALTER TABLE build
        ADD COLUMN is_active INTEGER NOT NULL DEFAULT 1
    )")) {
        qCritical() << "Unable to add build.is_active:" << query.lastError().text();
        return false;
    }

    if (!query.exec(R"(
        CREATE INDEX IF NOT EXISTS
            idx_build_workspace_active
        ON build(workspace_id, is_active)
    )")) {
        qCritical() << "Unable to create Build active index:" << query.lastError().text();
        return false;
    }

    return true;
}

bool DatabaseSchema::migrateVersion10ToVersion11(QSqlDatabase& database)
{
    //
    // SQLite does not support ALTER CHECK CONSTRAINT. Existing Version 10
    // databases therefore require the standard SQLite table-rebuild pattern.
    //
    // First inspect the table definition. Brand-new databases created by this
    // source already include Cancelled and only need the version advanced.
    //
    QSqlQuery schemaQuery(database);

    schemaQuery.prepare(R"(
        SELECT sql
        FROM sqlite_master
        WHERE type = 'table'
          AND name = 'build'
    )");

    if (!schemaQuery.exec() || !schemaQuery.next()) {
        qCritical() << "Unable to inspect Build table before Version 11 migration:"
                    << schemaQuery.lastError().text();
        return false;
    }

    const QString buildSql = schemaQuery.value(0).toString();

    //
    // Release the sqlite_master read statement before attempting to rebuild
    // the Build table. Leaving this query active holds a schema read lock and
    // SQLite can then reject DROP TABLE with "database table is locked".
    //
    schemaQuery.finish();

    if (buildSql.contains("'Cancelled'", Qt::CaseInsensitive)) {
        return setSchemaVersion(database, 11);
    }

    //
    // This branch is used for an existing Version 10 database and is called
    // before DatabaseSchema opens its normal migration transaction.
    //
    QSqlQuery pragmaQuery(database);

    bool foreignKeysWereEnabled = false;

    if (pragmaQuery.exec("PRAGMA foreign_keys") && pragmaQuery.next()) {
        foreignKeysWereEnabled = pragmaQuery.value(0).toInt() != 0;
    }

    //
    // Release the PRAGMA result before changing schema state.
    //
    pragmaQuery.finish();

    if (foreignKeysWereEnabled) {
        if (!pragmaQuery.exec("PRAGMA foreign_keys = OFF")) {
            qCritical() << "Unable to temporarily disable foreign keys:"
                        << pragmaQuery.lastError().text();
            return false;
        }

        pragmaQuery.finish();
    }

    if (!database.transaction()) {
        qCritical() << "Unable to begin Version 11 Build migration:"
                    << database.lastError().text();

        if (foreignKeysWereEnabled)
            pragmaQuery.exec("PRAGMA foreign_keys = ON");

        return false;
    }

    QSqlQuery query(database);

    if (!query.exec(R"(
        CREATE TABLE build_v11
        (
            id            INTEGER PRIMARY KEY AUTOINCREMENT,

            workspace_id  INTEGER NOT NULL,

            build_type    TEXT NOT NULL,
            name          TEXT NOT NULL,

            set_number    TEXT,

            inventory_mode TEXT NOT NULL DEFAULT 'Stock',

            status        TEXT NOT NULL DEFAULT 'Planned',
            is_active     INTEGER NOT NULL DEFAULT 1,

            notes         TEXT,

            created_utc   TEXT NOT NULL,
            modified_utc  TEXT NOT NULL,

            FOREIGN KEY (workspace_id)
                REFERENCES workspace(id),

            CHECK
            (
                build_type IN
                (
                    'Set',
                    'MOC'
                )
            ),

            CHECK
            (
                inventory_mode IN
                (
                    'Stock',
                    'CompleteSet'
                )
            ),

            CHECK
            (
                status IN
                (
                    'Planned',
                    'Pulling',
                    'Complete',
                    'Disassembled',
                    'Cancelled'
                )
            )
        )
    )")) {
        qCritical() << "Unable to create Version 11 Build table:"
                    << query.lastError().text();

        database.rollback();

        if (foreignKeysWereEnabled)
            pragmaQuery.exec("PRAGMA foreign_keys = ON");

        return false;
    }

    if (!query.exec(R"(
        INSERT INTO build_v11
        (
            id,
            workspace_id,
            build_type,
            name,
            set_number,
            inventory_mode,
            status,
            is_active,
            notes,
            created_utc,
            modified_utc
        )
        SELECT
            id,
            workspace_id,
            build_type,
            name,
            set_number,
            inventory_mode,
            status,
            is_active,
            notes,
            created_utc,
            modified_utc
        FROM build
    )")) {
        qCritical() << "Unable to copy Builds during Version 11 migration:"
                    << query.lastError().text();

        database.rollback();

        if (foreignKeysWereEnabled)
            pragmaQuery.exec("PRAGMA foreign_keys = ON");

        return false;
    }

    if (!query.exec("DROP TABLE build")) {
        qCritical() << "Unable to replace old Build table:"
                    << query.lastError().text();

        database.rollback();

        if (foreignKeysWereEnabled)
            pragmaQuery.exec("PRAGMA foreign_keys = ON");

        return false;
    }

    if (!query.exec("ALTER TABLE build_v11 RENAME TO build")) {
        qCritical() << "Unable to rename Version 11 Build table:"
                    << query.lastError().text();

        database.rollback();

        if (foreignKeysWereEnabled)
            pragmaQuery.exec("PRAGMA foreign_keys = ON");

        return false;
    }

    if (!createBuildIndexes(database)) {
        database.rollback();

        if (foreignKeysWereEnabled)
            pragmaQuery.exec("PRAGMA foreign_keys = ON");

        return false;
    }

    if (!query.exec(R"(
        CREATE INDEX IF NOT EXISTS
            idx_build_workspace_active
        ON build(workspace_id, is_active)
    )")) {
        qCritical() << "Unable to recreate Build active index:"
                    << query.lastError().text();

        database.rollback();

        if (foreignKeysWereEnabled)
            pragmaQuery.exec("PRAGMA foreign_keys = ON");

        return false;
    }

    if (!setSchemaVersion(database, 11)) {
        database.rollback();

        if (foreignKeysWereEnabled)
            pragmaQuery.exec("PRAGMA foreign_keys = ON");

        return false;
    }

    if (!database.commit()) {
        qCritical() << "Unable to commit Version 11 Build migration:"
                    << database.lastError().text();

        database.rollback();

        if (foreignKeysWereEnabled)
            pragmaQuery.exec("PRAGMA foreign_keys = ON");

        return false;
    }

    if (foreignKeysWereEnabled) {
        if (!pragmaQuery.exec("PRAGMA foreign_keys = ON")) {
            qCritical() << "Unable to re-enable foreign keys after Version 11 migration:"
                        << pragmaQuery.lastError().text();
            return false;
        }
    }

    //
    // Validate that rebuilding the referenced Build table did not leave any
    // broken foreign-key references.
    //
    if (!pragmaQuery.exec("PRAGMA foreign_key_check")) {
        qCritical() << "Unable to validate foreign keys after Version 11 migration:"
                    << pragmaQuery.lastError().text();
        return false;
    }

    if (pragmaQuery.next()) {
        qCritical() << "Foreign-key validation failed after Version 11 Build migration.";
        return false;
    }

    return true;
}

bool DatabaseSchema::createSetCatalogTable(QSqlDatabase& database)
{
    QSqlQuery query(database);

    if (!query.exec(R"(
        CREATE TABLE IF NOT EXISTS set_catalog
        (
            id           INTEGER PRIMARY KEY AUTOINCREMENT,
            set_number   TEXT NOT NULL UNIQUE,
            name         TEXT NOT NULL,
            year         INTEGER NOT NULL DEFAULT 0,
            theme_id     INTEGER NOT NULL DEFAULT 0,
            num_parts    INTEGER NOT NULL DEFAULT 0,
            image_url    TEXT,
            created_utc  TEXT NOT NULL,
            modified_utc TEXT NOT NULL,

            CHECK(year >= 0),
            CHECK(theme_id >= 0),
            CHECK(num_parts >= 0)
        )
    )")) {
        qCritical() << "Unable to create set_catalog table:" << query.lastError().text();

        return false;
    }

    if (!query.exec(R"(
        CREATE INDEX IF NOT EXISTS
            idx_set_catalog_number
        ON set_catalog(set_number)
    )")) {
        qCritical() << "Unable to create set number index:" << query.lastError().text();

        return false;
    }

    if (!query.exec(R"(
        CREATE INDEX IF NOT EXISTS
            idx_set_catalog_year
        ON set_catalog(year)
    )")) {
        qCritical() << "Unable to create set year index:" << query.lastError().text();

        return false;
    }

    if (!query.exec(R"(
        CREATE INDEX IF NOT EXISTS
            idx_set_catalog_theme
        ON set_catalog(theme_id)
    )")) {
        qCritical() << "Unable to create set theme index:" << query.lastError().text();

        return false;
    }

    return true;
}

bool DatabaseSchema::createMinifigCatalogTables(QSqlDatabase& database)
{
    QSqlQuery query(database);

    if (!query.exec(R"(
        CREATE TABLE IF NOT EXISTS minifig_catalog
        (
            id           INTEGER PRIMARY KEY AUTOINCREMENT,
            name         TEXT NOT NULL,
            num_parts    INTEGER NOT NULL DEFAULT 0 CHECK(num_parts >= 0),
            image_url    TEXT,
            is_active    INTEGER NOT NULL DEFAULT 1 CHECK(is_active IN (0, 1)),
            created_utc  TEXT NOT NULL,
            modified_utc TEXT NOT NULL
        )
    )")) {
        qCritical() << "Unable to create minifig_catalog table:"
                    << query.lastError().text();
        return false;
    }

    if (!query.exec(R"(
        CREATE TABLE IF NOT EXISTS minifig_external_identifier
        (
            id                  INTEGER PRIMARY KEY AUTOINCREMENT,
            minifig_catalog_id INTEGER NOT NULL,
            provider            TEXT NOT NULL COLLATE NOCASE,
            external_id         TEXT NOT NULL COLLATE NOCASE,
            source              TEXT NOT NULL,
            is_active           INTEGER NOT NULL DEFAULT 1 CHECK(is_active IN (0, 1)),
            created_utc         TEXT NOT NULL,
            modified_utc        TEXT NOT NULL,

            FOREIGN KEY(minifig_catalog_id) REFERENCES minifig_catalog(id),
            UNIQUE(provider, external_id),
            UNIQUE(minifig_catalog_id, provider)
        )
    )")) {
        qCritical() << "Unable to create minifig_external_identifier table:"
                    << query.lastError().text();
        return false;
    }

    if (!query.exec(R"(
        CREATE INDEX IF NOT EXISTS idx_minifig_catalog_active_name
        ON minifig_catalog(is_active, name)
    )")) {
        qCritical() << "Unable to create Minifig Catalog search index:"
                    << query.lastError().text();
        return false;
    }

    if (!query.exec(R"(
        CREATE INDEX IF NOT EXISTS idx_minifig_identifier_catalog_active
        ON minifig_external_identifier(minifig_catalog_id, is_active)
    )")) {
        qCritical() << "Unable to create Minifig identifier catalog index:"
                    << query.lastError().text();
        return false;
    }

    return true;
}

bool DatabaseSchema::createThemeCatalogTables(QSqlDatabase& database)
{
    QSqlQuery query(database);
    const QStringList statements = {
        R"(CREATE TABLE IF NOT EXISTS theme_catalog (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL,
            parent_theme_catalog_id INTEGER,
            is_active INTEGER NOT NULL DEFAULT 1 CHECK(is_active IN (0,1)),
            created_utc TEXT NOT NULL,
            modified_utc TEXT NOT NULL,
            FOREIGN KEY(parent_theme_catalog_id) REFERENCES theme_catalog(id)
        ))",
        R"(CREATE TABLE IF NOT EXISTS theme_external_identifier (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            theme_catalog_id INTEGER NOT NULL,
            provider TEXT NOT NULL COLLATE NOCASE,
            external_id TEXT NOT NULL COLLATE NOCASE,
            source TEXT NOT NULL,
            is_active INTEGER NOT NULL DEFAULT 1 CHECK(is_active IN (0,1)),
            created_utc TEXT NOT NULL,
            modified_utc TEXT NOT NULL,
            FOREIGN KEY(theme_catalog_id) REFERENCES theme_catalog(id),
            UNIQUE(provider, external_id),
            UNIQUE(theme_catalog_id, provider)
        ))",
        R"(CREATE TABLE IF NOT EXISTS minifig_theme (
            minifig_catalog_id INTEGER NOT NULL,
            theme_catalog_id INTEGER NOT NULL,
            provider TEXT NOT NULL COLLATE NOCASE,
            PRIMARY KEY(minifig_catalog_id, theme_catalog_id, provider),
            FOREIGN KEY(minifig_catalog_id) REFERENCES minifig_catalog(id),
            FOREIGN KEY(theme_catalog_id) REFERENCES theme_catalog(id)
        ))",
        "CREATE INDEX IF NOT EXISTS idx_theme_parent_active ON theme_catalog(parent_theme_catalog_id, is_active)",
        "CREATE INDEX IF NOT EXISTS idx_theme_active_name ON theme_catalog(is_active, name)",
        "CREATE INDEX IF NOT EXISTS idx_theme_identity_provider ON theme_external_identifier(provider, external_id, is_active)",
        "CREATE INDEX IF NOT EXISTS idx_minifig_theme_theme ON minifig_theme(theme_catalog_id, provider, minifig_catalog_id)",
        "CREATE INDEX IF NOT EXISTS idx_minifig_theme_minifig ON minifig_theme(minifig_catalog_id, provider, theme_catalog_id)"
    };
    for (const QString& statement : statements) {
        if (!query.exec(statement)) {
            qCritical() << "Unable to create Theme Catalog schema:" << query.lastError().text();
            return false;
        }
    }
    return true;
}

bool DatabaseSchema::migrateVersion11ToVersion12(QSqlDatabase& database)
{
    if (!createExternalColorMappingTable(database))
        return false;

    if (!createExternalPartMappingTable(database))
        return false;

    return true;
}

bool DatabaseSchema::createExternalColorMappingTable(QSqlDatabase& database)
{
    QSqlQuery query(database);

    if (!query.exec(R"(
        CREATE TABLE IF NOT EXISTS external_color_mapping
        (
            id              INTEGER PRIMARY KEY AUTOINCREMENT,
            color_id        INTEGER NOT NULL,
            provider        TEXT NOT NULL,
            external_id     TEXT,
            mapping_status  TEXT NOT NULL DEFAULT 'Unknown'
                            CHECK(mapping_status IN ('Unknown', 'Mapped', 'Unsupported')),
            source          TEXT,
            notes           TEXT,
            created_utc     TEXT NOT NULL,
            modified_utc    TEXT NOT NULL,

            FOREIGN KEY(color_id) REFERENCES color(id),
            UNIQUE(color_id, provider)
        )
    )")) {
        qCritical() << "Unable to create external_color_mapping table:"
                    << query.lastError().text();
        return false;
    }

    if (!query.exec(R"(
        CREATE INDEX IF NOT EXISTS idx_external_color_mapping_provider_status
        ON external_color_mapping(provider, mapping_status)
    )")) {
        qCritical() << "Unable to create external color mapping index:"
                    << query.lastError().text();
        return false;
    }

    return true;
}

bool DatabaseSchema::createExternalPartMappingTable(QSqlDatabase& database)
{
    QSqlQuery query(database);

    if (!query.exec(R"(
        CREATE TABLE IF NOT EXISTS external_part_mapping
        (
            id              INTEGER PRIMARY KEY AUTOINCREMENT,
            part_id         INTEGER NOT NULL,
            provider        TEXT NOT NULL,
            external_id     TEXT,
            mapping_status  TEXT NOT NULL DEFAULT 'Unknown'
                            CHECK(mapping_status IN ('Unknown', 'Mapped', 'Unsupported')),
            source          TEXT,
            notes           TEXT,
            created_utc     TEXT NOT NULL,
            modified_utc    TEXT NOT NULL,

            FOREIGN KEY(part_id) REFERENCES part(id),
            UNIQUE(part_id, provider)
        )
    )")) {
        qCritical() << "Unable to create external_part_mapping table:"
                    << query.lastError().text();
        return false;
    }

    if (!query.exec(R"(
        CREATE INDEX IF NOT EXISTS idx_external_part_mapping_provider_status
        ON external_part_mapping(provider, mapping_status)
    )")) {
        qCritical() << "Unable to create external part mapping index:"
                    << query.lastError().text();
        return false;
    }

    return true;
}

bool DatabaseSchema::migrateVersion12ToVersion13(QSqlDatabase& database)
{
    if (!createPartRelationshipTable(database))
        return false;

    if (!createPartAliasTable(database))
        return false;

    return true;
}

bool DatabaseSchema::createPartRelationshipTable(QSqlDatabase& database)
{
    QSqlQuery query(database);

    if (!query.exec(R"(
        CREATE TABLE IF NOT EXISTS part_relationship
        (
            id                        INTEGER PRIMARY KEY AUTOINCREMENT,

            parent_part_id            INTEGER NOT NULL,
            child_part_id             INTEGER NOT NULL,

            relationship_type         TEXT NOT NULL
                                      CHECK(
                                          relationship_type IN
                                          (
                                              'Unknown',
                                              'Alternate',
                                              'Mold',
                                              'Print',
                                              'Pattern',
                                              'Subpart',
                                              'Related'
                                          )
                                      ),

            source_relationship_type  TEXT NOT NULL,
            source                    TEXT NOT NULL,

            is_active                 INTEGER NOT NULL DEFAULT 1
                                      CHECK(is_active IN (0, 1)),

            created_utc               TEXT NOT NULL,
            modified_utc              TEXT NOT NULL,

            FOREIGN KEY(parent_part_id)
                REFERENCES part(id),

            FOREIGN KEY(child_part_id)
                REFERENCES part(id),

            CHECK(parent_part_id <> child_part_id),

            UNIQUE(
                parent_part_id,
                child_part_id,
                source_relationship_type,
                source
            )
        )
    )")) {
        qCritical() << "Unable to create part_relationship table:"
                    << query.lastError().text();
        return false;
    }

    if (!query.exec(R"(
        CREATE INDEX IF NOT EXISTS idx_part_relationship_parent
        ON part_relationship(parent_part_id, is_active)
    )")) {
        qCritical() << "Unable to create part relationship parent index:"
                    << query.lastError().text();
        return false;
    }

    if (!query.exec(R"(
        CREATE INDEX IF NOT EXISTS idx_part_relationship_child
        ON part_relationship(child_part_id, is_active)
    )")) {
        qCritical() << "Unable to create part relationship child index:"
                    << query.lastError().text();
        return false;
    }

    if (!query.exec(R"(
        CREATE INDEX IF NOT EXISTS idx_part_relationship_source_type
        ON part_relationship(source, source_relationship_type, is_active)
    )")) {
        qCritical() << "Unable to create part relationship source/type index:"
                    << query.lastError().text();
        return false;
    }

    return true;
}

bool DatabaseSchema::createPartAliasTable(QSqlDatabase& database)
{
    QSqlQuery query(database);

    if (!query.exec(R"(
        CREATE TABLE IF NOT EXISTS part_alias
        (
            id                 INTEGER PRIMARY KEY AUTOINCREMENT,

            part_id            INTEGER NOT NULL,

            alias_part_number  TEXT NOT NULL COLLATE NOCASE UNIQUE,

            alias_type         TEXT NOT NULL
                               CHECK(
                                   alias_type IN
                                   (
                                       'Unknown',
                                       'PartNumberMapping',
                                       'RebrickableAlternate',
                                       'RebrickableMold',
                                       'UserConfirmed',
                                       'Legacy',
                                       'Molded'
                                   )
                               ),

            source             TEXT NOT NULL,

            is_active          INTEGER NOT NULL DEFAULT 1
                               CHECK(is_active IN (0, 1)),

            notes              TEXT,

            created_utc        TEXT NOT NULL,
            modified_utc       TEXT NOT NULL,

            FOREIGN KEY(part_id)
                REFERENCES part(id)
        )
    )")) {
        qCritical() << "Unable to create part_alias table:"
                    << query.lastError().text();
        return false;
    }

    if (!query.exec(R"(
        CREATE INDEX IF NOT EXISTS idx_part_alias_part
        ON part_alias(part_id, is_active)
    )")) {
        qCritical() << "Unable to create part alias part index:"
                    << query.lastError().text();
        return false;
    }

    if (!query.exec(R"(
        CREATE INDEX IF NOT EXISTS idx_part_alias_source_type
        ON part_alias(source, alias_type, is_active)
    )")) {
        qCritical() << "Unable to create part alias source/type index:"
                    << query.lastError().text();
        return false;
    }

    return true;
}



bool DatabaseSchema::migrateVersion13ToVersion14(QSqlDatabase& database)
{
    return createExternalPartIdentifierTable(database);
}

bool DatabaseSchema::createExternalPartIdentifierTable(QSqlDatabase& database)
{
    QSqlQuery query(database);

    if (!query.exec(R"(
        CREATE TABLE IF NOT EXISTS external_part_identifier
        (
            id           INTEGER PRIMARY KEY AUTOINCREMENT,
            part_id      INTEGER NOT NULL,
            provider     TEXT NOT NULL,
            external_id  TEXT NOT NULL COLLATE NOCASE,
            source       TEXT NOT NULL,
            is_active    INTEGER NOT NULL DEFAULT 1 CHECK(is_active IN (0, 1)),
            created_utc  TEXT NOT NULL,
            modified_utc TEXT NOT NULL,

            FOREIGN KEY(part_id) REFERENCES part(id),

            UNIQUE(part_id, provider, external_id)
        )
    )")) {
        qCritical() << "Unable to create external_part_identifier table:"
                    << query.lastError().text();
        return false;
    }

    if (!query.exec(R"(
        CREATE INDEX IF NOT EXISTS idx_external_part_identifier_lookup
        ON external_part_identifier(external_id, is_active)
    )")) {
        qCritical() << "Unable to create external part identifier lookup index:"
                    << query.lastError().text();
        return false;
    }

    return true;
}

bool DatabaseSchema::migrateVersion14ToVersion15(QSqlDatabase& database)
{
    if (!createManufacturerTable(database))
        return false;

    if (!seedManufacturers(database))
        return false;

    QSqlQuery query(database);

    //
    // SQLite only allows an added REFERENCES column to default to NULL while
    // foreign-key enforcement is enabled. M17.3.1 therefore adds the column
    // as nullable, immediately backfills every existing row to LEGO, and the
    // repository supplies LEGO for all existing workflows. M17.3.2 will
    // rebuild the inventory identity/uniqueness rule around manufacturer_id.
    //
    if (!query.exec(R"(
        ALTER TABLE inventory_record
        ADD COLUMN manufacturer_id INTEGER
        REFERENCES manufacturer(id)
    )")) {
        qCritical() << "Unable to add manufacturer_id to inventory_record:"
                    << query.lastError().text();
        return false;
    }

    int legoId = 0;

    query.prepare(R"(
        SELECT id
        FROM manufacturer
        WHERE code = 'LEGO' COLLATE NOCASE
        LIMIT 1
    )");

    if (!query.exec() || !query.next()) {
        qCritical() << "Unable to resolve seeded LEGO manufacturer:"
                    << query.lastError().text();
        return false;
    }

    legoId = query.value(0).toInt();

    query.prepare(R"(
        UPDATE inventory_record
        SET manufacturer_id = :manufacturer_id
        WHERE manufacturer_id IS NULL
    )");
    query.bindValue(":manufacturer_id", legoId);

    if (!query.exec()) {
        qCritical() << "Unable to backfill inventory manufacturer:"
                    << query.lastError().text();
        return false;
    }

    if (!query.exec(R"(
        CREATE INDEX IF NOT EXISTS idx_inventory_manufacturer
        ON inventory_record(manufacturer_id)
    )")) {
        qCritical() << "Unable to create inventory manufacturer index:"
                    << query.lastError().text();
        return false;
    }

    //
    // Existing application workflows do not expose Manufacturer until
    // M17.3.2/M17.3.3. Keep them backward-compatible by assigning LEGO
    // whenever a legacy insert omits manufacturer_id.
    //
    if (!query.exec(R"(
        CREATE TRIGGER IF NOT EXISTS trg_inventory_default_manufacturer
        AFTER INSERT ON inventory_record
        FOR EACH ROW
        WHEN NEW.manufacturer_id IS NULL
        BEGIN
            UPDATE inventory_record
            SET manufacturer_id =
                (
                    SELECT id
                    FROM manufacturer
                    WHERE code = 'LEGO' COLLATE NOCASE
                    LIMIT 1
                )
            WHERE id = NEW.id;
        END
    )")) {
        qCritical() << "Unable to create inventory default-manufacturer trigger:"
                    << query.lastError().text();
        return false;
    }

    qInfo() << "Manufacturer reference foundation initialized."
            << "Default manufacturer: LEGO";

    return true;
}

bool DatabaseSchema::createManufacturerTable(QSqlDatabase& database)
{
    QSqlQuery query(database);

    if (!query.exec(R"(
        CREATE TABLE IF NOT EXISTS manufacturer
        (
            id                         INTEGER PRIMARY KEY AUTOINCREMENT,
            code                       TEXT NOT NULL COLLATE NOCASE UNIQUE,
            name                       TEXT NOT NULL COLLATE NOCASE UNIQUE,
            website_url                TEXT,
            supports_lego_element_ids  INTEGER NOT NULL DEFAULT 0
                                       CHECK(supports_lego_element_ids IN (0, 1)),
            is_active                  INTEGER NOT NULL DEFAULT 1
                                       CHECK(is_active IN (0, 1)),
            notes                      TEXT,
            created_utc                TEXT NOT NULL,
            modified_utc               TEXT NOT NULL
        )
    )")) {
        qCritical() << "Unable to create manufacturer table:"
                    << query.lastError().text();
        return false;
    }

    if (!query.exec(R"(
        CREATE INDEX IF NOT EXISTS idx_manufacturer_active_name
        ON manufacturer(is_active, name)
    )")) {
        qCritical() << "Unable to create manufacturer index:"
                    << query.lastError().text();
        return false;
    }

    return true;
}

bool DatabaseSchema::seedManufacturers(QSqlDatabase& database)
{
    QSqlQuery query(database);

    const QString now =
        QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);

    if (!query.prepare(R"(
        INSERT INTO manufacturer
        (
            code,
            name,
            website_url,
            supports_lego_element_ids,
            is_active,
            notes,
            created_utc,
            modified_utc
        )
        VALUES
        (
            :code,
            :name,
            NULL,
            :supports_lego_element_ids,
            1,
            :notes,
            :created_utc,
            :modified_utc
        )
        ON CONFLICT(code)
        DO UPDATE SET
            name = excluded.name,
            supports_lego_element_ids = excluded.supports_lego_element_ids,
            is_active = 1,
            notes = excluded.notes,
            modified_utc = excluded.modified_utc
    )")) {
        qCritical() << "Unable to prepare manufacturer seed:"
                    << query.lastError().text();
        return false;
    }

    struct Seed
    {
        const char* code;
        const char* name;
        bool supportsLegoElementIds;
        const char* notes;
    };

    const Seed seeds[] = {
        {"LEGO", "LEGO", true, "Default manufacturer for existing and Rebrickable inventory."},
        {"NEXUS", "Nexus", false, "Compatible brick manufacturer."},
        {"MANNIDOO", "Mannidoo", false, "Compatible brick manufacturer."}
    };

    for (const Seed& seed : seeds) {
        query.bindValue(":code", QString::fromLatin1(seed.code));
        query.bindValue(":name", QString::fromLatin1(seed.name));
        query.bindValue(":supports_lego_element_ids",
                        seed.supportsLegoElementIds ? 1 : 0);
        query.bindValue(":notes", QString::fromLatin1(seed.notes));
        query.bindValue(":created_utc", now);
        query.bindValue(":modified_utc", now);

        if (!query.exec()) {
            qCritical() << "Unable to seed manufacturer:"
                        << seed.code
                        << query.lastError().text();
            return false;
        }
    }

    return true;
}

bool DatabaseSchema::migrateVersion15ToVersion16(QSqlDatabase& database)
{
    QSqlQuery pragmaQuery(database);

    bool foreignKeysWereEnabled = false;

    if (pragmaQuery.exec("PRAGMA foreign_keys") && pragmaQuery.next()) {
        foreignKeysWereEnabled = pragmaQuery.value(0).toInt() != 0;
    }

    pragmaQuery.finish();

    if (foreignKeysWereEnabled) {
        if (!pragmaQuery.exec("PRAGMA foreign_keys = OFF")) {
            qCritical() << "Unable to temporarily disable foreign keys for Version 16:"
                        << pragmaQuery.lastError().text();
            return false;
        }
        pragmaQuery.finish();
    }

    if (!database.transaction()) {
        qCritical() << "Unable to begin Version 16 inventory migration:"
                    << database.lastError().text();

        if (foreignKeysWereEnabled)
            pragmaQuery.exec("PRAGMA foreign_keys = ON");

        return false;
    }

    QSqlQuery query(database);

    if (!query.exec(R"(
        CREATE TABLE inventory_record_v16
        (
            id                  INTEGER PRIMARY KEY AUTOINCREMENT,
            workspace_id        INTEGER NOT NULL,
            part_id             INTEGER NOT NULL,
            color_id            INTEGER NOT NULL,
            storage_location_id INTEGER NOT NULL,
            manufacturer_id     INTEGER NOT NULL,

            condition           TEXT NOT NULL DEFAULT 'Used',
            ownership_type      TEXT NOT NULL DEFAULT 'Owned',

            quantity            INTEGER NOT NULL DEFAULT 0,

            created_utc         TEXT NOT NULL,
            modified_utc        TEXT NOT NULL,

            FOREIGN KEY (workspace_id)
                REFERENCES workspace(id),

            FOREIGN KEY (part_id)
                REFERENCES part(id),

            FOREIGN KEY (color_id)
                REFERENCES color(id),

            FOREIGN KEY (storage_location_id)
                REFERENCES storage_location(id),

            FOREIGN KEY (manufacturer_id)
                REFERENCES manufacturer(id),

            CHECK(quantity >= 0),

            UNIQUE
            (
                workspace_id,
                part_id,
                color_id,
                storage_location_id,
                manufacturer_id,
                condition,
                ownership_type
            )
        )
    )")) {
        qCritical() << "Unable to create Version 16 inventory_record table:"
                    << query.lastError().text();
        database.rollback();

        if (foreignKeysWereEnabled)
            pragmaQuery.exec("PRAGMA foreign_keys = ON");

        return false;
    }

    if (!query.exec(R"(
        INSERT INTO inventory_record_v16
        (
            id,
            workspace_id,
            part_id,
            color_id,
            storage_location_id,
            manufacturer_id,
            condition,
            ownership_type,
            quantity,
            created_utc,
            modified_utc
        )
        SELECT
            ir.id,
            ir.workspace_id,
            ir.part_id,
            ir.color_id,
            ir.storage_location_id,
            COALESCE(
                ir.manufacturer_id,
                (
                    SELECT id
                    FROM manufacturer
                    WHERE code = 'LEGO' COLLATE NOCASE
                    LIMIT 1
                )
            ),
            ir.condition,
            ir.ownership_type,
            ir.quantity,
            ir.created_utc,
            ir.modified_utc
        FROM inventory_record ir
    )")) {
        qCritical() << "Unable to copy inventory during Version 16 migration:"
                    << query.lastError().text();
        database.rollback();

        if (foreignKeysWereEnabled)
            pragmaQuery.exec("PRAGMA foreign_keys = ON");

        return false;
    }

    if (!query.exec("DROP TABLE inventory_record")) {
        qCritical() << "Unable to replace old inventory_record table:"
                    << query.lastError().text();
        database.rollback();

        if (foreignKeysWereEnabled)
            pragmaQuery.exec("PRAGMA foreign_keys = ON");

        return false;
    }

    if (!query.exec("ALTER TABLE inventory_record_v16 RENAME TO inventory_record")) {
        qCritical() << "Unable to rename Version 16 inventory_record table:"
                    << query.lastError().text();
        database.rollback();

        if (foreignKeysWereEnabled)
            pragmaQuery.exec("PRAGMA foreign_keys = ON");

        return false;
    }

    if (!createInventoryIndexes(database)) {
        database.rollback();

        if (foreignKeysWereEnabled)
            pragmaQuery.exec("PRAGMA foreign_keys = ON");

        return false;
    }

    if (!query.exec(R"(
        CREATE INDEX IF NOT EXISTS idx_inventory_manufacturer
        ON inventory_record(manufacturer_id)
    )")) {
        qCritical() << "Unable to recreate inventory manufacturer index:"
                    << query.lastError().text();
        database.rollback();

        if (foreignKeysWereEnabled)
            pragmaQuery.exec("PRAGMA foreign_keys = ON");

        return false;
    }

    if (!setSchemaVersion(database, 16)) {
        database.rollback();

        if (foreignKeysWereEnabled)
            pragmaQuery.exec("PRAGMA foreign_keys = ON");

        return false;
    }

    if (!database.commit()) {
        qCritical() << "Unable to commit Version 16 inventory migration:"
                    << database.lastError().text();
        database.rollback();

        if (foreignKeysWereEnabled)
            pragmaQuery.exec("PRAGMA foreign_keys = ON");

        return false;
    }

    if (foreignKeysWereEnabled) {
        if (!pragmaQuery.exec("PRAGMA foreign_keys = ON")) {
            qCritical() << "Unable to re-enable foreign keys after Version 16:"
                        << pragmaQuery.lastError().text();
            return false;
        }
    }

    if (!pragmaQuery.exec("PRAGMA foreign_key_check")) {
        qCritical() << "Unable to validate foreign keys after Version 16:"
                    << pragmaQuery.lastError().text();
        return false;
    }

    if (pragmaQuery.next()) {
        qCritical() << "Foreign-key validation failed after Version 16 inventory migration."
                    << "Table:" << pragmaQuery.value(0).toString()
                    << "RowId:" << pragmaQuery.value(1).toLongLong()
                    << "Parent:" << pragmaQuery.value(2).toString();
        return false;
    }

    qInfo() << "Inventory manufacturer identity migration completed."
            << "SchemaVersion: 16";

    return true;
}

bool DatabaseSchema::migrateVersion16ToVersion17(QSqlDatabase& database)
{
    QSqlQuery query(database);

    if (!query.exec(R"(
        ALTER TABLE build
        ADD COLUMN manufacturer_id INTEGER
        REFERENCES manufacturer(id)
    )")) {
        qCritical() << "Unable to add manufacturer_id to build:"
                    << query.lastError().text();
        return false;
    }

    query.prepare(R"(
        UPDATE build
        SET manufacturer_id =
            (
                SELECT id
                FROM manufacturer
                WHERE code = 'LEGO' COLLATE NOCASE
                LIMIT 1
            )
        WHERE manufacturer_id IS NULL
    )");

    if (!query.exec()) {
        qCritical() << "Unable to backfill Build manufacturer:"
                    << query.lastError().text();
        return false;
    }

    if (!query.exec(R"(
        CREATE INDEX IF NOT EXISTS idx_build_manufacturer
        ON build(manufacturer_id)
    )")) {
        qCritical() << "Unable to create Build manufacturer index:"
                    << query.lastError().text();
        return false;
    }

    if (!query.exec(R"(
        CREATE TRIGGER IF NOT EXISTS trg_build_default_manufacturer
        AFTER INSERT ON build
        FOR EACH ROW
        WHEN NEW.manufacturer_id IS NULL
        BEGIN
            UPDATE build
            SET manufacturer_id =
                (
                    SELECT id
                    FROM manufacturer
                    WHERE code = 'LEGO' COLLATE NOCASE
                    LIMIT 1
                )
            WHERE id = NEW.id;
        END
    )")) {
        qCritical() << "Unable to create Build default-manufacturer trigger:"
                    << query.lastError().text();
        return false;
    }

    qInfo() << "Build manufacturer migration completed."
            << "Existing Builds defaulted to LEGO.";

    return true;
}

bool DatabaseSchema::migrateVersion17ToVersion18(QSqlDatabase& database)
{
    QSqlQuery query(database);

    //
    // Allocations are reservations and disappear as pieces are physically
    // pulled. Persist manufacturer provenance separately so a Build-from-Stock
    // can later return each piece under the manufacturer it actually had.
    //
    if (!query.exec(R"(
        CREATE TABLE IF NOT EXISTS build_part_provenance
        (
            id                 INTEGER PRIMARY KEY AUTOINCREMENT,
            build_id           INTEGER NOT NULL,
            part_id            INTEGER NOT NULL,
            color_id           INTEGER NOT NULL,
            manufacturer_id    INTEGER NOT NULL,
            quantity_pulled    INTEGER NOT NULL DEFAULT 0,
            created_utc        TEXT NOT NULL,
            modified_utc       TEXT NOT NULL,

            FOREIGN KEY(build_id) REFERENCES build(id),
            FOREIGN KEY(part_id) REFERENCES part(id),
            FOREIGN KEY(color_id) REFERENCES color(id),
            FOREIGN KEY(manufacturer_id) REFERENCES manufacturer(id),

            CHECK(quantity_pulled >= 0),

            UNIQUE(build_id, part_id, color_id, manufacturer_id)
        )
    )")) {
        qCritical() << "Unable to create build_part_provenance table:"
                    << query.lastError().text();
        return false;
    }

    if (!query.exec(R"(
        CREATE INDEX IF NOT EXISTS idx_build_part_provenance_build
        ON build_part_provenance(build_id)
    )")) {
        qCritical() << "Unable to create Build provenance index:"
                    << query.lastError().text();
        return false;
    }

    qInfo() << "Build manufacturer provenance foundation initialized.";

    return true;
}

bool DatabaseSchema::migrateVersion18ToVersion19(QSqlDatabase& database)
{
    QSqlQuery query(database);

    //
    // Complete Set spare requirements remain part of the canonical box
    // contents, while quantity_released records how many boxed spare pieces
    // have already been transferred into loose inventory.
    //
    if (!query.exec(R"(
        ALTER TABLE build_requirement
        ADD COLUMN quantity_released INTEGER NOT NULL DEFAULT 0
        CHECK(quantity_released >= 0)
    )")) {
        qCritical() << "Unable to add quantity_released to build_requirement:"
                    << query.lastError().text();
        return false;
    }

    qInfo() << "Complete Set spare-release tracking initialized.";

    return true;
}



bool DatabaseSchema::migrateVersion19ToVersion20(QSqlDatabase& database)
{
    QSqlQuery query(database);

    // A successful Rebrickable enrichment may return no external IDs. Keep a
    // separate terminal lookup state so background catalog browsing does not
    // repeatedly request those parts on every visit.
    if (!query.exec(R"(
        CREATE TABLE IF NOT EXISTS external_part_identifier_lookup
        (
            part_id     INTEGER NOT NULL,
            source      TEXT NOT NULL,
            status      TEXT NOT NULL DEFAULT 'Unknown'
                        CHECK(status IN ('Unknown', 'Loaded', 'Unavailable')),
            checked_utc TEXT NOT NULL,

            PRIMARY KEY(part_id, source),
            FOREIGN KEY(part_id) REFERENCES part(id)
        )
    )")) {
        qCritical() << "Unable to create external part identifier lookup table:"
                    << query.lastError().text();
        return false;
    }

    qInfo() << "External part identifier enrichment tracking initialized.";
    return true;
}


bool DatabaseSchema::migrateVersion20ToVersion21(QSqlDatabase& database)
{
    QSqlQuery query(database);

    // Bag is a normal first-class storage location type. It is useful for
    // received procurement orders as well as general temporary or grouped
    // storage. INSERT OR IGNORE keeps the migration safe for databases where
    // the type was already seeded while creating a new database.
    query.prepare(R"(
        INSERT OR IGNORE INTO storage_location_type
        (
            name,
            description,
            is_system,
            is_active,
            sort_order
        )
        VALUES
        (
            'Bag',
            'A bag used to contain or group stored parts.',
            1,
            1,
            75
        )
    )");

    if (!query.exec()) {
        qCritical() << "Unable to add Bag storage location type:"
                    << query.lastError().text();
        return false;
    }

    qInfo() << "Bag storage location type initialized.";
    return true;
}


bool DatabaseSchema::migrateVersion21ToVersion22(QSqlDatabase& database)
{
    QSqlQuery query(database);

    // Preserve the canonical requirement identity while allowing a
    // Build-from-Stock requirement to select an alternate part and/or color.
    // NULL means "use the original requirement value".
    if (!query.exec(R"(
        ALTER TABLE build_requirement
        ADD COLUMN substitute_part_id INTEGER
        REFERENCES part(id)
    )")) {
        qCritical() << "Unable to add substitute_part_id to build_requirement:"
                    << query.lastError().text();
        return false;
    }

    if (!query.exec(R"(
        ALTER TABLE build_requirement
        ADD COLUMN substitute_color_id INTEGER
        REFERENCES color(id)
    )")) {
        qCritical() << "Unable to add substitute_color_id to build_requirement:"
                    << query.lastError().text();
        return false;
    }

    // Requirement-specific allocation ownership needs a different uniqueness
    // rule from the historical Build + inventory-record rule. Rebuild the
    // table so one inventory record can participate in two separate
    // requirements of the same Build when necessary. Existing allocations are
    // deliberately preserved with a NULL requirement id; Phase 3 will create
    // requirement-linked allocations and can safely replace legacy rows during
    // reallocation. We do not guess which requirement owns historical rows.
    if (!query.exec(R"(
        CREATE TABLE build_allocation_v22
        (
            id                    INTEGER PRIMARY KEY AUTOINCREMENT,
            build_id              INTEGER NOT NULL,
            build_requirement_id  INTEGER,
            inventory_record_id   INTEGER NOT NULL,
            part_id               INTEGER NOT NULL,
            color_id              INTEGER NOT NULL,
            storage_location_id   INTEGER NOT NULL,
            quantity_allocated    INTEGER NOT NULL,
            created_utc           TEXT NOT NULL,
            modified_utc          TEXT NOT NULL,

            FOREIGN KEY (build_id)
                REFERENCES build(id),
            FOREIGN KEY (build_requirement_id)
                REFERENCES build_requirement(id),
            FOREIGN KEY (inventory_record_id)
                REFERENCES inventory_record(id),
            FOREIGN KEY (part_id)
                REFERENCES part(id),
            FOREIGN KEY (color_id)
                REFERENCES color(id),
            FOREIGN KEY (storage_location_id)
                REFERENCES storage_location(id),

            CHECK(quantity_allocated > 0),

            UNIQUE
            (
                build_requirement_id,
                inventory_record_id
            )
        )
    )")) {
        qCritical() << "Unable to create v22 build_allocation table:"
                    << query.lastError().text();
        return false;
    }

    if (!query.exec(R"(
        INSERT INTO build_allocation_v22
        (
            id,
            build_id,
            build_requirement_id,
            inventory_record_id,
            part_id,
            color_id,
            storage_location_id,
            quantity_allocated,
            created_utc,
            modified_utc
        )
        SELECT
            id,
            build_id,
            NULL,
            inventory_record_id,
            part_id,
            color_id,
            storage_location_id,
            quantity_allocated,
            created_utc,
            modified_utc
        FROM build_allocation
    )")) {
        qCritical() << "Unable to preserve existing build allocations during v22 migration:"
                    << query.lastError().text();
        return false;
    }

    if (!query.exec(QStringLiteral("DROP TABLE build_allocation"))) {
        qCritical() << "Unable to replace legacy build_allocation table:"
                    << query.lastError().text();
        return false;
    }

    if (!query.exec(QStringLiteral(
            "ALTER TABLE build_allocation_v22 RENAME TO build_allocation"))) {
        qCritical() << "Unable to rename v22 build_allocation table:"
                    << query.lastError().text();
        return false;
    }

    if (!query.exec(R"(
        CREATE INDEX IF NOT EXISTS idx_build_allocation_build
        ON build_allocation(build_id)
    )")) {
        qCritical() << "Unable to recreate build allocation Build index:"
                    << query.lastError().text();
        return false;
    }

    if (!query.exec(R"(
        CREATE INDEX IF NOT EXISTS idx_build_allocation_inventory
        ON build_allocation(inventory_record_id)
    )")) {
        qCritical() << "Unable to recreate build allocation inventory index:"
                    << query.lastError().text();
        return false;
    }

    if (!query.exec(R"(
        CREATE INDEX IF NOT EXISTS idx_build_allocation_requirement
        ON build_allocation(build_requirement_id)
    )")) {
        qCritical() << "Unable to create build allocation requirement index:"
                    << query.lastError().text();
        return false;
    }

    qInfo() << "Build requirement substitution and allocation linkage initialized.";
    return true;
}

bool DatabaseSchema::migrateVersion22ToVersion23(QSqlDatabase& database)
{
    QSqlQuery query(database);

    //
    // Version 22 introduced requirement-specific allocation ownership, but
    // historical allocation rows were deliberately preserved with a NULL
    // build_requirement_id. Those rows still reserve physical stock, while
    // requirement-aware Phase 3 code cannot credit them to a requirement.
    //
    // Safely repair only allocations for which exactly one non-spare Build
    // Requirement in the same Build matches the allocation's physical
    // Part/Color using the requirement's CURRENT EFFECTIVE identity.
    //
    // A legacy row can coexist with a newer Phase-3 row for the same
    // requirement + inventory record. In that case the newer linked row is
    // authoritative and the old NULL row is a superseded duplicate. Remove
    // that stale legacy row first so the v22 UNIQUE
    // (build_requirement_id, inventory_record_id) rule is never violated.
    //
    // Effective identity:
    //   part  = substitute_part_id  when present, otherwise part_id
    //   color = substitute_color_id when present, otherwise color_id
    //
    // If zero or multiple requirements match, leave the allocation unresolved
    // rather than guessing ownership.
    //

    const QString matchingRequirementCount = QStringLiteral(R"(
        (
            SELECT COUNT(*)
            FROM build_requirement br
            WHERE br.build_id = ba.build_id
              AND br.is_spare = 0
              AND COALESCE(br.substitute_part_id, br.part_id) = ba.part_id
              AND COALESCE(br.substitute_color_id, br.color_id) = ba.color_id
        )
    )");

    auto countLegacyAllocations = [&](const QString& predicate, int& count) -> bool {
        QSqlQuery countQuery(database);

        countQuery.prepare(QStringLiteral(R"(
            SELECT COUNT(*)
            FROM build_allocation ba
            WHERE (ba.build_requirement_id IS NULL OR ba.build_requirement_id <= 0)
              AND %1
        )").arg(predicate));

        if (!countQuery.exec() || !countQuery.next()) {
            qCritical() << "Unable to inspect legacy Build allocations during Version 23 migration:"
                        << countQuery.lastError().text();
            return false;
        }

        count = countQuery.value(0).toInt();
        return true;
    };

    int legacyCount = 0;
    int supersededCount = 0;
    int repairableCount = 0;
    int ambiguousCount = 0;
    int unmatchedCount = 0;

    if (!countLegacyAllocations(QStringLiteral("1 = 1"), legacyCount))
        return false;

    if (!countLegacyAllocations(matchingRequirementCount + QStringLiteral(" > 1"),
                                ambiguousCount)) {
        return false;
    }

    if (!countLegacyAllocations(matchingRequirementCount + QStringLiteral(" = 0"),
                                unmatchedCount)) {
        return false;
    }

    //
    // Count legacy rows for which the uniquely matching requirement already
    // has a requirement-linked allocation using the same inventory record.
    // These are stale duplicates left behind when a pre-v23 allocation was
    // edited/reallocated after Phase 3 became requirement-aware.
    //
    if (!countLegacyAllocations(QStringLiteral(R"(
            %1 = 1
            AND EXISTS
            (
                SELECT 1
                FROM build_allocation linked
                WHERE linked.build_requirement_id =
                    (
                        SELECT br.id
                        FROM build_requirement br
                        WHERE br.build_id = ba.build_id
                          AND br.is_spare = 0
                          AND COALESCE(br.substitute_part_id, br.part_id) = ba.part_id
                          AND COALESCE(br.substitute_color_id, br.color_id) = ba.color_id
                        LIMIT 1
                    )
                  AND linked.inventory_record_id = ba.inventory_record_id
            )
        )").arg(matchingRequirementCount),
        supersededCount)) {
        return false;
    }

    //
    // Remove only the stale NULL legacy copy. The already-linked Phase-3 row
    // remains untouched and is the authoritative current reservation.
    //
    query.prepare(R"(
        DELETE FROM build_allocation
        WHERE (build_requirement_id IS NULL OR build_requirement_id <= 0)
          AND
              (
                  SELECT COUNT(*)
                  FROM build_requirement br
                  WHERE br.build_id = build_allocation.build_id
                    AND br.is_spare = 0
                    AND COALESCE(br.substitute_part_id, br.part_id)
                        = build_allocation.part_id
                    AND COALESCE(br.substitute_color_id, br.color_id)
                        = build_allocation.color_id
              ) = 1
          AND EXISTS
              (
                  SELECT 1
                  FROM build_allocation linked
                  WHERE linked.build_requirement_id =
                      (
                          SELECT br.id
                          FROM build_requirement br
                          WHERE br.build_id = build_allocation.build_id
                            AND br.is_spare = 0
                            AND COALESCE(br.substitute_part_id, br.part_id)
                                = build_allocation.part_id
                            AND COALESCE(br.substitute_color_id, br.color_id)
                                = build_allocation.color_id
                          LIMIT 1
                      )
                    AND linked.inventory_record_id =
                        build_allocation.inventory_record_id
              )
    )");

    if (!query.exec()) {
        qCritical() << "Unable to remove superseded legacy Build allocations:"
                    << query.lastError().text();
        return false;
    }

    const int removedSupersededCount = query.numRowsAffected();

    if (removedSupersededCount >= 0
        && removedSupersededCount != supersededCount) {
        qWarning() << "Superseded legacy Build allocation count differed from preflight count."
                   << "Expected:" << supersededCount
                   << "Removed:" << removedSupersededCount;
    }

    //
    // After removing superseded duplicates, every remaining uniquely matching
    // legacy row can be linked without colliding with an existing
    // requirement + inventory-record allocation.
    //
    if (!countLegacyAllocations(matchingRequirementCount + QStringLiteral(" = 1"),
                                repairableCount)) {
        return false;
    }

    query.prepare(R"(
        UPDATE build_allocation
        SET
            build_requirement_id =
                (
                    SELECT br.id
                    FROM build_requirement br
                    WHERE br.build_id = build_allocation.build_id
                      AND br.is_spare = 0
                      AND COALESCE(br.substitute_part_id, br.part_id)
                          = build_allocation.part_id
                      AND COALESCE(br.substitute_color_id, br.color_id)
                          = build_allocation.color_id
                    LIMIT 1
                ),
            modified_utc = :modified_utc
        WHERE (build_requirement_id IS NULL OR build_requirement_id <= 0)
          AND
              (
                  SELECT COUNT(*)
                  FROM build_requirement br
                  WHERE br.build_id = build_allocation.build_id
                    AND br.is_spare = 0
                    AND COALESCE(br.substitute_part_id, br.part_id)
                        = build_allocation.part_id
                    AND COALESCE(br.substitute_color_id, br.color_id)
                        = build_allocation.color_id
              ) = 1
    )");

    query.bindValue(
        ":modified_utc",
        QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));

    if (!query.exec()) {
        qCritical() << "Unable to backfill legacy Build allocation requirement ownership:"
                    << query.lastError().text();
        return false;
    }

    const int repairedCount = query.numRowsAffected();

    if (repairedCount >= 0 && repairedCount != repairableCount) {
        qWarning() << "Legacy Build allocation repair count differed from preflight count."
                   << "Expected:" << repairableCount
                   << "Affected:" << repairedCount;
    }

    qInfo() << "Legacy Build allocation requirement repair completed."
            << "Legacy:" << legacyCount
            << "Superseded:" << supersededCount
            << "RemovedSuperseded:" << removedSupersededCount
            << "Repairable:" << repairableCount
            << "Repaired:" << repairedCount
            << "Ambiguous:" << ambiguousCount
            << "Unmatched:" << unmatchedCount;

    if (ambiguousCount > 0 || unmatchedCount > 0) {
        qWarning() << "Some legacy Build allocations remain without requirement ownership."
                   << "Ambiguous:" << ambiguousCount
                   << "Unmatched:" << unmatchedCount
                   << "These rows were preserved rather than guessed.";
    }

    return true;
}

bool DatabaseSchema::migrateVersion23ToVersion24(QSqlDatabase& database)
{
    if (!createMinifigCatalogTables(database))
        return false;

    qInfo() << "Minifig Catalog foundation initialized.";
    return true;
}

bool DatabaseSchema::migrateVersion24ToVersion25(QSqlDatabase& database)
{
    if (!createThemeCatalogTables(database))
        return false;
    qInfo() << "Theme Catalog foundation initialized.";
    return true;
}
