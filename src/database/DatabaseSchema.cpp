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
                                  {"Case", "A portable or fixed storage case.", 30},
                                  {"Drawer", "A drawer within a storage unit.", 40},
                                  {"Bin", "A removable or fixed storage bin.", 50},
                                  {"Tray", "A storage tray.", 60},
                                  {"Compartment", "A compartment within another location.", 70},
                                  {"Divider", "A divided section of another location.", 80}};

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
