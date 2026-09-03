#include "../src/database/DatabaseManager.h"
#include "../src/database/DatabaseSchema.h"
#include "../src/models/CollectionSearchCriteria.h"
#include "../src/models/StorageLocation.h"
#include "../src/repositories/CollectionRepository.h"
#include "../src/repositories/StorageLocationRepository.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
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
    app.setOrganizationName("RFStateSideTests"); app.setApplicationName("MyCollection");
    QStandardPaths::setTestModeEnabled(true);
    const QString data = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir(data).removeRecursively(); Cleanup cleanup(data);
    bool ok = check(DatabaseManager::instance().initialize(), "initialize schema 31");
    QSqlDatabase db = DatabaseManager::instance().database(); QSqlQuery q(db);
    const QString now = "2026-01-01T00:00:00.000Z";
    ok &= check(scalar(db,"SELECT version FROM schema_version")==31, "schema remains 31");
    ok &= check(q.exec("INSERT INTO workspace(name,description,created_utc,modified_utc) VALUES('One','', '"+now+"','"+now+"')"), "workspace");
    ok &= check(q.exec("INSERT INTO set_catalog(set_number,name,year,theme_id,num_parts,image_url,created_utc,modified_utc) VALUES('100-1','City Set',2026,1,10,'set-url','"+now+"','"+now+"')"), "set");
    const int setId=q.lastInsertId().toInt();
    ok &= check(q.exec("INSERT INTO minifig_catalog(name,num_parts,image_url,is_active,created_utc,modified_utc) VALUES('Arctic Fig',4,'fig-url',1,'"+now+"','"+now+"')"), "fig");
    const int figId=q.lastInsertId().toInt();
    ok &= check(q.exec("INSERT INTO minifig_external_identifier(minifig_catalog_id,provider,external_id,source,is_active,created_utc,modified_utc) VALUES("+QString::number(figId)+",'Rebrickable','fig-ice','test',1,'"+now+"','"+now+"')"), "fig identity");
    ok &= check(q.exec("INSERT INTO build(workspace_id,build_type,set_number,name,inventory_mode,status,is_active,created_utc,modified_utc) VALUES(1,'MOC','MOC-7','Moon Base','Stock','Complete',1,'"+now+"','"+now+"')"), "MOC build");
    const int buildId=q.lastInsertId().toInt();
    const int typeId=scalar(db,"SELECT id FROM storage_location_type ORDER BY id LIMIT 1");
    StorageLocationRepository locations;
    auto addLocation=[&](const QString& name,int parent,bool inventory,bool collection){ StorageLocation l; l.setWorkspaceId(1); l.setLocationTypeId(typeId); l.setParentLocationId(parent); l.setName(name); l.setAllowsInventory(inventory); l.setAllowsCollection(collection); ok &= check(locations.create(l),"location"); return l.id(); };
    const int room=addLocation("Room",0,false,false);
    const int shelf=addLocation("Shelf",room,false,true);
    const int bin=addLocation("Bin",room,true,false);
    const int both=addLocation("Both",room,true,true);
    auto insertItem=[&](const QString& type,int catalogId,int location,const QString& state,const QString& nickname,int source=0,int active=1){
        QString setValue=type=="Set"?QString::number(catalogId):"NULL";
        QString figValue=type=="Minifig"?QString::number(catalogId):"NULL";
        QString sourceValue=source>0?QString::number(source):"NULL";
        QString locationValue=location>0?QString::number(location):"NULL";
        return q.exec("INSERT INTO collection_item(workspace_id,item_type,set_catalog_id,minifig_catalog_id,state,storage_location_id,source_build_id,nickname,notes,allow_parts_source,is_active,created_utc,modified_utc) VALUES(1,'"+type+"',"+setValue+","+figValue+",'"+state+"',"+locationValue+","+sourceValue+",'"+nickname+"','',0,"+QString::number(active)+",'"+now+"','"+now+"')");
    };
    ok &= check(insertItem("Set",setId,shelf,"Sealed","Boxed"),"Set item one");
    ok &= check(insertItem("Set",setId,both,"Assembled","Display"),"Set item two");
    ok &= check(insertItem("Minifig",figId,0,"Unassembled","Explorer"),"Minifig item");
    ok &= check(insertItem("MOC",0,shelf,"Assembled","Custom",buildId,0),"MOC item");

    CollectionRepository repository; CollectionSearchCriteria c; c.workspaceId=1;
    ok &= check(repository.count(c)==3 && repository.search(c).size()==3,"active count/search alignment and multiple copies");
    c.searchText="fig-ice"; auto rows=repository.search(c);
    ok &= check(rows.size()==1 && rows.first().displayName=="Arctic Fig" && rows.first().imageUrl=="fig-url","Minifig projected identity/image");
    c.searchText="MOC-7"; c.activeState=-1; rows=repository.search(c);
    ok &= check(rows.size()==1 && rows.first().displayName=="Moon Base" && rows.first().sourceBuildReference=="MOC-7","MOC Build projection");
    c.searchText=""; c.activeState=1; c.type=CollectionItemType::Set; c.state=CollectionItemState::Sealed;
    ok &= check(repository.count(c)==1,"type and state filters");
    c.type=CollectionItemType::Invalid; c.state=CollectionItemState::Invalid; c.storageLocationId=room;
    ok &= check(repository.count(c)==2,"parent location includes eligible descendants");
    c.storageLocationId=-1; ok &= check(repository.count(c)==1,"Unassigned filter");
    c.storageLocationId=0; c.activeState=0; ok &= check(repository.count(c)==1,"archived-only filter");
    c.activeState=-1; ok &= check(repository.count(c)==4,"combined active/archive filter");
    c.activeState=-1; c.storageLocationId=0; c.condition=CollectionItemCondition::Used;
    c.completeness=CollectionItemCompleteness::Unknown;
    ok &= check(repository.count(c)==4 && repository.search(c).size()==4,
                "condition/completeness count and search predicates align");
    ok &= check(locations.isValidInventoryDestination(1,bin) && !locations.isValidCollectionDestination(1,bin)
                && !locations.isValidInventoryDestination(1,shelf) && locations.isValidCollectionDestination(1,shelf)
                && locations.isValidInventoryDestination(1,both) && locations.isValidCollectionDestination(1,both),
                "Inventory/Collection/Both destination isolation");
    auto editable=locations.getById(bin); editable->setAllowsCollection(true);
    ok &= check(locations.update(*editable) && locations.getById(bin)->allowsCollection(),"capability edit persists");
    if (ok) qInfo() << "M23.9.2 My Collection validation passed.";
    return ok ? 0 : 1;
}
