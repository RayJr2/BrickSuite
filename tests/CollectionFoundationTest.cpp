#include "../src/database/DatabaseManager.h"
#include "../src/database/DatabaseSchema.h"
#include "../src/models/Build.h"
#include "../src/models/CollectionSearchCriteria.h"
#include "../src/models/StorageLocation.h"
#include "../src/repositories/BuildRepository.h"
#include "../src/repositories/CollectionRepository.h"
#include "../src/repositories/StorageLocationRepository.h"
#include "../src/services/collection/CollectionItemService.h"
#include "../src/services/database/AutomaticBackupPolicy.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QTemporaryDir>

namespace {
bool require(bool condition, const QString& message)
{
    if (!condition) qCritical().noquote() << "FAILED:" << message;
    return condition;
}

int scalar(QSqlDatabase database, const QString& sql)
{
    QSqlQuery query(database);
    return query.exec(sql) && query.next() ? query.value(0).toInt() : -1;
}

class Cleanup
{
public:
    explicit Cleanup(QString path) : m_path(std::move(path)) {}
    ~Cleanup() { DatabaseManager::instance().close(); QDir(m_path).removeRecursively(); }
private:
    QString m_path;
};

bool validateMigration29To30()
{
    QTemporaryDir directory;
    if (!directory.isValid()) return false;
    const QString connection = "collection-migration-29";
    bool ok = false;
    {
        QSqlDatabase database = QSqlDatabase::addDatabase("QSQLITE", connection);
        database.setDatabaseName(directory.filePath("v29.db"));
        if (!database.open()) return false;
        QSqlQuery q(database);
        const QStringList statements = {
            "PRAGMA foreign_keys=ON",
            "CREATE TABLE schema_version(version INTEGER)",
            "INSERT INTO schema_version VALUES(29)",
            "CREATE TABLE workspace(id INTEGER PRIMARY KEY, name TEXT, description TEXT, created_utc TEXT, modified_utc TEXT, is_active INTEGER)",
            "CREATE TABLE storage_location(id INTEGER PRIMARY KEY, workspace_id INTEGER NOT NULL, parent_location_id INTEGER, location_type_id INTEGER NOT NULL, name TEXT NOT NULL, description TEXT, sort_order INTEGER NOT NULL DEFAULT 0, is_active INTEGER NOT NULL DEFAULT 1, created_utc TEXT NOT NULL, modified_utc TEXT NOT NULL, FOREIGN KEY(workspace_id) REFERENCES workspace(id), FOREIGN KEY(parent_location_id) REFERENCES storage_location(id))",
            "CREATE TABLE set_catalog(id INTEGER PRIMARY KEY)",
            "CREATE TABLE minifig_catalog(id INTEGER PRIMARY KEY)",
            "CREATE TABLE build(id INTEGER PRIMARY KEY, workspace_id INTEGER NOT NULL, FOREIGN KEY(workspace_id) REFERENCES workspace(id))",
            "INSERT INTO workspace VALUES(1,'Test','', '2026-01-01','2026-01-01',1)",
            "INSERT INTO storage_location VALUES(1,1,NULL,1,'Existing','',0,1,'2026-01-01','2026-01-01')"
        };
        bool seeded = true;
        for (const QString& statement : statements) seeded = seeded && q.exec(statement);
        ok = seeded && DatabaseSchema::initialize(database)
             && scalar(database, "SELECT version FROM schema_version") == 30
             && scalar(database, "SELECT allows_inventory FROM storage_location WHERE id=1") == 1
             && scalar(database, "SELECT allows_collection FROM storage_location WHERE id=1") == 0
             && scalar(database, "SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name='collection_item'") == 1;
        database.close();
    }
    QSqlDatabase::removeDatabase(connection);
    return ok;
}
}

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    app.setOrganizationName("RFStateSideTests");
    app.setApplicationName("CollectionFoundation");
    QStandardPaths::setTestModeEnabled(true);
    const QString data = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir(data).removeRecursively();
    Cleanup cleanup(data);

    bool ok = require(validateMigration29To30(), "schema 29 to 30 migration and defaults");
    if (!require(DatabaseManager::instance().initialize(), "fresh schema initialization")) return 1;
    QSqlDatabase db = DatabaseManager::instance().database();
    QSqlQuery q(db);
    const QString now = "2026-01-01T00:00:00.000Z";
    ok &= require(scalar(db, "SELECT version FROM schema_version") == 30, "fresh schema is version 30");
    ok &= require(q.exec("INSERT INTO workspace(name,description,created_utc,modified_utc) VALUES('One','', '"+now+"','"+now+"'),('Two','', '"+now+"','"+now+"')"), "workspace seed");
    ok &= require(q.exec("INSERT INTO set_catalog(set_number,name,year,theme_id,num_parts,image_url,created_utc,modified_utc) VALUES('1000-1','Test Set',2026,1,3,'set.png','"+now+"','"+now+"')"), "Set seed");
    const int setId = q.lastInsertId().toInt();
    ok &= require(q.exec("INSERT INTO minifig_catalog(name,num_parts,image_url,is_active,created_utc,modified_utc) VALUES('Test Fig',3,'fig.png',1,'"+now+"','"+now+"')"), "Minifig seed");
    const int minifigId = q.lastInsertId().toInt();
    ok &= require(q.exec("INSERT INTO minifig_external_identifier(minifig_catalog_id,provider,external_id,source,is_active,created_utc,modified_utc) VALUES("+QString::number(minifigId)+",'Rebrickable','fig-1','test',1,'"+now+"','"+now+"')"), "Minifig identity seed");
    const QString invalidBase = "INSERT INTO collection_item(workspace_id,item_type,set_catalog_id,minifig_catalog_id,state,source_build_id,allow_parts_source,is_active,created_utc,modified_utc) VALUES";
    ok &= require(!q.exec(invalidBase + "(1,'Set',NULL,NULL,'Assembled',NULL,0,1,'"+now+"','"+now+"')")
                  && !q.exec(invalidBase + "(1,'Minifig',NULL,"+QString::number(minifigId)+",'Assembled',NULL,1,1,'"+now+"','"+now+"')")
                  && !q.exec(invalidBase + "(1,'MOC',NULL,NULL,'Assembled',NULL,0,1,'"+now+"','"+now+"')")
                  && scalar(db,"SELECT COUNT(*) FROM collection_item")==0,
                  "database constraints reject invalid Set, Minifig, and MOC identities");

    const int typeId = scalar(db, "SELECT id FROM storage_location_type ORDER BY id LIMIT 1");
    StorageLocationRepository locations;
    auto makeLocation = [&](int workspace, int parent, const QString& name, bool inventory, bool collection) {
        StorageLocation location; location.setWorkspaceId(workspace); location.setParentLocationId(parent);
        location.setLocationTypeId(typeId); location.setName(name);
        location.setAllowsInventory(inventory); location.setAllowsCollection(collection);
        ok &= require(locations.create(location), "create location " + name); return location.id();
    };
    const int parent = makeLocation(1, 0, "Room", false, false);
    const int inventoryLocation = makeLocation(1, parent, "Parts", true, false);
    const int collectionLocation = makeLocation(1, parent, "Display", false, true);
    const int bothLocation = makeLocation(1, parent, "Both", true, true);
    const int otherWorkspaceLocation = makeLocation(2, 0, "Other", false, true);
    const int hiddenInventoryParent = makeLocation(1, 0, "Inventory Parent", true, false);
    makeLocation(1, hiddenInventoryParent, "Collection Child", false, true);
    const auto inventoryHierarchy = locations.getInventoryHierarchy(1);
    const auto collectionHierarchy = locations.getCollectionHierarchy(1);
    auto contains = [](const QList<StorageLocation>& list, int id) {
        for (const auto& value : list) if (value.id() == id) return true;
        return false;
    };
    ok &= require(contains(inventoryHierarchy,parent) && contains(inventoryHierarchy,inventoryLocation)
                  && contains(inventoryHierarchy,bothLocation) && !contains(inventoryHierarchy,collectionLocation),
                  "Inventory hierarchy retains ancestors and excludes Collection-only branches");
    ok &= require(!contains(inventoryHierarchy,hiddenInventoryParent),
                  "non-leaf Inventory capability is not exposed as a false destination");
    ok &= require(contains(collectionHierarchy,parent) && contains(collectionHierarchy,collectionLocation)
                  && contains(collectionHierarchy,bothLocation) && !contains(collectionHierarchy,inventoryLocation),
                  "Collection hierarchy retains ancestors and excludes Inventory-only branches");
    ok &= require(locations.isValidInventoryDestination(1, inventoryLocation)
                  && locations.isValidInventoryDestination(1, bothLocation)
                  && !locations.isValidInventoryDestination(1, collectionLocation)
                  && locations.isValidCollectionDestination(1, collectionLocation)
                  && locations.isValidCollectionDestination(1, bothLocation)
                  && !locations.isValidCollectionDestination(1, inventoryLocation)
                  && !locations.isValidCollectionDestination(1, parent), "capability and active-leaf validation");

    auto createBuild = [&](const QString& type, const QString& name, int workspace,
                           int setCatalog, int minifigCatalog, const QString& status,
                           const QString& inventoryMode = QStringLiteral("Stock")) {
        Build build; build.setWorkspaceId(workspace); build.setBuildType(type); build.setName(name);
        build.setSetNumber(type == "MOC" ? "MOC-42" : "1000-1"); build.setInventoryMode(inventoryMode);
        build.setSetCatalogId(setCatalog); build.setMinifigCatalogId(minifigCatalog); build.setStatus(status);
        ok &= require(BuildRepository().create(build), "create Build " + name); return build.id();
    };
    const int setBuild = createBuild("Set", "Set Build", 1, setId, 0, "Complete");
    const int completeSetBuild = createBuild("Set", "Complete Set Build", 1, setId, 0,
                                             "Complete", "CompleteSet");
    const int legacySetBuild = createBuild("Set", "Legacy Set Build", 1, 0, 0,
                                           "Complete", "CompleteSet");
    const int minifigBuild = createBuild("Minifig", "Fig Build", 1, 0, minifigId, "Complete");
    const int mocBuild = createBuild("MOC", "Castle MOC", 1, 0, 0, "Complete");
    const int incompleteMoc = createBuild("MOC", "Incomplete", 1, 0, 0, "Planned");
    const int otherMoc = createBuild("MOC", "Other MOC", 2, 0, 0, "Complete");

    CollectionItemService service;
    const auto setOne = service.createSet(1, setId, CollectionItemState::Sealed, collectionLocation, 0, "Sealed copy", "one");
    const auto setTwo = service.createFromBuild(1, setBuild, CollectionItemState::Assembled, bothLocation, "Display", "two");
    const auto completeSet = service.createFromBuild(1, completeSetBuild,
                                                     CollectionItemState::Assembled,
                                                     collectionLocation);
    const auto figOne = service.createMinifig(1, minifigId, CollectionItemState::Assembled, collectionLocation);
    const auto figTwo = service.createFromBuild(1, minifigBuild, CollectionItemState::Unassembled);
    const auto moc = service.createFromBuild(1, mocBuild, CollectionItemState::Assembled, bothLocation);
    ok &= require(setOne.success && setTwo.success && completeSet.success
                  && figOne.success && figTwo.success && moc.success,
                  "Set Stock/CompleteSet, Minifig, MOC, multiple-instance creation");
    ok &= require(!service.createFromBuild(1, legacySetBuild,
                                           CollectionItemState::Assembled).success,
                  "legacy Set Build remains ineligible without catalog identity inference");
    ok &= require(setOne.collectionItemId != setTwo.collectionItemId
                  && figOne.collectionItemId != figTwo.collectionItemId, "stable independent instance IDs");
    ok &= require(!service.createMocFromBuild(1, incompleteMoc, CollectionItemState::Assembled).success,
                  "incomplete Build rejected");
    ok &= require(!service.createFromBuild(1, incompleteMoc, CollectionItemState::Assembled).success,
                  "Build-origin workflow rejects non-Complete status");
    ok &= require(!service.createMocFromBuild(1, otherMoc, CollectionItemState::Assembled).success,
                  "cross-workspace Build rejected");
    ok &= require(!service.createSet(1,setId,CollectionItemState::Assembled,otherWorkspaceLocation).success,
                  "cross-workspace location rejected");
    ok &= require(!service.createSet(1,setId,CollectionItemState::Assembled,0,setBuild).success,
                  "duplicate source Build rejected");
    ok &= require(scalar(db,"SELECT allow_parts_source FROM collection_item WHERE id="+QString::number(setOne.collectionItemId))==0,
                  "allow_parts_source defaults false");
    auto updated = service.updateDetails(setOne.collectionItemId, CollectionItemState::PartiallyAssembled,
                                         bothLocation, "Changed", "independent", true);
    const auto updatedItem = CollectionRepository().getById(setOne.collectionItemId);
    ok &= require(updated.success && updatedItem && updatedItem->allowPartsSource
                  && updatedItem->state==CollectionItemState::PartiallyAssembled
                  && updatedItem->storageLocationId==bothLocation, "independent state/location update and Set opt-in groundwork");
    ok &= require(!service.updateDetails(figOne.collectionItemId,CollectionItemState::Assembled,
                                         collectionLocation,"","",true).success,
                  "Minifig parts-source opt-in rejected");

    CollectionSearchCriteria criteria; criteria.workspaceId=1; criteria.searchText="fig-1";
    ok &= require(CollectionRepository().count(criteria)==2
                  && CollectionRepository().search(criteria).size()==2,
                  "search projects Minifig identity without N+1 queries");
    criteria.searchText="Castle MOC";
    const auto mocResults=CollectionRepository().search(criteria);
    ok &= require(mocResults.size()==1 && mocResults.first().displayReference=="MOC-42"
                  && mocResults.first().displayName=="Castle MOC", "MOC Build display identity");

    ok &= require(q.exec("UPDATE minifig_catalog SET is_active=0 WHERE id="+QString::number(minifigId))
                  && BuildRepository().setActive(mocBuild,false)
                  && CollectionRepository().getById(figOne.collectionItemId).has_value()
                  && CollectionRepository().getById(moc.collectionItemId).has_value(),
                  "catalog deactivation and Build archival preserve Collection history");
    ok &= require(service.setActive(setOne.collectionItemId,false).success
                  && !CollectionRepository().getById(setOne.collectionItemId)->isActive
                  && service.setActive(setOne.collectionItemId,true).success
                  && CollectionRepository().getById(setOne.collectionItemId)->id==setOne.collectionItemId,
                  "archive/reactivate preserves identity");

    const int before = scalar(db,"SELECT COUNT(*) FROM collection_item");
    ok &= require(q.exec("CREATE TRIGGER force_collection_failure BEFORE INSERT ON collection_item BEGIN SELECT RAISE(ABORT,'forced'); END"), "rollback trigger");
    const auto failed = service.createSet(1,setId,CollectionItemState::Assembled,collectionLocation);
    ok &= require(!failed.success && scalar(db,"SELECT COUNT(*) FROM collection_item")==before,
                  "failed transaction leaves no partial Collection row");
    q.exec("DROP TRIGGER force_collection_failure");
    ok &= require(scalar(db,"SELECT COUNT(*) FROM collection_item")==6, "no hard-delete operation occurred");
    ok &= require(AutomaticBackupPolicy::versionDirectory(data,30).endsWith("v30")
                  && AutomaticBackupPolicy::versionDirectory(data,29).endsWith("v29")
                  && AutomaticBackupPolicy::versionDirectory(data,30)!=AutomaticBackupPolicy::versionDirectory(data,29),
                  "automatic backup version directories remain isolated");

    if (ok) qInfo() << "M23.9.1 Collection foundation validation passed.";
    return ok ? 0 : 1;
}
