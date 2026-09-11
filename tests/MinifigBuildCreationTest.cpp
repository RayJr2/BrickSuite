#include "../src/database/DatabaseManager.h"
#include "../src/database/DatabaseSchema.h"
#include "../src/models/Build.h"
#include "../src/models/BuildRequirement.h"
#include "../src/repositories/BuildRepository.h"
#include "../src/repositories/BuildRequirementRepository.h"
#include "../src/services/builds/MinifigBuildCreationService.h"
#include "../src/services/builds/BuildLifecycleService.h"
#include "../src/services/builds/BuildRequirementMutationService.h"
#include "../src/services/builds/BuildAllocationMutationService.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QUuid>
#include <utility>
#include <cstdio>

namespace {
bool require(bool ok, const QString& message) { if (!ok) { qCritical().noquote() << message; std::fprintf(stderr, "FAILED: %s\n", message.toUtf8().constData()); } return ok; }
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
                 && q.exec("CREATE TABLE storage_location(id INTEGER PRIMARY KEY,workspace_id INTEGER NOT NULL)")
                 && q.exec("CREATE TABLE manufacturer(id INTEGER PRIMARY KEY,code TEXT)")
                 && q.exec("CREATE TABLE minifig_catalog(id INTEGER PRIMARY KEY)")
                 && q.exec("CREATE TABLE set_catalog(id INTEGER PRIMARY KEY)")
                 && q.exec("CREATE TABLE part(id INTEGER PRIMARY KEY)")
                 && q.exec("CREATE TABLE color(id INTEGER PRIMARY KEY)")
                 && q.exec("CREATE TABLE build(id INTEGER PRIMARY KEY AUTOINCREMENT,workspace_id INTEGER NOT NULL,build_type TEXT NOT NULL CHECK(build_type IN ('Set','MOC')),name TEXT NOT NULL,set_number TEXT,inventory_mode TEXT NOT NULL DEFAULT 'Stock' CHECK(inventory_mode IN ('Stock','CompleteSet')),status TEXT NOT NULL DEFAULT 'Planned' CHECK(status IN ('Planned','Pulling','Complete','Disassembled','Cancelled')),is_active INTEGER NOT NULL DEFAULT 1,notes TEXT,created_utc TEXT NOT NULL,modified_utc TEXT NOT NULL,manufacturer_id INTEGER REFERENCES manufacturer(id),FOREIGN KEY(workspace_id) REFERENCES workspace(id))")
                 && q.exec("INSERT INTO build(id,workspace_id,build_type,name,set_number,inventory_mode,status,is_active,notes,created_utc,modified_utc,manufacturer_id) VALUES(41,1,'Set','Existing Set','1-1','Stock','Planned',1,'note','2026-01-01T00:00:00.000Z','2026-01-02T00:00:00.000Z',NULL),(42,1,'MOC','Existing MOC','MOC-1','Stock','Cancelled',0,NULL,'2026-01-03T00:00:00.000Z','2026-01-04T00:00:00.000Z',NULL)")
                 && DatabaseSchema::initialize(db)
                 && scalar(db,"SELECT version FROM schema_version")==35
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
    const int callerOwnedBuildsBefore = scalar(db, "SELECT COUNT(*) FROM build");
    const int callerOwnedRequirementsBefore = scalar(db, "SELECT COUNT(*) FROM build_requirement");
    if (!require(db.transaction(), "Unable to begin caller-owned Minifig Build transaction.")) return 1;
    const auto callerOwned = service.createInCurrentTransaction(workspaceId, minifigId, "Caller rollback");
    if (!require(callerOwned.success && scalar(db, "SELECT COUNT(*) FROM build") == callerOwnedBuildsBefore + 1,
                 "Minifig Build caller-owned seam did not mutate inside the transaction.")) return 1;
    if (!require(db.rollback()
                 && scalar(db, "SELECT COUNT(*) FROM build") == callerOwnedBuildsBefore
                 && scalar(db, "SELECT COUNT(*) FROM build_requirement") == callerOwnedRequirementsBefore,
                 "Caller rollback did not remove the Minifig Build snapshot.")) return 1;
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

