/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 *
 * This file is part of BrickSuite.
 */

#include "../src/database/DatabaseManager.h"
#include "../src/database/DatabaseSchema.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QMap>
#include <QSqlRecord>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QStringList>
#include <QUuid>

#include <utility>
#include <stdexcept>

namespace {

bool require(bool condition, const QString& message)
{
    if (!condition)
        qCritical().noquote() << "FAILED:" << message;
    return condition;
}

bool executeAll(QSqlDatabase& database, const QStringList& statements)
{
    for (const QString& statement : statements) {
        QSqlQuery query(database);
        if (!query.exec(statement)) {
            qCritical().noquote() << "Schema-19 fixture statement failed:"
                                  << query.lastError().text() << statement;
            return false;
        }
    }
    return true;
}

QVariant scalar(QSqlDatabase& database, const QString& statement)
{
    QSqlQuery query(database);
    if (!query.exec(statement) || !query.next()) {
        qCritical().noquote() << "Scalar query failed:" << query.lastError().text()
                              << statement;
        return {};
    }
    return query.value(0);
}

struct ConnectionCleanup
{
    QString name;
    ~ConnectionCleanup() { QSqlDatabase::removeDatabase(name); }
};

bool createSchema19Fixture(const QString& databasePath)
{
    const QString connectionName = QStringLiteral("v020-schema-19-fixture");
    const ConnectionCleanup cleanup{connectionName};
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                                           connectionName);
        database.setDatabaseName(databasePath);
        if (!database.open())
            return false;

        QSqlQuery pragma(database);
        if (!pragma.exec(QStringLiteral("PRAGMA foreign_keys = ON")))
            return false;

