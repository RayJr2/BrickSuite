#include "DatabaseSchema.h"

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

    if (!database.transaction()) {
        qCritical() << "Unable to begin schema transaction:" << database.lastError().text();

        return false;
    }

    if (!createSchemaVersionTable(database)) {
        database.rollback();
        return false;
    }

    int version = 0;

    if (!getSchemaVersion(database, version)) {
        database.rollback();
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
                    'Disassembled'
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