    const int storageType = scalar(db, "SELECT id FROM storage_location_type LIMIT 1");
    if (!require(q.exec(QString("INSERT INTO storage_location(workspace_id,parent_location_id,location_type_id,name,description,sort_order,is_active,allows_inventory,allows_collection,created_utc,modified_utc) VALUES(%1,NULL,%2,'Test Bin','',0,1,1,1,'%3','%3')").arg(workspaceId).arg(storageType).arg(now)), "Storage seed failed.")) return 1;
    const int storageId = q.lastInsertId().toInt();
    const int partId = scalar(db, "SELECT id FROM part WHERE part_number='p1'");
    const int colorId = scalar(db, "SELECT id FROM color WHERE rebrickable_id=1");
    const int manufacturerId = scalar(db, "SELECT id FROM manufacturer WHERE code='LEGO'");
    Build spareBuild; spareBuild.setWorkspaceId(workspaceId); spareBuild.setBuildType("Set"); spareBuild.setName("Spare lifecycle"); spareBuild.setSetNumber("1-1"); spareBuild.setInventoryMode("CompleteSet"); spareBuild.setManufacturerId(manufacturerId); spareBuild.setStatus("Complete");
    if (!require(builds.create(spareBuild), "Spare Build seed failed.")) return 1;
    BuildRequirement spare; spare.setBuildId(spareBuild.id()); spare.setPartId(partId); spare.setColorId(colorId); spare.setQuantityRequired(2); spare.setIsSpare(true);
    if (!require(BuildRequirementRepository().create(spare), "Spare requirement seed failed.")) return 1;
    BuildLifecycleService lifecycle(db);
    const int inventoryBefore = scalar(db, "SELECT COUNT(*) FROM inventory_record");
    const int movementsBefore = scalar(db, "SELECT COUNT(*) FROM inventory_movement");
    if (!require(db.transaction(), "Begin spare rollback failed.")) return 1;
    const auto stored = lifecycle.storeCompleteSetSpareInCurrentTransaction(spareBuild.id(), spare.id(), storageId, 1);
    if (!require(stored.success && scalar(db, "SELECT COUNT(*) FROM inventory_record")==inventoryBefore+1 && scalar(db,"SELECT COUNT(*) FROM inventory_movement")==movementsBefore+1 && scalar(db,QString("SELECT quantity_released FROM build_requirement WHERE id=%1").arg(spare.id()))==1, "Spare seam did not reach all mutations.")) return 1;
    if (!require(db.rollback() && scalar(db,"SELECT COUNT(*) FROM inventory_record")==inventoryBefore && scalar(db,"SELECT COUNT(*) FROM inventory_movement")==movementsBefore && scalar(db,QString("SELECT quantity_released FROM build_requirement WHERE id=%1").arg(spare.id()))==0, "Spare rollback left partial state.")) return 1;