        // Final v0.2.0 baseline: commit c7ae00a, CurrentSchemaVersion=19.
        // Table definitions include the v7/v8/v10/v16/v17/v18/v19 additions.
        // Unused indexes and the default-Manufacturer trigger are omitted;
        // all fixture inserts specify their historical Manufacturer explicitly.
        const QStringList schema = {
            R"(CREATE TABLE schema_version(version INTEGER NOT NULL))",
            R"(CREATE TABLE workspace(id INTEGER PRIMARY KEY AUTOINCREMENT,name TEXT NOT NULL,description TEXT,created_utc TEXT NOT NULL,modified_utc TEXT NOT NULL,is_active INTEGER NOT NULL DEFAULT 1))",
            R"(CREATE TABLE color(id INTEGER PRIMARY KEY AUTOINCREMENT,name TEXT NOT NULL,rgb TEXT,is_transparent INTEGER NOT NULL DEFAULT 0,rebrickable_id INTEGER,created_utc TEXT NOT NULL,modified_utc TEXT NOT NULL,UNIQUE(rebrickable_id)))",
            R"(CREATE TABLE part_category(id INTEGER PRIMARY KEY AUTOINCREMENT,name TEXT NOT NULL,rebrickable_id INTEGER,created_utc TEXT NOT NULL,modified_utc TEXT NOT NULL,UNIQUE(rebrickable_id)))",
            R"(CREATE TABLE storage_location_type(id INTEGER PRIMARY KEY AUTOINCREMENT,name TEXT NOT NULL UNIQUE,description TEXT,is_system INTEGER NOT NULL DEFAULT 0,is_active INTEGER NOT NULL DEFAULT 1,sort_order INTEGER NOT NULL DEFAULT 0))",
            R"(CREATE TABLE storage_location(id INTEGER PRIMARY KEY AUTOINCREMENT,workspace_id INTEGER NOT NULL,parent_location_id INTEGER,location_type_id INTEGER NOT NULL,name TEXT NOT NULL,description TEXT,sort_order INTEGER NOT NULL DEFAULT 0,is_active INTEGER NOT NULL DEFAULT 1,created_utc TEXT NOT NULL,modified_utc TEXT NOT NULL,FOREIGN KEY(workspace_id) REFERENCES workspace(id),FOREIGN KEY(parent_location_id) REFERENCES storage_location(id),FOREIGN KEY(location_type_id) REFERENCES storage_location_type(id)))",
            R"(CREATE TABLE part(id INTEGER PRIMARY KEY AUTOINCREMENT,part_number TEXT NOT NULL,name TEXT NOT NULL,part_category_id INTEGER,rebrickable_part_id TEXT,is_active INTEGER NOT NULL DEFAULT 1,created_utc TEXT NOT NULL,modified_utc TEXT NOT NULL,FOREIGN KEY(part_category_id) REFERENCES part_category(id),UNIQUE(part_number),UNIQUE(rebrickable_part_id)))",
            R"(CREATE TABLE manufacturer(id INTEGER PRIMARY KEY AUTOINCREMENT,code TEXT NOT NULL COLLATE NOCASE UNIQUE,name TEXT NOT NULL COLLATE NOCASE UNIQUE,website_url TEXT,supports_lego_element_ids INTEGER NOT NULL DEFAULT 0 CHECK(supports_lego_element_ids IN (0,1)),is_active INTEGER NOT NULL DEFAULT 1 CHECK(is_active IN (0,1)),notes TEXT,created_utc TEXT NOT NULL,modified_utc TEXT NOT NULL))",
            R"(CREATE TABLE inventory_record(id INTEGER PRIMARY KEY AUTOINCREMENT,workspace_id INTEGER NOT NULL,part_id INTEGER NOT NULL,color_id INTEGER NOT NULL,storage_location_id INTEGER NOT NULL,manufacturer_id INTEGER NOT NULL,condition TEXT NOT NULL DEFAULT 'Used',ownership_type TEXT NOT NULL DEFAULT 'Owned',quantity INTEGER NOT NULL DEFAULT 0,created_utc TEXT NOT NULL,modified_utc TEXT NOT NULL,FOREIGN KEY(workspace_id) REFERENCES workspace(id),FOREIGN KEY(part_id) REFERENCES part(id),FOREIGN KEY(color_id) REFERENCES color(id),FOREIGN KEY(storage_location_id) REFERENCES storage_location(id),FOREIGN KEY(manufacturer_id) REFERENCES manufacturer(id),CHECK(quantity >= 0),UNIQUE(workspace_id,part_id,color_id,storage_location_id,manufacturer_id,condition,ownership_type)))",
            R"(CREATE TABLE inventory_movement(id INTEGER PRIMARY KEY AUTOINCREMENT,workspace_id INTEGER NOT NULL,inventory_record_id INTEGER,part_id INTEGER NOT NULL,color_id INTEGER NOT NULL,movement_type TEXT NOT NULL,quantity_change INTEGER NOT NULL,from_storage_location_id INTEGER,to_storage_location_id INTEGER,condition TEXT,ownership_type TEXT,reference_type TEXT,reference_id TEXT,notes TEXT,created_utc TEXT NOT NULL,FOREIGN KEY(workspace_id) REFERENCES workspace(id),FOREIGN KEY(inventory_record_id) REFERENCES inventory_record(id),FOREIGN KEY(part_id) REFERENCES part(id),FOREIGN KEY(color_id) REFERENCES color(id),FOREIGN KEY(from_storage_location_id) REFERENCES storage_location(id),FOREIGN KEY(to_storage_location_id) REFERENCES storage_location(id)))",
            R"(CREATE TABLE build(id INTEGER PRIMARY KEY AUTOINCREMENT,workspace_id INTEGER NOT NULL,build_type TEXT NOT NULL CHECK(build_type IN ('Set','MOC')),name TEXT NOT NULL,set_number TEXT,inventory_mode TEXT NOT NULL DEFAULT 'Stock' CHECK(inventory_mode IN ('Stock','CompleteSet')),status TEXT NOT NULL DEFAULT 'Planned' CHECK(status IN ('Planned','Pulling','Complete','Disassembled','Cancelled')),is_active INTEGER NOT NULL DEFAULT 1,notes TEXT,created_utc TEXT NOT NULL,modified_utc TEXT NOT NULL,manufacturer_id INTEGER REFERENCES manufacturer(id),FOREIGN KEY(workspace_id) REFERENCES workspace(id)))",
            R"(CREATE TABLE build_requirement(id INTEGER PRIMARY KEY AUTOINCREMENT,build_id INTEGER NOT NULL,part_id INTEGER NOT NULL,color_id INTEGER NOT NULL,quantity_required INTEGER NOT NULL,is_spare INTEGER NOT NULL DEFAULT 0,quantity_pulled INTEGER NOT NULL DEFAULT 0 CHECK(quantity_pulled >= 0),quantity_released INTEGER NOT NULL DEFAULT 0 CHECK(quantity_released >= 0),created_utc TEXT NOT NULL,modified_utc TEXT NOT NULL,FOREIGN KEY(build_id) REFERENCES build(id),FOREIGN KEY(part_id) REFERENCES part(id),FOREIGN KEY(color_id) REFERENCES color(id),CHECK(quantity_required > 0),UNIQUE(build_id,part_id,color_id,is_spare)))",
            R"(CREATE TABLE build_allocation(id INTEGER PRIMARY KEY AUTOINCREMENT,build_id INTEGER NOT NULL,inventory_record_id INTEGER NOT NULL,part_id INTEGER NOT NULL,color_id INTEGER NOT NULL,storage_location_id INTEGER NOT NULL,quantity_allocated INTEGER NOT NULL,created_utc TEXT NOT NULL,modified_utc TEXT NOT NULL,FOREIGN KEY(build_id) REFERENCES build(id),FOREIGN KEY(inventory_record_id) REFERENCES inventory_record(id),FOREIGN KEY(part_id) REFERENCES part(id),FOREIGN KEY(color_id) REFERENCES color(id),FOREIGN KEY(storage_location_id) REFERENCES storage_location(id),CHECK(quantity_allocated > 0),UNIQUE(build_id,inventory_record_id)))",
            R"(CREATE TABLE build_part_provenance(id INTEGER PRIMARY KEY AUTOINCREMENT,build_id INTEGER NOT NULL,part_id INTEGER NOT NULL,color_id INTEGER NOT NULL,manufacturer_id INTEGER NOT NULL,quantity_pulled INTEGER NOT NULL DEFAULT 0,created_utc TEXT NOT NULL,modified_utc TEXT NOT NULL,FOREIGN KEY(build_id) REFERENCES build(id),FOREIGN KEY(part_id) REFERENCES part(id),FOREIGN KEY(color_id) REFERENCES color(id),FOREIGN KEY(manufacturer_id) REFERENCES manufacturer(id),CHECK(quantity_pulled >= 0),UNIQUE(build_id,part_id,color_id,manufacturer_id)))",
            R"(CREATE TABLE set_catalog(id INTEGER PRIMARY KEY AUTOINCREMENT,set_number TEXT NOT NULL UNIQUE,name TEXT NOT NULL,year INTEGER NOT NULL DEFAULT 0,theme_id INTEGER NOT NULL DEFAULT 0,num_parts INTEGER NOT NULL DEFAULT 0,image_url TEXT,created_utc TEXT NOT NULL,modified_utc TEXT NOT NULL,CHECK(year >= 0),CHECK(theme_id >= 0),CHECK(num_parts >= 0)))",
            R"(CREATE TABLE external_color_mapping(id INTEGER PRIMARY KEY AUTOINCREMENT,color_id INTEGER NOT NULL,provider TEXT NOT NULL,external_id TEXT,mapping_status TEXT NOT NULL DEFAULT 'Unknown' CHECK(mapping_status IN ('Unknown','Mapped','Unsupported')),source TEXT,notes TEXT,created_utc TEXT NOT NULL,modified_utc TEXT NOT NULL,FOREIGN KEY(color_id) REFERENCES color(id),UNIQUE(color_id,provider)))",
            R"(CREATE TABLE external_part_mapping(id INTEGER PRIMARY KEY AUTOINCREMENT,part_id INTEGER NOT NULL,provider TEXT NOT NULL,external_id TEXT,mapping_status TEXT NOT NULL DEFAULT 'Unknown' CHECK(mapping_status IN ('Unknown','Mapped','Unsupported')),source TEXT,notes TEXT,created_utc TEXT NOT NULL,modified_utc TEXT NOT NULL,FOREIGN KEY(part_id) REFERENCES part(id),UNIQUE(part_id,provider)))",
            R"(CREATE TABLE part_relationship(id INTEGER PRIMARY KEY AUTOINCREMENT,parent_part_id INTEGER NOT NULL,child_part_id INTEGER NOT NULL,relationship_type TEXT NOT NULL CHECK(relationship_type IN ('Unknown','Alternate','Mold','Print','Pattern','Subpart','Related')),source_relationship_type TEXT NOT NULL,source TEXT NOT NULL,is_active INTEGER NOT NULL DEFAULT 1 CHECK(is_active IN (0,1)),created_utc TEXT NOT NULL,modified_utc TEXT NOT NULL,FOREIGN KEY(parent_part_id) REFERENCES part(id),FOREIGN KEY(child_part_id) REFERENCES part(id),CHECK(parent_part_id <> child_part_id),UNIQUE(parent_part_id,child_part_id,source_relationship_type,source)))",
            R"(CREATE TABLE part_alias(id INTEGER PRIMARY KEY AUTOINCREMENT,part_id INTEGER NOT NULL,alias_part_number TEXT NOT NULL COLLATE NOCASE UNIQUE,alias_type TEXT NOT NULL CHECK(alias_type IN ('Unknown','PartNumberMapping','RebrickableAlternate','RebrickableMold','UserConfirmed','Legacy','Molded')),source TEXT NOT NULL,is_active INTEGER NOT NULL DEFAULT 1 CHECK(is_active IN (0,1)),notes TEXT,created_utc TEXT NOT NULL,modified_utc TEXT NOT NULL,FOREIGN KEY(part_id) REFERENCES part(id)))",
            R"(CREATE TABLE external_part_identifier(id INTEGER PRIMARY KEY AUTOINCREMENT,part_id INTEGER NOT NULL,provider TEXT NOT NULL,external_id TEXT NOT NULL COLLATE NOCASE,source TEXT NOT NULL,is_active INTEGER NOT NULL DEFAULT 1 CHECK(is_active IN (0,1)),created_utc TEXT NOT NULL,modified_utc TEXT NOT NULL,FOREIGN KEY(part_id) REFERENCES part(id),UNIQUE(part_id,provider,external_id)))"
        };

