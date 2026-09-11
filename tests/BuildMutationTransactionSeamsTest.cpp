#include "../src/models/Build.h"
#include "../src/services/builds/BuildMutationService.h"

#include <QCoreApplication>
#include <QDebug>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <cstdio>

namespace {
bool check(bool value, const char* message) { if (!value) std::fprintf(stderr, "FAILED: %s\n", message); return value; }
int scalar(QSqlDatabase db, const QString& sql) { QSqlQuery q(db); return q.exec(sql)&&q.next()?q.value(0).toInt():-1; }
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    bool ok = true;
    {
        QSqlDatabase poisoned = QSqlDatabase::addDatabase("QSQLITE");
        poisoned.setDatabaseName(":memory:");
        ok &= check(poisoned.open(), "open poisoned default");
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "build-mutation-seams");
        db.setDatabaseName(":memory:");
        ok &= check(db.open(), "open named database");
        QSqlQuery q(db);
        const QStringList schema = {
            "PRAGMA foreign_keys=ON",
            "CREATE TABLE workspace(id INTEGER PRIMARY KEY AUTOINCREMENT,name TEXT,description TEXT,is_active INTEGER,created_utc TEXT,modified_utc TEXT)",
            "CREATE TABLE manufacturer(id INTEGER PRIMARY KEY AUTOINCREMENT,code TEXT,name TEXT,website_url TEXT,supports_lego_element_ids INTEGER,is_active INTEGER,notes TEXT,created_utc TEXT,modified_utc TEXT,origin TEXT)",
            "CREATE TABLE minifig_external_identifier(id INTEGER PRIMARY KEY,minifig_catalog_id INTEGER,provider TEXT,external_id TEXT,is_active INTEGER)",
            "CREATE TABLE build(id INTEGER PRIMARY KEY AUTOINCREMENT,workspace_id INTEGER,build_type TEXT,name TEXT,set_number TEXT,set_catalog_id INTEGER,minifig_catalog_id INTEGER,inventory_mode TEXT,manufacturer_id INTEGER,status TEXT,is_active INTEGER,notes TEXT,created_utc TEXT,modified_utc TEXT)",
            "CREATE TABLE build_requirement(id INTEGER PRIMARY KEY AUTOINCREMENT,build_id INTEGER,part_id INTEGER,color_id INTEGER,substitute_part_id INTEGER,substitute_color_id INTEGER,quantity_required INTEGER,quantity_pulled INTEGER,quantity_released INTEGER,is_spare INTEGER,created_utc TEXT,modified_utc TEXT)",
            "CREATE TABLE build_allocation(id INTEGER PRIMARY KEY AUTOINCREMENT,build_id INTEGER,build_requirement_id INTEGER,inventory_record_id INTEGER,part_id INTEGER,color_id INTEGER,storage_location_id INTEGER,quantity_allocated INTEGER,created_utc TEXT,modified_utc TEXT)",
            "CREATE TABLE build_part_provenance(build_id INTEGER,part_id INTEGER,color_id INTEGER,manufacturer_id INTEGER,quantity_pulled INTEGER,created_utc TEXT,modified_utc TEXT)"
        };
        for (const QString& statement : schema)
            ok &= check(q.exec(statement), "create named schema");
        ok &= check(q.exec("INSERT INTO workspace(name,description,is_active,created_utc,modified_utc) VALUES('W','',1,'x','x')"), "seed workspace");
        const int workspaceId = q.lastInsertId().toInt();
        ok &= check(q.exec("INSERT INTO manufacturer(code,name,website_url,supports_lego_element_ids,is_active,notes,created_utc,modified_utc,origin) VALUES('LEGO','LEGO','',1,1,'','x','x','BuiltIn')"), "seed manufacturer");
        const int manufacturerId = q.lastInsertId().toInt();
        BuildMutationService service(db);
        Build seed; seed.setWorkspaceId(workspaceId); seed.setBuildType("MOC"); seed.setSetNumber("MOC-1"); seed.setName("Original"); seed.setManufacturerId(manufacturerId); seed.setInventoryMode("Stock"); seed.setStatus("Planned");

        ok &= check(db.transaction(), "begin create rollback");
        auto created = service.createInCurrentTransaction(seed);
        ok &= check(created.success && created.build.id()>0 && scalar(db,"SELECT COUNT(*) FROM build")==1, "create seam returns identity");
        ok &= check(db.rollback() && scalar(db,"SELECT COUNT(*) FROM build")==0, "create rollback");
        ok &= check(scalar(poisoned,"SELECT COUNT(*) FROM sqlite_master WHERE name='build'")==0, "default connection untouched");

        created = service.create(seed);
        ok &= check(created.success && scalar(db,"SELECT COUNT(*) FROM build")==1, "local wrapper commits");
        const int id = created.build.id();
        ok &= check(db.transaction(), "begin metadata rollback");
        auto edited = service.updateMetadataInCurrentTransaction(id,"Changed",manufacturerId,"notes");
        ok &= check(edited.success && edited.build.name()=="Changed", "metadata seam returns authoritative row");
        ok &= check(db.rollback() && scalar(db,QString("SELECT COUNT(*) FROM build WHERE id=%1 AND name='Original'").arg(id))==1, "metadata rollback");

        QSqlQuery update(db); update.exec(QString("UPDATE build SET status='Cancelled' WHERE id=%1").arg(id));
        ok &= check(db.transaction(), "begin reactivation rollback");
        auto active = service.setActiveInCurrentTransaction(id,false);
        ok &= check(active.success && !active.build.isActive(), "archive seam");
        ok &= check(db.rollback() && scalar(db,QString("SELECT is_active FROM build WHERE id=%1").arg(id))==1, "archive rollback");
        service.setActive(id,false);
        ok &= check(service.setActive(id,true).success
                    && scalar(db,QString("SELECT COUNT(*) FROM build WHERE id=%1 AND is_active=1 AND status='Planned'").arg(id))==1,
                    "Cancelled reactivation returns Planned");

        ok &= check(q.exec(QString("INSERT INTO build_requirement(build_id,part_id,color_id,quantity_required,quantity_pulled,quantity_released,is_spare,created_utc,modified_utc) VALUES(%1,1,1,2,2,0,0,'x','x')").arg(id)), "seed completion requirement");
        ok &= check(db.transaction(), "begin completion rollback");
        auto completed = service.completeInCurrentTransaction(id);
        ok &= check(completed.success && completed.build.status()=="Complete", "completion seam");
        ok &= check(db.rollback() && scalar(db,QString("SELECT COUNT(*) FROM build WHERE id=%1 AND status='Planned'").arg(id))==1, "completion rollback");

        db.close(); poisoned.close();
    }
    QSqlDatabase::removeDatabase("build-mutation-seams");
    QSqlDatabase::removeDatabase(QSqlDatabase::defaultConnection);
    return ok ? 0 : 1;
}