    Build stockBuild; stockBuild.setWorkspaceId(workspaceId); stockBuild.setBuildType("MOC"); stockBuild.setName("Disassembly lifecycle"); stockBuild.setSetNumber("MOC-D"); stockBuild.setInventoryMode("Stock"); stockBuild.setManufacturerId(manufacturerId); stockBuild.setStatus("Complete");
    if (!require(builds.create(stockBuild), "Disassembly Build seed failed.")) return 1;
    BuildRequirement pulled; pulled.setBuildId(stockBuild.id()); pulled.setPartId(partId); pulled.setColorId(colorId); pulled.setQuantityRequired(1); pulled.setQuantityPulled(1);
    if (!require(BuildRequirementRepository().create(pulled), "Pulled requirement seed failed.")) return 1;
    if (!require(q.exec(QString("INSERT INTO build_part_provenance(build_id,part_id,color_id,manufacturer_id,quantity_pulled,created_utc,modified_utc) VALUES(%1,%2,%3,%4,1,'%5','%5')").arg(stockBuild.id()).arg(partId).arg(colorId).arg(manufacturerId).arg(now)), "Provenance seed failed.")) return 1;
    if (!require(q.exec(QString("INSERT INTO collection_item(workspace_id,item_type,state,condition,completeness,source_build_id,nickname,notes,allow_parts_source,is_active,created_utc,modified_utc) VALUES(%1,'MOC','Assembled','Used','Complete',%2,'','','0',1,'%3','%3')").arg(workspaceId).arg(stockBuild.id()).arg(now)), "Linked Collection seed failed.")) return 1;
    BuildLifecycleService::DisassemblyReturn returned{pulled.id(),partId,colorId,manufacturerId,storageId,1,false};
    if (!require(db.transaction(), "Begin disassembly rollback failed.")) return 1;
    const auto disassembled = lifecycle.disassembleInCurrentTransaction(stockBuild.id(), {returned}, CollectionItemState::Unassembled);
    if (!require(disassembled.success && scalar(db,QString("SELECT COUNT(*) FROM build WHERE id=%1 AND status='Disassembled'").arg(stockBuild.id()))==1 && scalar(db,QString("SELECT quantity_pulled FROM build_requirement WHERE id=%1").arg(pulled.id()))==0 && scalar(db,QString("SELECT COUNT(*) FROM build_part_provenance WHERE build_id=%1").arg(stockBuild.id()))==0 && scalar(db,QString("SELECT COUNT(*) FROM collection_item WHERE source_build_id=%1 AND state='Unassembled'").arg(stockBuild.id()))==1, "Disassembly seam did not reach lifecycle/provenance/Collection mutations.")) return 1;
    if (!require(db.rollback() && scalar(db,QString("SELECT COUNT(*) FROM build WHERE id=%1 AND status='Complete'").arg(stockBuild.id()))==1 && scalar(db,QString("SELECT quantity_pulled FROM build_requirement WHERE id=%1").arg(pulled.id()))==1 && scalar(db,QString("SELECT quantity_pulled FROM build_part_provenance WHERE build_id=%1").arg(stockBuild.id()))==1 && scalar(db,QString("SELECT COUNT(*) FROM collection_item WHERE source_build_id=%1 AND state='Assembled'").arg(stockBuild.id()))==1 && scalar(db,"SELECT COUNT(*) FROM inventory_record")==inventoryBefore, "Disassembly rollback left cross-domain state.")) return 1;
    if (!require(q.exec(QString("UPDATE build SET status='Pulling' WHERE id=%1").arg(stockBuild.id())), "Prepare pulled cancellation failed.")) return 1;
    const int cancellationAllocationsBefore = scalar(db, "SELECT COUNT(*) FROM build_allocation");
    if (!require(db.transaction(), "Begin pulled cancellation rollback failed.")) return 1;
    const auto cancelledPulled = lifecycle.cancelInCurrentTransaction(
        stockBuild.id(), {returned}, CollectionItemState::Unassembled);
    if (!require(cancelledPulled.success
                 && scalar(db, QString("SELECT COUNT(*) FROM build WHERE id=%1 AND status='Cancelled'").arg(stockBuild.id())) == 1
                 && scalar(db, QString("SELECT quantity_pulled FROM build_requirement WHERE id=%1").arg(pulled.id())) == 0
                 && scalar(db, QString("SELECT COUNT(*) FROM collection_item WHERE source_build_id=%1 AND state='Unassembled'").arg(stockBuild.id())) == 1,
                 "Pulled cancellation did not reach its late lifecycle mutations.")) return 1;
    if (!require(db.rollback()
                 && scalar(db, QString("SELECT COUNT(*) FROM build WHERE id=%1 AND status='Pulling'").arg(stockBuild.id())) == 1
                 && scalar(db, QString("SELECT quantity_pulled FROM build_requirement WHERE id=%1").arg(pulled.id())) == 1
                 && scalar(db, QString("SELECT quantity_pulled FROM build_part_provenance WHERE build_id=%1").arg(stockBuild.id())) == 1
                 && scalar(db, QString("SELECT COUNT(*) FROM collection_item WHERE source_build_id=%1 AND state='Assembled'").arg(stockBuild.id())) == 1
                 && scalar(db, "SELECT COUNT(*) FROM inventory_record") == inventoryBefore
                 && scalar(db, "SELECT COUNT(*) FROM inventory_movement") == movementsBefore
                 && scalar(db, "SELECT COUNT(*) FROM build_allocation") == cancellationAllocationsBefore,
                 "Pulled cancellation rollback left cross-domain state.")) return 1;