        if (!executeAll(database, schema))
            return false;

        const QString t = QStringLiteral("'2026-08-25T12:00:00.000Z'");
        const QStringList data = {
            QStringLiteral("INSERT INTO schema_version VALUES(19)"),
            QStringLiteral("INSERT INTO workspace VALUES(7,'Workshop','v0.2 workspace',%1,%1,1)").arg(t),
            QStringLiteral("INSERT INTO storage_location_type VALUES(2,'Shelf','Shelf',1,1,30)"),
            QStringLiteral("INSERT INTO storage_location VALUES(10,7,NULL,2,'Room','Parent',0,1,%1,%1)").arg(t),
            QStringLiteral("INSERT INTO storage_location VALUES(11,7,10,2,'Parts Shelf','Leaf',0,1,%1,%1)").arg(t),
            QStringLiteral("INSERT INTO part_category VALUES(20,'Bricks',11,%1,%1)").arg(t),
            QStringLiteral("INSERT INTO color VALUES(5,'Red','C91A09',0,4,%1,%1)").arg(t),
            QStringLiteral("INSERT INTO color VALUES(6,'Blue','0055BF',0,1,%1,%1)").arg(t),
            QStringLiteral("INSERT INTO part VALUES(100,'3001','Brick 2 x 4',20,'3001',1,%1,%1)").arg(t),
            QStringLiteral("INSERT INTO part VALUES(101,'3002','Brick 2 x 3',20,'3002',1,%1,%1)").arg(t),
            QStringLiteral("INSERT INTO manufacturer VALUES(3,'LEGO','LEGO',NULL,1,1,'System',%1,%1)").arg(t),
            QStringLiteral("INSERT INTO manufacturer VALUES(4,'CUSTOM','Custom Maker',NULL,0,1,'User',%1,%1)").arg(t),
            QStringLiteral("INSERT INTO inventory_record VALUES(200,7,100,5,11,3,'Used','Owned',12,%1,%1)").arg(t),
            QStringLiteral("INSERT INTO inventory_record VALUES(201,7,101,6,11,4,'New','Owned',3,%1,%1)").arg(t),
            QStringLiteral("INSERT INTO inventory_movement VALUES(300,7,200,100,5,'Add',12,NULL,11,'Used','Owned','Import','v020','Initial quantity',%1)").arg(t),
            QStringLiteral("INSERT INTO inventory_movement VALUES(301,7,201,101,6,'BuildPull',-2,11,NULL,'New','Owned','Build','401','Pulled for MOC',%1)").arg(t),
            QStringLiteral("INSERT INTO build VALUES(400,7,'Set','Planned Set','1000-1','Stock','Planned',1,'set notes',%1,%1,3)").arg(t),
            QStringLiteral("INSERT INTO build VALUES(401,7,'MOC','Completed MOC','MOC-7','Stock','Complete',1,'moc notes',%1,%1,4)").arg(t),
            QStringLiteral("INSERT INTO build VALUES(402,7,'Set','Disassembled Set','2000-1','CompleteSet','Disassembled',0,'historical',%1,%1,3)").arg(t),
            QStringLiteral("INSERT INTO build_requirement VALUES(500,400,100,5,4,0,0,0,%1,%1)").arg(t),
            QStringLiteral("INSERT INTO build_requirement VALUES(501,401,101,6,2,0,2,0,%1,%1)").arg(t),
            QStringLiteral("INSERT INTO build_requirement VALUES(502,402,100,5,5,0,5,0,%1,%1)").arg(t),
            QStringLiteral("INSERT INTO build_requirement VALUES(503,402,101,6,1,1,0,1,%1,%1)").arg(t),
            QStringLiteral("INSERT INTO build_allocation VALUES(600,400,200,100,5,11,4,%1,%1)").arg(t),
            QStringLiteral("INSERT INTO build_allocation VALUES(601,401,200,100,5,11,1,%1,%1)").arg(t),
            QStringLiteral("INSERT INTO build_part_provenance VALUES(700,401,101,6,4,2,%1,%1)").arg(t),
            QStringLiteral("INSERT INTO set_catalog VALUES(800,'1000-1','Catalog Set',2020,1,4,NULL,%1,%1)").arg(t),
            QStringLiteral("INSERT INTO external_color_mapping VALUES(900,5,'BrickLink','5','Mapped','v020',NULL,%1,%1)").arg(t),
            QStringLiteral("INSERT INTO external_part_mapping VALUES(901,100,'BrickLink','3001','Mapped','v020',NULL,%1,%1)").arg(t),
            QStringLiteral("INSERT INTO external_part_identifier VALUES(902,100,'BrickLink','3001','v020',1,%1,%1)").arg(t)
        };

