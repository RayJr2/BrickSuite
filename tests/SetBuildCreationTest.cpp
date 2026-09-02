#include "../src/database/DatabaseManager.h"
#include "../src/models/Build.h"
#include "../src/repositories/BuildRepository.h"
#include "../src/repositories/BuildRequirementRepository.h"
#include "../src/services/builds/SetBuildCreationService.h"
#include "../src/services/sets/SetCompositionReplacementService.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QStandardPaths>

namespace {
bool require(bool ok, const QString& text) { if (!ok) qCritical().noquote() << text; return ok; }
int scalar(QSqlDatabase db, const QString& sql) { QSqlQuery q(db); return q.exec(sql) && q.next() ? q.value(0).toInt() : -1; }
class Cleanup { public: explicit Cleanup(QString p):path(std::move(p)){} ~Cleanup(){DatabaseManager::instance().close();QDir(path).removeRecursively();} QString path; };
}

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    app.setOrganizationName("RFStateSideTests"); app.setApplicationName("SetBuildCreation");
    QStandardPaths::setTestModeEnabled(true);
    const QString data = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir(data).removeRecursively(); Cleanup cleanup(data);
    if (!require(DatabaseManager::instance().initialize(), "Database initialization failed.")) return 1;
    QSqlDatabase db = DatabaseManager::instance().database(); QSqlQuery q(db);
    const QString now = "2026-01-01T00:00:00.000Z";
    if (!require(q.exec("INSERT INTO workspace(name,description,created_utc,modified_utc) VALUES('Test','', '"+now+"','"+now+"')"), "Workspace seed failed.")) return 1;
    const int workspaceId = q.lastInsertId().toInt();
    if (!require(q.exec("INSERT INTO set_catalog(set_number,name,year,theme_id,num_parts,image_url,created_utc,modified_utc) VALUES('1234-1','Test Set',2026,1,4,NULL,'"+now+"','"+now+"')"), "Set seed failed.")) return 1;
    const int setId = q.lastInsertId().toInt();
    if (!require(q.exec("INSERT INTO part(part_number,name,rebrickable_part_id,is_active,created_utc,modified_utc,material) VALUES('p1','One','p1',1,'"+now+"','"+now+"','Plastic'),('p2','Two','p2',1,'"+now+"','"+now+"','Plastic')"), "Part seed failed.")) return 1;
    if (!require(q.exec("INSERT INTO color(name,rebrickable_id,created_utc,modified_utc) VALUES('Red',1,'"+now+"','"+now+"'),('Blue',2,'"+now+"','"+now+"')"), "Color seed failed.")) return 1;
    SetCompositionReplacementService composition;
    auto replaced = composition.replace(setId, {{"p1",1,2,false,"required"},{"p2",2,1,true,"spare"}}, "Rebrickable", "test");
    if (!require(replaced.success, "Composition seed failed: " + replaced.message)) return 1;

    SetBuildCreationService service;
    auto created = service.create(workspaceId, setId, "First");
    if (!require(created.success && created.requirementRows==1 && created.requiredPieces==2 && created.excludedSparePieces==1, "Set Build creation failed: " + created.message)) return 1;
    BuildRepository builds; const auto build = builds.getById(created.buildId);
    if (!require(build && build->buildType()=="Set" && build->inventoryMode()=="Stock" && build->status()=="Planned" && build->setCatalogId()==setId && build->setNumber()=="1234-1", "Linked Set Build identity is incorrect.")) return 1;
    auto requirements = BuildRequirementRepository().getByBuild(created.buildId);
    if (!require(requirements.size()==1 && requirements.first().partId()>0 && requirements.first().colorId()>0 && requirements.first().quantityRequired()==2 && requirements.first().quantityPulled()==0 && requirements.first().quantityReleased()==0 && !requirements.first().isSpare() && requirements.first().substitutePartId()==0 && requirements.first().substituteColorId()==0, "Requirement snapshot semantics are incorrect.")) return 1;
    const int snapPart=requirements.first().partId(), snapColor=requirements.first().colorId(), snapQuantity=requirements.first().quantityRequired();

    const auto second = service.create(workspaceId, setId, "Second");
    if (!require(second.success && second.buildId!=created.buildId, "Multiple Builds per Set failed.")) return 1;
    replaced = composition.replace(setId, {{"p2",2,9,false,"refresh"}}, "Rebrickable", "refresh");
    requirements = BuildRequirementRepository().getByBuild(created.buildId);
    if (!require(replaced.success && requirements.size()==1 && requirements.first().partId()==snapPart && requirements.first().colorId()==snapColor && requirements.first().quantityRequired()==snapQuantity, "Catalog refresh changed an existing Build snapshot.")) return 1;

    q.exec("DELETE FROM set_catalog_part"); const int buildsBefore=scalar(db,"SELECT COUNT(*) FROM build");
    if (!require(!service.create(workspaceId,setId,"Empty").success && scalar(db,"SELECT COUNT(*) FROM build")==buildsBefore, "Empty composition created a Build.")) return 1;
    replaced=composition.replace(setId,{{"p2",2,1,true,"spare"}},"Rebrickable","test");
    if (!require(replaced.success && !service.create(workspaceId,setId,"Spare only").success && scalar(db,"SELECT COUNT(*) FROM build")==buildsBefore, "Spare-only composition created a Build.")) return 1;

    replaced=composition.replace(setId,{{"p1",1,1,false,"one"},{"p2",2,1,false,"two"}},"Rebrickable","test");
    if (!require(replaced.success && q.exec("CREATE TRIGGER force_set_build_failure BEFORE INSERT ON build_requirement WHEN NEW.part_id=(SELECT id FROM part WHERE part_number='p2') BEGIN SELECT RAISE(ABORT,'forced late failure'); END"), "Failure setup failed.")) return 1;
    const int requirementsBefore=scalar(db,"SELECT COUNT(*) FROM build_requirement");
    const auto failed=service.create(workspaceId,setId,"Rollback");
    if (!require(!failed.success && scalar(db,"SELECT COUNT(*) FROM build")==buildsBefore && scalar(db,"SELECT COUNT(*) FROM build_requirement")==requirementsBefore && scalar(db,"SELECT COUNT(*) FROM build WHERE name='Rollback'")==0, "Late failure left a partial Build.")) return 1;
    q.exec("DROP TRIGGER force_set_build_failure");

    Build legacy; legacy.setWorkspaceId(workspaceId); legacy.setBuildType("Set"); legacy.setName("Legacy"); legacy.setSetNumber("1234-1"); legacy.setInventoryMode("Stock"); legacy.setStatus("Planned");
    Build complete; complete.setWorkspaceId(workspaceId); complete.setBuildType("Set"); complete.setName("Complete"); complete.setSetNumber("1234-1"); complete.setInventoryMode("CompleteSet"); complete.setStatus("Planned");
    if (!require(builds.create(legacy)&&legacy.setCatalogId()==0&&builds.create(complete)&&complete.inventoryMode()=="CompleteSet"&&complete.setCatalogId()==0, "Legacy or CompleteSet creation changed.")) return 1;
    qInfo() << "M23.10.2 Set Build creation validation passed.";
    return 0;
}