    Build requirementBuild; requirementBuild.setWorkspaceId(workspaceId); requirementBuild.setBuildType("MOC"); requirementBuild.setName("Requirement seams"); requirementBuild.setSetNumber("MOC-R"); requirementBuild.setInventoryMode("Stock"); requirementBuild.setManufacturerId(manufacturerId); requirementBuild.setStatus("Planned");
    if (!require(builds.create(requirementBuild), "Requirement Build seed failed.")) return 1;
    BuildRequirementMutationService requirementMutations(db);
    BuildRequirement candidate; candidate.setBuildId(requirementBuild.id()); candidate.setPartId(partId); candidate.setColorId(colorId); candidate.setQuantityRequired(3);
    const int requirementCount = scalar(db,"SELECT COUNT(*) FROM build_requirement");
    if (!require(db.transaction(), "Begin requirement add rollback failed.")) return 1;
    auto addedRequirement = requirementMutations.addInCurrentTransaction(candidate);
    if (!require(addedRequirement.success && db.rollback() && scalar(db,"SELECT COUNT(*) FROM build_requirement")==requirementCount, "Requirement add rollback failed.")) return 1;
    addedRequirement = requirementMutations.add(candidate);
    if (!require(addedRequirement.success, "Requirement add wrapper failed.")) return 1;
    const int mutationRequirementId = addedRequirement.requirement.id();
    if (!require(db.transaction(), "Begin requirement edit rollback failed.")) return 1;
    auto editedRequirement = requirementMutations.editInCurrentTransaction(mutationRequirementId,0,0,5,true);
    if (!require(editedRequirement.success && db.rollback() && scalar(db,QString("SELECT COUNT(*) FROM build_requirement WHERE id=%1 AND quantity_required=3 AND is_spare=0").arg(mutationRequirementId))==1, "Requirement edit rollback failed.")) return 1;
    if (!require(db.transaction(), "Begin requirement remove rollback failed.")) return 1;
    auto removedRequirement = requirementMutations.removeInCurrentTransaction(mutationRequirementId);
    if (!require(removedRequirement.success && db.rollback() && scalar(db,QString("SELECT COUNT(*) FROM build_requirement WHERE id=%1").arg(mutationRequirementId))==1, "Requirement remove rollback failed.")) return 1;

    if (!require(q.exec(QString("INSERT INTO inventory_record(workspace_id,part_id,color_id,storage_location_id,manufacturer_id,condition,ownership_type,quantity,created_utc,modified_utc) VALUES(%1,%2,%3,%4,%5,'Used','Owned',10,'%6','%6')").arg(workspaceId).arg(partId).arg(colorId).arg(storageId).arg(manufacturerId).arg(now)), "Inventory allocation seed failed.")) return 1;
    const int inventoryId = q.lastInsertId().toInt();
    BuildAllocationMutationService allocationMutations(db);
    BuildAllocation requested; requested.setInventoryRecordId(inventoryId); requested.setQuantityAllocated(2);
    if (!require(db.transaction(), "Begin allocation replace rollback failed.")) return 1;
    auto replacedAllocation = allocationMutations.replaceForRequirementInCurrentTransaction(mutationRequirementId,{requested});
    if (!require(replacedAllocation.success && scalar(db,"SELECT COUNT(*) FROM build_allocation")==1 && db.rollback() && scalar(db,"SELECT COUNT(*) FROM build_allocation")==0, "Allocation replacement rollback failed.")) return 1;
    if (!require(db.transaction(), "Begin allocate-available rollback failed.")) return 1;
    auto automatic = allocationMutations.allocateAvailableInCurrentTransaction(requirementBuild.id(),storageId);
    if (!require(automatic.success && automatic.piecesAdded==3 && scalar(db,"SELECT COUNT(*) FROM build_allocation")==1 && db.rollback() && scalar(db,"SELECT COUNT(*) FROM build_allocation")==0 && scalar(db,QString("SELECT quantity FROM inventory_record WHERE id=%1").arg(inventoryId))==10, "Allocate Available rollback failed.")) return 1;
    if (!require(allocationMutations.replaceForRequirement(mutationRequirementId,{requested}).success,
                 "Unpulled cancellation allocation seed failed.")) return 1;
    if (!require(db.transaction(), "Begin unpulled cancellation rollback failed.")) return 1;
    const auto cancelledUnpulled = lifecycle.cancelInCurrentTransaction(
        requirementBuild.id(), {}, CollectionItemState::Unassembled);
    if (!require(cancelledUnpulled.success
                 && scalar(db, "SELECT COUNT(*) FROM build_allocation") == 0
                 && db.rollback()
                 && scalar(db, "SELECT COUNT(*) FROM build_allocation") == 1
                 && scalar(db, QString("SELECT COUNT(*) FROM build WHERE id=%1 AND status='Planned'").arg(requirementBuild.id())) == 1,
                 "Unpulled cancellation rollback failed.")) return 1;
    qInfo() << "M23.7.4 Minifig Build creation validation passed.";
    return 0;
}