        if (!executeAll(database, data))
            return false;
        database.close();
    }
    return true;
}

using Rows = QList<QVariantList>;
using Snapshot = QMap<QString, Rows>;

Rows readRows(QSqlDatabase& database, const QString& sql)
{
    Rows rows;
    QSqlQuery query(database);
    if (!query.exec(sql)) {
        throw std::runtime_error(query.lastError().text().toStdString());
    }
    while (query.next()) {
        QVariantList row;
        for (int column = 0; column < query.record().count(); ++column)
            row.append(query.value(column));
        rows.append(row);
    }
    return rows;
}

Snapshot captureHistoricalRows(QSqlDatabase& database)
{
    Snapshot snapshot;
    const QStringList tables = {"workspace", "color", "part_category", "storage_location",
        "part", "manufacturer", "inventory_record", "inventory_movement", "build",
        "build_requirement", "build_allocation", "build_part_provenance", "set_catalog",
        "external_color_mapping", "external_part_mapping", "external_part_identifier"};
    for (const QString& table : tables) {
        QStringList columns;
        QSqlQuery info(database);
        if (!info.exec("PRAGMA table_info(" + table + ")"))
            throw std::runtime_error("Unable to inspect fixture table");
        while (info.next()) {
            const QString column = info.value("name").toString();
            // v23 intentionally timestamps repaired allocation ownership.
            // BrickSuite-owned Manufacturer seeds intentionally refresh their
            // notes and modified timestamp during normal initialization.
            const bool refreshedSeedField = table == "manufacturer"
                                            && (column == "notes" || column == "modified_utc");
            if ((table != "build_allocation" || column != "modified_utc")
                && !refreshedSeedField)
                columns.append(column);
        }
        const QString sql = "SELECT " + columns.join(',') + " FROM " + table + " ORDER BY id";
        snapshot.insert(sql, readRows(database, sql));
    }
    return snapshot;
}

