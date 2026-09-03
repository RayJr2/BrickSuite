#include "../src/database/DatabaseManager.h"
#include "../src/database/DatabaseSchema.h"
#include "../src/repositories/ManufacturerRepository.h"
#include "../src/services/reference/ManufacturerManagementService.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QUuid>

namespace {
bool require(bool ok, const QString& message) { if (!ok) qCritical().noquote() << message; return ok; }
int scalar(QSqlDatabase db, const QString& sql) { QSqlQuery q(db); return q.exec(sql) && q.next() ? q.value(0).toInt() : -1; }
class Cleanup { public: explicit Cleanup(QString p):path(std::move(p)){} ~Cleanup(){DatabaseManager::instance().close();QDir(path).removeRecursively();} QString path; };

bool migrationTest(const QString& path)
{
    const QString name = "manufacturer_v28_" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    bool ok = false;
    {
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", name); db.setDatabaseName(path);
        if (db.open()) {
            QSqlQuery q(db); const QString table = "CREATE TABLE manufacturer(id INTEGER PRIMARY KEY AUTOINCREMENT,code TEXT NOT NULL COLLATE NOCASE UNIQUE,name TEXT NOT NULL COLLATE NOCASE UNIQUE,website_url TEXT,supports_lego_element_ids INTEGER NOT NULL DEFAULT 0,is_active INTEGER NOT NULL DEFAULT 1,notes TEXT,created_utc TEXT NOT NULL,modified_utc TEXT NOT NULL)";
            ok = q.exec("PRAGMA foreign_keys=ON") && q.exec("CREATE TABLE schema_version(version INTEGER NOT NULL)")
                 && q.exec("INSERT INTO schema_version VALUES(28)") && q.exec(table)
                 && q.exec("CREATE TABLE workspace(id INTEGER PRIMARY KEY)")
                 && q.exec("CREATE TABLE storage_location(id INTEGER PRIMARY KEY,workspace_id INTEGER NOT NULL)")
                 && q.exec("CREATE TABLE set_catalog(id INTEGER PRIMARY KEY)")
                 && q.exec("CREATE TABLE minifig_catalog(id INTEGER PRIMARY KEY)")
                 && q.exec("CREATE TABLE build(id INTEGER PRIMARY KEY,workspace_id INTEGER NOT NULL)")
                 && q.exec("INSERT INTO manufacturer VALUES(5,'LEGO','LEGO','https://lego.example',1,1,'seed','old','old')")
                 && q.exec("INSERT INTO manufacturer VALUES(9,'OTHER','Other','https://other.example',0,0,'keep','old','old')")
                 && q.exec("CREATE TABLE inventory_record(id INTEGER PRIMARY KEY,manufacturer_id INTEGER REFERENCES manufacturer(id))")
                 && q.exec("INSERT INTO inventory_record VALUES(1,9)") && DatabaseSchema::initialize(db)
                 && scalar(db,"SELECT version FROM schema_version")==30
                 && scalar(db,"SELECT COUNT(*) FROM manufacturer WHERE id=5 AND origin='BrickSuite' AND is_active=1")==1
                 && scalar(db,"SELECT COUNT(*) FROM manufacturer WHERE id=9 AND origin='User' AND code='OTHER' AND name='Other' AND website_url='https://other.example' AND is_active=0 AND notes='keep' AND created_utc='old' AND modified_utc='old'")==1
                 && scalar(db,"SELECT manufacturer_id FROM inventory_record WHERE id=1")==9
                 && q.exec("PRAGMA foreign_key_check") && !q.next();
            db.close();
        }
    }
    QSqlDatabase::removeDatabase(name); return ok;
}
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv); app.setOrganizationName("RFStateSideTests"); app.setApplicationName("ManufacturerManagement"); QStandardPaths::setTestModeEnabled(true);
    QTemporaryDir files; if (!require(files.isValid() && migrationTest(files.filePath("v28.db")), "Schema 28 to 29 migration failed.")) return 1;
    const QString appData=QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation); QDir(appData).removeRecursively(); Cleanup cleanup(appData);
    if (!require(DatabaseManager::instance().initialize(), "Database initialization failed.")) return 1;
    QSqlDatabase db=DatabaseManager::instance().database(); ManufacturerManagementService service; ManufacturerRepository repository;
    const auto lego=repository.getByCode("lego");
    if (!require(lego && lego->origin()=="BrickSuite", "Known seed origin failed.")) return 1;
    Manufacturer value; value.setCode(" custom "); value.setName("  Custom Bricks  "); value.setNotes("first");
    auto result=service.create(value); if (!require(result.success, "User Manufacturer create failed: "+result.message)) return 1;
    const int id=result.manufacturerId; auto saved=repository.getById(id);
    if (!require(saved && saved->code()=="CUSTOM" && saved->name()=="Custom Bricks" && saved->origin()=="User", "Normalization/origin failed.")) return 1;
    Manufacturer invalid; invalid.setCode(" "); invalid.setName("Name");
    if (!require(service.create(invalid).error==ManufacturerManagementService::Error::InvalidInput, "Empty input accepted.")) return 1;
    Manufacturer duplicate; duplicate.setCode("custom"); duplicate.setName("Different");
    if (!require(service.create(duplicate).error==ManufacturerManagementService::Error::DuplicateCode, "Duplicate code accepted.")) return 1;
    duplicate.setCode("different"); duplicate.setName("CUSTOM BRICKS");
    if (!require(service.create(duplicate).error==ManufacturerManagementService::Error::DuplicateName, "Duplicate name accepted.")) return 1;
    saved->setName("Updated Name"); saved->setCode(" updated ");
    if (!require(service.edit(*saved).success && repository.getById(id)->code()=="UPDATED", "Edit failed.")) return 1;
    if (!require(service.setActive(id,false).success && !repository.getById(id)->isActive(), "Deactivation failed.")) return 1;
    duplicate.setCode("UPDATED"); duplicate.setName("Unused");
    if (!require(service.create(duplicate).error==ManufacturerManagementService::Error::DuplicateCode, "Inactive uniqueness failed.")) return 1;
    if (!require(service.setActive(id,true).success, "Reactivation failed.")) return 1;
    Manufacturer protectedEdit=*lego; protectedEdit.setName("Changed");
    if (!require(service.edit(protectedEdit).error==ManufacturerManagementService::Error::ProtectedOperation
                 && service.setActive(lego->id(),false).error==ManufacturerManagementService::Error::ProtectedOperation, "System protection failed.")) return 1;
    QSqlQuery q(db); const QString now="2026-01-01T00:00:00.000Z";
    if (!require(q.exec("INSERT INTO workspace(name,description,created_utc,modified_utc,is_active) VALUES('W','', '"+now+"','"+now+"',1)"),"Workspace fixture failed.")) return 1; int workspace=q.lastInsertId().toInt();
    q.exec("INSERT INTO part(part_number,name,is_active,created_utc,modified_utc) VALUES('p','P',1,'"+now+"','"+now+"')"); int part=q.lastInsertId().toInt();
    q.exec("INSERT INTO color(name,created_utc,modified_utc) VALUES('C','"+now+"','"+now+"')"); int color=q.lastInsertId().toInt();
    int type=scalar(db,"SELECT id FROM storage_location_type LIMIT 1");
    q.exec(QString("INSERT INTO storage_location(workspace_id,location_type_id,name,is_active,created_utc,modified_utc) VALUES(%1,%2,'Bin',1,'%3','%3')").arg(workspace).arg(type).arg(now)); int location=q.lastInsertId().toInt();
    q.exec(QString("INSERT INTO inventory_record(workspace_id,part_id,color_id,storage_location_id,condition,ownership_type,quantity,created_utc,modified_utc,manufacturer_id) VALUES(%1,%2,%3,%4,'Used','Owned',12,'%5','%5',%6)").arg(workspace).arg(part).arg(color).arg(location).arg(now).arg(id));
    q.exec(QString("INSERT INTO build(workspace_id,build_type,name,inventory_mode,status,is_active,created_utc,modified_utc,manufacturer_id) VALUES(%1,'Set','B','CompleteSet','Planned',1,'%2','%2',%3)").arg(workspace).arg(now).arg(id)); int build=q.lastInsertId().toInt();
    q.exec(QString("INSERT INTO build_part_provenance(build_id,part_id,color_id,manufacturer_id,quantity_pulled,created_utc,modified_utc) VALUES(%1,%2,%3,%4,4,'%5','%5')").arg(build).arg(part).arg(color).arg(id).arg(now));
    auto usage=service.usage(id); if (!require(usage.success && usage.usage.inventoryRecordCount==1 && usage.usage.inventoryPieceQuantity==12 && usage.usage.buildCount==1 && usage.usage.provenanceCount==1 && usage.usage.provenancePieceQuantity==4, "Usage totals failed.")) return 1;
    if (!require(service.setActive(id,false).success && repository.getById(id).has_value(), "Referenced inactive Manufacturer became unreadable.")) return 1;
    if (!require(DatabaseSchema::seedManufacturers(db) && repository.getById(id)->name()=="Updated Name", "User row changed during reseed.")) return 1;
    q.exec("DELETE FROM manufacturer WHERE code='NEXUS'"); Manufacturer collision; collision.setCode("NEXUS"); collision.setName("User Nexus"); if(!require(service.create(collision).success,"Collision fixture failed."))return 1;
    q.exec("UPDATE manufacturer SET name='Preflight Preserved' WHERE code='LEGO'");
    if (!require(!DatabaseSchema::seedManufacturers(db) && repository.getByCode("NEXUS")->name()=="User Nexus" && repository.getByCode("NEXUS")->origin()=="User" && repository.getByCode("LEGO")->name()=="Preflight Preserved", "Seed collision overwrote a User row or partially updated system rows.")) return 1;
    q.exec("CREATE TRIGGER fail_manufacturer BEFORE INSERT ON manufacturer BEGIN SELECT RAISE(ABORT,'forced'); END"); Manufacturer failed; failed.setCode("FAIL"); failed.setName("Failure"); if(!require(service.create(failed).error==ManufacturerManagementService::Error::DatabaseFailure && !repository.getByCode("FAIL"),"Forced failure did not roll back."))return 1; q.exec("DROP TRIGGER fail_manufacturer");
    q.exec("DROP TABLE build_part_provenance"); if(!require(!service.usage(id).success,"Usage query failure was reported as zero."))return 1;
    qInfo()<<"M23.11.1 Manufacturer Management validation passed."; return 0;
}
