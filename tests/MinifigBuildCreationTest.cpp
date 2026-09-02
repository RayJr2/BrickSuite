#include "../src/database/DatabaseManager.h"
#include "../src/database/DatabaseSchema.h"
#include "../src/models/Build.h"
#include "../src/models/BuildRequirement.h"
#include "../src/repositories/BuildRepository.h"
#include "../src/repositories/BuildRequirementRepository.h"
#include "../src/services/builds/MinifigBuildCreationService.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QUuid>
#include <utility>

namespace {
bool require(bool ok, const QString& message) { if (!ok) qCritical().noquote() << message; return ok; }
class Cleanup { public: explicit Cleanup(QString p): path(std::move(p)) {} ~Cleanup(){ DatabaseManager::instance().close(); QDir(path).removeRecursively(); } QString path; };
int scalar(QSqlDatabase db, const QString& sql) { QSqlQuery q(db); return q.exec(sql) && q.next() ? q.value(0).toInt() : -1; }
bool validateV26Migration(const QString& path)
{
    const QString connection = "M2374_" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    bool ok = false;
    {
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", connection);
        db.setDatabaseName(path);
        if (db.open()) {
            QSqlQuery q(db);
            ok = q.exec("PRAGMA foreign_keys=ON")
                 && q.exec("CREATE TABLE schema_version(version INTEGER NOT NULL)")
                 && q.exec("INSERT INTO schema_version VALUES(26)")
                 && q.exec("CREATE TABLE workspace(id INTEGER PRIMARY KEY)")
                 && q.exec("INSERT INTO workspace VALUES(1)")
                 && q.exec("CREATE TABLE manufacturer(id INTEGER PRIMARY KEY,code TEXT)")
                 && q.exec("CREATE TABLE minifig_catalog(id INTEGER PRIMARY KEY)")
                 && q.exec("CREATE TABLE set_catalog(id INTEGER PRIMARY KEY)")
                 && q.exec("CREATE TABLE part(id INTEGER PRIMARY KEY)")
                 && q.exec("CREATE TABLE color(id INTEGER PRIMARY KEY)")
                 && q.exec("CREATE TABLE build(id INTEGER PRIMARY KEY AUTOINCREMENT,workspace_id INTEGER NOT NULL,build_type TEXT NOT NULL CHECK(build_type IN ('Set','MOC')),name TEXT NOT NULL,set_number TEXT,inventory_mode TEXT NOT NULL DEFAULT 'Stock' CHECK(inventory_mode IN ('Stock','CompleteSet')),status TEXT NOT NULL DEFAULT 'Planned' CHECK(status IN ('Planned','Pulling','Complete','Disassembled','Cancelled')),is_active INTEGER NOT NULL DEFAULT 1,notes TEXT,created_utc TEXT NOT NULL,modified_utc TEXT NOT NULL,manufacturer_id INTEGER REFERENCES manufacturer(id),FOREIGN KEY(workspace_id) REFERENCES workspace(id))")
                 && q.exec("INSERT INTO build(id,workspace_id,build_type,name,set_number,inventory_mode,status,is_active,notes,created_utc,modified_utc,manufacturer_id) VALUES(41,1,'Set','Existing Set','1-1','Stock','Planned',1,'note','2026-01-01T00:00:00.000Z','2026-01-02T00:00:00.000Z',NULL),(42,1,'MOC','Existing MOC','MOC-1','Stock','Cancelled',0,NULL,'2026-01-03T00:00:00.000Z','2026-01-04T00:00:00.000Z',NULL)")
                 && DatabaseSchema::initialize(db)
                 && scalar(db,"SELECT version FROM schema_version")==28
                 && scalar(db,"SELECT COUNT(*) FROM build WHERE id IN (41,42)")==2
                 && scalar(db,"SELECT COUNT(*) FROM build WHERE id=41 AND build_type='Set' AND name='Existing Set' AND set_number='1-1' AND status='Planned' AND is_active=1 AND notes='note' AND created_utc='2026-01-01T00:00:00.000Z' AND modified_utc='2026-01-02T00:00:00.000Z'")==1
                 && scalar(db,"SELECT COUNT(*) FROM build WHERE id=42 AND build_type='MOC' AND name='Existing MOC' AND set_number='MOC-1' AND status='Cancelled' AND is_active=0 AND notes IS NULL")==1
                 && scalar(db,"SELECT COUNT(*) FROM pragma_table_info('build') WHERE name='minifig_catalog_id'")==1
                 && scalar(db,"SELECT COUNT(*) FROM pragma_table_info('build') WHERE name='set_catalog_id'")==1;
        }
        db.close();
    }
    QSqlDatabase::removeDatabase(connection);
    return ok;
}
}

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    app.setOrganizationName("RFStateSideTests"); app.setApplicationName("BrickSuiteM2374Test");
    QStandardPaths::setTestModeEnabled(true);
    QTemporaryDir migrationFiles;
    if (!require(migrationFiles.isValid() && validateV26Migration(migrationFiles.filePath("v26.db")),
                 "Schema 26 to 28 migration did not preserve existing Builds.")) return 1;
    const QString data = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir(data).removeRecursively(); Cleanup cleanup(data);
    if (!require(DatabaseManager::instance().initialize(), "Database initialization failed.")) return 1;
    QSqlDatabase db = DatabaseManager::instance().database(); QSqlQuery q(db);
    const QString now = "2026-01-01T00:00:00.000Z";
    if (!require(q.exec("INSERT INTO workspace(name,description,created_utc,modified_utc) VALUES('Test','', '"+now+"','"+now+"')"), "Workspace seed failed.")) return 1;
    const int workspaceId = q.lastInsertId().toInt();
    if (!require(q.exec("INSERT INTO minifig_catalog(name,num_parts,is_active,created_utc,modified_utc) VALUES('Pirate',4,1,'"+now+"','"+now+"')"), "Minifig seed failed.")) return 1;
    const int minifigId = q.lastInsertId().toInt();
    q.exec("INSERT INTO minifig_external_identifier(minifig_catalog_id,provider,external_id,source,is_active,created_utc,modified_utc) VALUES("+QString::number(minifigId)+",'Rebrickable','fig-1','test',1,'"+now+"','"+now+"')");
    if (!require(q.exec("INSERT INTO part(part_number,name,rebrickable_part_id,is_active,created_utc,modified_utc,material) VALUES('p1','One','p1',1,'"+now+"','"+now+"','Plastic'),('p2','Two','p2',1,'"+now+"','"+now+"','Plastic')"), "Part seed failed.")) return 1;
    if (!require(q.exec("INSERT INTO color(name,rebrickable_id,created_utc,modified_utc) VALUES('Red',1,'"+now+"','"+now+"'),('Blue',2,'"+now+"','"+now+"')"), "Color seed failed.")) return 1;
    if (!require(q.exec("INSERT INTO minifig_catalog_part(minifig_catalog_id,part_id,color_id,quantity_required,is_spare,provider,source,created_utc,modified_utc) SELECT "+QString::number(minifigId)+",p.id,c.id,CASE p.part_number WHEN 'p1' THEN 2 ELSE 1 END,CASE p.part_number WHEN 'p2' THEN 1 ELSE 0 END,'Rebrickable','test','"+now+"','"+now+"' FROM part p JOIN color c ON c.rebrickable_id=CASE p.part_number WHEN 'p1' THEN 1 ELSE 2 END"), "Composition seed failed.")) return 1;

    MinifigBuildCreationService service;
    auto result = service.create(workspaceId, minifigId, "Pirate Build");
    if (!require(result.success && result.requirementRows == 1 && result.requiredPieces == 2, "Valid creation failed: "+result.message)) return 1;
    BuildRepository builds; auto build = builds.getById(result.buildId);
    if (!require(build && build->buildType()=="Minifig" && build->inventoryMode()=="Stock" && build->status()=="Planned" && build->minifigCatalogId()==minifigId && build->sourceReference()=="fig-1", "Build identity is incorrect.")) return 1;
    auto requirements = BuildRequirementRepository().getByBuild(result.buildId);
    if (!require(requirements.size()==1 && requirements.first().quantityRequired()==2 && requirements.first().quantityPulled()==0 && requirements.first().quantityReleased()==0 && !requirements.first().isSpare() && requirements.first().substitutePartId()==0 && requirements.first().substituteColorId()==0, "Requirement snapshot is incorrect.")) return 1;
    q.exec("UPDATE minifig_catalog_part SET quantity_required=9 WHERE is_spare=0");
    if (!require(BuildRequirementRepository().getByBuild(result.buildId).first().quantityRequired()==2, "Catalog update changed snapshot.")) return 1;
    auto second = service.create(workspaceId,minifigId,"Second");
    if (!require(second.success && second.buildId != result.buildId, "Multiple Builds per Minifig failed.")) return 1;

    q.exec("DELETE FROM minifig_catalog_part");
    const int before = scalar(db,"SELECT COUNT(*) FROM build");
    auto empty = service.create(workspaceId,minifigId,"Empty");
    if (!require(!empty.success && scalar(db,"SELECT COUNT(*) FROM build")==before, "Empty composition was not rejected.")) return 1;

    q.exec("INSERT INTO minifig_catalog_part(minifig_catalog_id,part_id,color_id,quantity_required,is_spare,provider,source,created_utc,modified_utc) SELECT "+QString::number(minifigId)+",p.id,c.id,1,1,'Rebrickable','test','"+now+"','"+now+"' FROM part p JOIN color c ON c.rebrickable_id=1 WHERE p.part_number='p1'");
    auto spareOnly = service.create(workspaceId,minifigId,"Spare only");
    if (!require(!spareOnly.success && scalar(db,"SELECT COUNT(*) FROM build")==before, "Spare-only composition was not rejected.")) return 1;
    q.exec("DELETE FROM minifig_catalog_part");
    q.exec("INSERT INTO minifig_catalog_part(minifig_catalog_id,part_id,color_id,quantity_required,is_spare,provider,source,created_utc,modified_utc) SELECT "+QString::number(minifigId)+",p.id,c.id,1,0,'Rebrickable','test','"+now+"','"+now+"' FROM part p JOIN color c ON c.rebrickable_id=CASE p.part_number WHEN 'p1' THEN 1 ELSE 2 END");
    q.exec("CREATE TRIGGER force_late_build_failure BEFORE INSERT ON build_requirement WHEN NEW.part_id=(SELECT id FROM part WHERE part_number='p2') BEGIN SELECT RAISE(ABORT,'forced late failure'); END");
    const int requirementsBeforeFailure = scalar(db,"SELECT COUNT(*) FROM build_requirement");
    auto failed = service.create(workspaceId,minifigId,"Rollback");
    if (!require(!failed.success && scalar(db,"SELECT COUNT(*) FROM build")==before
                 && scalar(db,"SELECT COUNT(*) FROM build WHERE name='Rollback'")==0,
                 "Late requirement failure did not roll back the Build.")) return 1;
    if (!require(scalar(db,"SELECT COUNT(*) FROM build_requirement")==requirementsBeforeFailure,
                 "Late requirement failure left a partial requirement snapshot.")) return 1;

    Build setBuild; setBuild.setWorkspaceId(workspaceId); setBuild.setBuildType("Set");
    setBuild.setName("Set"); setBuild.setSetNumber("1-1");
    Build mocBuild; mocBuild.setWorkspaceId(workspaceId); mocBuild.setBuildType("MOC");
    mocBuild.setName("MOC"); mocBuild.setSetNumber("MOC-1");
    if (!require(builds.create(setBuild) && builds.create(mocBuild)
                 && builds.getById(setBuild.id())->buildType()=="Set"
                 && builds.getById(mocBuild.id())->buildType()=="MOC",
                 "Existing Set/MOC repository behavior regressed.")) return 1;
    qInfo() << "M23.7.4 Minifig Build creation validation passed.";
    return 0;
}