bool verifyMigratedData(QSqlDatabase database, const Snapshot& snapshot)
{
    bool ok = true;
    for (auto it = snapshot.cbegin(); it != snapshot.cend(); ++it)
        ok &= require(readRows(database, it.key()) == it.value(), "historical rows preserved: " + it.key());
    ok &= require(scalar(database, "SELECT version FROM schema_version").toInt() == 31,
                  "schema migrated from 19 to 31");
    ok &= require(scalar(database, "SELECT name FROM workspace WHERE id=7").toString()
                      == QStringLiteral("Workshop"), "workspace identity preserved");
    ok &= require(scalar(database, "SELECT parent_location_id FROM storage_location WHERE id=11").toInt() == 10,
                  "storage hierarchy preserved");
    ok &= require(scalar(database, "SELECT allows_inventory FROM storage_location WHERE id=11").toInt() == 1
                  && scalar(database, "SELECT allows_collection FROM storage_location WHERE id=11").toInt() == 0,
                  "legacy Storage defaults to Inventory-only");
    ok &= require(scalar(database, "SELECT quantity FROM inventory_record WHERE id=200").toInt() == 12
                  && scalar(database, "SELECT manufacturer_id FROM inventory_record WHERE id=201").toInt() == 4,
                  "inventory identity, quantity, and Manufacturer preserved");
    ok &= require(scalar(database, "SELECT COUNT(*) FROM inventory_movement WHERE id IN (300,301)").toInt() == 2,
                  "movement history preserved");
    ok &= require(scalar(database, "SELECT COUNT(*) FROM build WHERE id IN (400,401,402)").toInt() == 3,
                  "Set and MOC Build identities preserved");
    ok &= require(scalar(database, "SELECT status||'|'||inventory_mode||'|'||set_number FROM build WHERE id=402").toString()
                      == QStringLiteral("Disassembled|CompleteSet|2000-1"),
                  "historical Build state preserved");
    ok &= require(scalar(database, "SELECT COUNT(*) FROM build_requirement WHERE id BETWEEN 500 AND 503").toInt() == 4
                  && scalar(database, "SELECT quantity_pulled FROM build_requirement WHERE id=501").toInt() == 2
                  && scalar(database, "SELECT quantity_released FROM build_requirement WHERE id=503").toInt() == 1,
                  "requirements, pulling, and spare release preserved");
    ok &= require(scalar(database, "SELECT build_requirement_id FROM build_allocation WHERE id=600").toInt() == 500,
                  "uniquely matching legacy allocation repaired");
    ok &= require(scalar(database, "SELECT build_requirement_id IS NULL FROM build_allocation WHERE id=601").toInt() == 1,
                  "unmatched allocation preserved without guessed ownership");
    ok &= require(scalar(database, "SELECT quantity_pulled FROM build_part_provenance WHERE id=700").toInt() == 2
                  && scalar(database, "SELECT manufacturer_id FROM build_part_provenance WHERE id=700").toInt() == 4,
                  "pulled Manufacturer provenance preserved");
    ok &= require(scalar(database, "SELECT minifig_catalog_id IS NULL AND set_catalog_id IS NULL FROM build WHERE id=400").toInt() == 1,
                  "historical Builds are not assigned guessed catalog identities");
    ok &= require(scalar(database, "SELECT origin FROM manufacturer WHERE id=3").toString() == QStringLiteral("BrickSuite")
                  && scalar(database, "SELECT origin FROM manufacturer WHERE id=4").toString() == QStringLiteral("User"),
                  "Manufacturer origins migrated conservatively");
    ok &= require(scalar(database, "SELECT COUNT(*) FROM collection_item").toInt() == 0,
                  "migration creates no Collection items");

    QSqlQuery integrity(database);
    bool healthy = integrity.exec("PRAGMA integrity_check");
    int integrityRows = 0;
    while (integrity.next()) {
        ++integrityRows;
        healthy &= integrity.value(0).toString().compare("ok", Qt::CaseInsensitive) == 0;
    }
    ok &= require(healthy && integrityRows == 1 && !integrity.lastError().isValid(),
                  "full integrity_check is healthy");
    QSqlQuery foreignKeys(database);
    bool fkHealthy = foreignKeys.exec("PRAGMA foreign_key_check");
    int violations = 0;
    while (foreignKeys.next()) ++violations;
    ok &= require(fkHealthy && violations == 0 && !foreignKeys.lastError().isValid(),
                  "foreign_key_check reports no violations");
    return ok;
}

