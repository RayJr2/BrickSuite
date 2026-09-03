#include "../src/database/DatabaseManager.h"
#include "../src/database/DatabaseSchema.h"
#include "../src/models/CollectionItem.h"
#include "../src/models/StorageLocation.h"
#include "../src/repositories/CollectionRepository.h"
#include "../src/repositories/StorageLocationRepository.h"
#include "../src/services/collection/CollectionItemService.h"

#include <QCoreApplication>
#include <QDir>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QStandardPaths>
#include <cstdio>

namespace {
bool check(bool value, const char* message) { if (!value) std::fprintf(stderr, "FAILED: %s\n", message); return value; }
int scalar(QSqlDatabase db, const QString& sql) { QSqlQuery q(db); return q.exec(sql) && q.next() ? q.value(0).toInt() : -1; }
class Cleanup { public: explicit Cleanup(QString path): path(std::move(path)) {} ~Cleanup(){ DatabaseManager::instance().close(); QDir(path).removeRecursively(); } QString path; };
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    app.setOrganizationName("RFStateSideTests"); app.setApplicationName("CatalogToCollection");
    QStandardPaths::setTestModeEnabled(true);
    const QString data=QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir(data).removeRecursively(); Cleanup cleanup(data);
    bool ok=check(DatabaseManager::instance().initialize(),"initialize");
    QSqlDatabase db=DatabaseManager::instance().database(); QSqlQuery q(db);
    const QString now="2026-01-01T00:00:00.000Z";
    ok &= check(scalar(db,"SELECT version FROM schema_version")==31,"schema remains 31");
    ok &= check(q.exec("INSERT INTO workspace(name,description,created_utc,modified_utc) VALUES('One','', '"+now+"','"+now+"'),('Two','', '"+now+"','"+now+"')"),"workspaces");
    ok &= check(q.exec("INSERT INTO set_catalog(set_number,name,year,theme_id,num_parts,image_url,created_utc,modified_utc) VALUES('100-1','Owned Set',2026,1,10,'','"+now+"','"+now+"')"),"set"); const int setId=q.lastInsertId().toInt();
    ok &= check(q.exec("INSERT INTO minifig_catalog(name,num_parts,image_url,is_active,created_utc,modified_utc) VALUES('Owned Fig',4,'',1,'"+now+"','"+now+"')"),"minifig"); const int figId=q.lastInsertId().toInt();
    const int typeId=scalar(db,"SELECT id FROM storage_location_type ORDER BY id LIMIT 1");
    StorageLocationRepository locations;
    auto location=[&](int workspace,const QString& name,bool inventory,bool collection){ StorageLocation l; l.setWorkspaceId(workspace); l.setLocationTypeId(typeId); l.setName(name); l.setAllowsInventory(inventory); l.setAllowsCollection(collection); ok &= check(locations.create(l),"location"); return l.id(); };
    const int collection=location(1,"Shelf",false,true), both=location(1,"Both",true,true);
    const int inventory=location(1,"Parts",true,false), wrong=location(2,"Other",false,true);
    const int buildsBefore=scalar(db,"SELECT COUNT(*) FROM build");
    const int inventoryBefore=scalar(db,"SELECT COUNT(*) FROM inventory_record");
    CollectionItemService service;
    const auto setOne=service.createSet(1,setId,CollectionItemState::Assembled,0,0,"First","set notes");
    const auto setTwo=service.createSet(1,setId,CollectionItemState::Sealed,collection);
    const auto figOne=service.createMinifig(1,figId,CollectionItemState::Assembled,both,0,"Hero","fig notes");
    const auto figTwo=service.createMinifig(1,figId,CollectionItemState::PartiallyAssembled,0);
    ok &= check(setOne.success && setTwo.success && figOne.success && figTwo.success,"Set/Minifig creation and alternate states");
    ok &= check(setOne.collectionItemId!=setTwo.collectionItemId && figOne.collectionItemId!=figTwo.collectionItemId,"multiple physical copies retain distinct IDs");
    const auto setItem=CollectionRepository().getById(setOne.collectionItemId);
    const auto figItem=CollectionRepository().getById(figOne.collectionItemId);
    ok &= check(setItem && setItem->state==CollectionItemState::Assembled && setItem->storageLocationId==0
                && setItem->condition==CollectionItemCondition::Used
                && setItem->completeness==CollectionItemCompleteness::Unknown
                && setItem->sourceBuildId==0 && !setItem->allowPartsSource
                && setItem->nickname=="First" && setItem->notes=="set notes","Set identity, default-state input, Unassigned, metadata, and dormant fields");
    ok &= check(figItem && figItem->minifigCatalogId==figId && figItem->setCatalogId==0
                && figItem->sourceBuildId==0 && !figItem->allowPartsSource
                && figItem->storageLocationId==both,"Minifig identity and Both-capability location");
    ok &= check(!service.createSet(1,setId,CollectionItemState::Assembled,inventory).success
                && !service.createMinifig(1,figId,CollectionItemState::Assembled,wrong).success,"Inventory-only and wrong-workspace locations rejected");
    ok &= check(scalar(db,"SELECT COUNT(*) FROM build")==buildsBefore
                && scalar(db,"SELECT COUNT(*) FROM inventory_record")==inventoryBefore,"no Build creation or inventory mutation");
    ok &= check(q.exec("UPDATE minifig_catalog SET is_active=0 WHERE id="+QString::number(figId))
                && CollectionRepository().getById(figOne.collectionItemId).has_value(),"catalog deactivation preserves Collection item");
    const int before=scalar(db,"SELECT COUNT(*) FROM collection_item");
    ok &= check(q.exec("CREATE TRIGGER fail_catalog_collection BEFORE INSERT ON collection_item BEGIN SELECT RAISE(ABORT,'forced'); END"),"failure trigger");
    ok &= check(!service.createSet(1,setId,CollectionItemState::Assembled).success
                && scalar(db,"SELECT COUNT(*) FROM collection_item")==before,"transaction failure leaves no partial row");
    if (ok) std::fprintf(stdout,"M23.9.3 Catalog to Collection validation passed.\n");
    return ok?0:1;
}
