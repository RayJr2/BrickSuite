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
#include <cstdio>

namespace {
bool require(bool ok, const QString& text) { if (!ok) { qCritical().noquote() << text; std::fprintf(stderr, "FAILED: %s\n", text.toUtf8().constData()); } return ok; }
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

    const int callerOwnedBuildsBefore = scalar(db, "SELECT COUNT(*) FROM build");
    const int callerOwnedRequirementsBefore = scalar(db, "SELECT COUNT(*) FROM build_requirement");
    if (!require(db.transaction(), "Unable to begin caller-owned Set Build transaction.")) return 1;
    const auto callerOwned = service.createInCurrentTransaction(workspaceId, setId, "Caller rollback");
    if (!require(callerOwned.success && scalar(db, "SELECT COUNT(*) FROM build") == callerOwnedBuildsBefore + 1,
                 "Set Build caller-owned seam did not mutate inside the transaction.")) return 1;
    if (!require(db.rollback()
                 && scalar(db, "SELECT COUNT(*) FROM build") == callerOwnedBuildsBefore
                 && scalar(db, "SELECT COUNT(*) FROM build_requirement") == callerOwnedRequirementsBefore,
                 "Caller rollback did not remove the Set Build snapshot.")) return 1;

    const int p1=scalar(db,"SELECT id FROM part WHERE part_number='p1'");
    const int p2=scalar(db,"SELECT id FROM part WHERE part_number='p2'");
    const int red=scalar(db,"SELECT id FROM color WHERE rebrickable_id=1");
    const int blue=scalar(db,"SELECT id FROM color WHERE rebrickable_id=2");
    if(!require(q.exec(QStringLiteral("INSERT INTO set_inventory_revision(provider,external_inventory_id,set_catalog_id,version,is_active,is_preferred,created_utc,modified_utc) VALUES('Rebrickable','preferred-1',%1,2,1,1,'%2','%2')").arg(setId).arg(now)),"Preferred revision seed failed."))return 1;
    const int revision=q.lastInsertId().toInt();
    if(!require(q.exec(QStringLiteral("INSERT INTO set_inventory_part(set_inventory_revision_id,part_id,color_id,quantity,is_spare,image_url,created_utc,modified_utc) VALUES(%1,%2,%3,4,0,'','%4','%4'),(%1,%5,%6,2,1,'','%4','%4')").arg(revision).arg(p2).arg(blue).arg(now).arg(p1).arg(red)),"Preferred composition seed failed."))return 1;
    const auto second = service.create(workspaceId, setId, "Second");
    const auto preferredRequirements=BuildRequirementRepository().getByBuild(second.buildId);
    if (!require(second.success && second.buildId!=created.buildId&&second.requiredPieces==4&&second.excludedSparePieces==2
        &&preferredRequirements.size()==1&&preferredRequirements.first().partId()==p2&&preferredRequirements.first().colorId()==blue&&preferredRequirements.first().quantityRequired()==4,
        "Preferred revision Set Build failed.")) return 1;
    replaced = composition.replace(setId, {{"p2",2,9,false,"refresh"}}, "Rebrickable", "refresh");
    requirements = BuildRequirementRepository().getByBuild(created.buildId);
    if (!require(replaced.success && requirements.size()==1 && requirements.first().partId()==snapPart && requirements.first().colorId()==snapColor && requirements.first().quantityRequired()==snapQuantity, "Catalog refresh changed an existing Build snapshot.")) return 1;
    if(!require(q.exec("UPDATE set_inventory_revision SET is_preferred=0 WHERE external_inventory_id='preferred-1'")
        &&q.exec(QStringLiteral("INSERT INTO set_inventory_revision(provider,external_inventory_id,set_catalog_id,version,is_active,is_preferred,created_utc,modified_utc) VALUES('Rebrickable','preferred-2',%1,3,1,1,'%2','%2')").arg(setId).arg(now)),"Preferred revision transition failed."))return 1;
    const int revision2=q.lastInsertId().toInt();
    if(!require(q.exec(QStringLiteral("INSERT INTO set_inventory_part(set_inventory_revision_id,part_id,color_id,quantity,is_spare,image_url,created_utc,modified_utc) VALUES(%1,%2,%3,7,0,'','%4','%4')").arg(revision2).arg(p1).arg(red).arg(now)),"New preferred composition seed failed."))return 1;
    const auto frozenPreferred=BuildRequirementRepository().getByBuild(second.buildId);
    if(!require(frozenPreferred.size()==1&&frozenPreferred.first().partId()==p2&&frozenPreferred.first().quantityRequired()==4&&scalar(db,"SELECT COUNT(*) FROM build_allocation WHERE build_id="+QString::number(second.buildId))==0,"Preferred revision change altered an existing Build snapshot/allocation state."))return 1;

    q.exec("UPDATE set_inventory_revision SET is_preferred=0,is_active=0");
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