class TestDataCleanup
{
public:
    explicit TestDataCleanup(QString path) : m_path(std::move(path)) {}
    ~TestDataCleanup()
    {
        DatabaseManager::instance().close();
        QDir(m_path).removeRecursively();
    }
private:
    QString m_path;
};

} // namespace

bool runScenario(bool forceLateFailure)
{
    QCoreApplication::setApplicationName("BrickSuiteV020ToV030MigrationTest-"
                                        + QUuid::createUuid().toString(QUuid::WithoutBraces));
    const QString dataPath = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    TestDataCleanup cleanup(dataPath);
    if (!require(QDir().mkpath(dataPath), "create isolated test data directory"))
        return false;

    DatabaseManager& manager = DatabaseManager::instance();
    if (!require(createSchema19Fixture(manager.databasePath()),
                 "create representative v0.2.0 schema-19 fixture"))
        return false;
    Snapshot snapshot;
    {
        const QString name = "migration-preflight";
        const ConnectionCleanup connectionCleanup{name};
        QSqlDatabase database = QSqlDatabase::addDatabase("QSQLITE", name);
        database.setDatabaseName(manager.databasePath());
        if (!database.open()) return false;
        snapshot = captureHistoricalRows(database);
        if (forceLateFailure && !executeAll(database, {
                "CREATE TRIGGER force_late_migration_failure BEFORE INSERT ON schema_version "
                "WHEN NEW.version=31 BEGIN SELECT RAISE(ABORT,'forced late migration failure'); END"}))
            return false;
        database.close();
    }
    if (forceLateFailure) {
        if (!require(!manager.initialize(), "forced late migration reports failure")) return false;
        QSqlDatabase database = manager.database();
        // The normal chain commits 19->26 and the v27 table rebuild separately.
        // A later failure must retain that valid checkpoint, never claim v31.
        if (!require(scalar(database, "SELECT version FROM schema_version").toInt() == 27
                     && scalar(database, "SELECT COUNT(*) FROM sqlite_master WHERE name='collection_item'").toInt() == 0,
                     "late failure rolls back 28->31 and retains the valid v27 checkpoint")) return false;
        if (!executeAll(database, {"DROP TRIGGER force_late_migration_failure"})) return false;
        manager.close();
    }
    if (!require(manager.initialize(), "normal DatabaseManager migration succeeds"))
        return false;
    if (!verifyMigratedData(manager.database(), snapshot))
        return false;

    manager.close();
    if (!require(manager.initialize(), "schema-31 database reopens through normal initialization"))
        return false;
    if (!verifyMigratedData(manager.database(), snapshot))
        return false;
    return true;
}

int main(int argc, char* argv[])
{
    QCoreApplication application(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("RFStateSideMigrationTest"));
    QStandardPaths::setTestModeEnabled(true);
    try {
        if (!runScenario(false) || !runScenario(true))
            return 1;
    } catch (const std::exception& error) {
        qCritical() << "Migration test failed:" << error.what();
        return 1;
    }

    qInfo() << "v0.2.0 schema 19 to v0.3.0 schema 31 migration validation passed.";
    return 0;
}
