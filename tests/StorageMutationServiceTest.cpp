#include "../src/database/DatabaseSchema.h"
#include "../src/services/application/HostStorageMutationService.h"
#include "../src/services/application/HostStorageProtocolMutationService.h"
#include "../src/services/application/dto/RemoteStorageMutationDtos.h"
#include "../src/repositories/StorageLocationRepository.h"

#include <QCoreApplication>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>

#include <cstdio>

namespace {
bool require(bool condition, const char* message)
{
    if (!condition) std::fprintf(stderr, "FAILED: %s\n", message);
    return condition;
}

int scalar(const QSqlDatabase& database, const QString& sql)
{
    QSqlQuery query(database);
    return query.exec(sql) && query.next() ? query.value(0).toInt() : -1;
}

struct Fixture {
    QTemporaryDir directory;
    QString name = QStringLiteral("storage-mutation-%1")
                       .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    QSqlDatabase database;
    int workspace = 0;
    int otherWorkspace = 0;
    int type = 0;
    int inactiveType = 0;
    int alternateType = 0;

    bool initialize()
    {
        database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), name);
        database.setDatabaseName(directory.filePath(QStringLiteral("storage.db")));
        if (!database.open() || !DatabaseSchema::initialize(database)) return false;
        QSqlQuery q(database);
        if (!q.exec("INSERT INTO workspace(name,description,is_active,created_utc,modified_utc) "
                    "VALUES('Primary','',1,CURRENT_TIMESTAMP,CURRENT_TIMESTAMP)")) return false;
        workspace = q.lastInsertId().toInt();
        if (!q.exec("INSERT INTO workspace(name,description,is_active,created_utc,modified_utc) "
                    "VALUES('Other','',1,CURRENT_TIMESTAMP,CURRENT_TIMESTAMP)")) return false;
        otherWorkspace = q.lastInsertId().toInt();
        type = scalar(database, "SELECT id FROM storage_location_type WHERE is_active=1 ORDER BY id LIMIT 1");
        if (!q.exec("INSERT INTO storage_location_type(name,description,is_system,is_active,sort_order) "
                    "VALUES('Inactive test type','',0,0,9999)")) return false;
        inactiveType = q.lastInsertId().toInt();
        if (!q.exec("INSERT INTO storage_location_type(name,description,is_system,is_active,sort_order) "
                    "VALUES('Alternate test type','',0,1,9998)")) return false;
        alternateType = q.lastInsertId().toInt();
        return workspace > 0 && otherWorkspace > 0 && type > 0
               && inactiveType > 0 && alternateType > 0;
    }
    void close()
    {
        database.close(); database = {}; QSqlDatabase::removeDatabase(name);
    }
};

HostStorageMutationService::AddRequest addRequest(int workspace, int type, const QString& name)
{
    HostStorageMutationService::AddRequest request;
    request.workspaceId = workspace; request.storageTypeId = type; request.name = name;
    request.allowsInventory = true; request.allowsCollection = true; return request;
}

HostStorageMutationService::EditRequest editRequest(const StorageLocation& location)
{
    HostStorageMutationService::EditRequest request;
    request.workspaceId = location.workspaceId(); request.storageId = location.id();
    request.parentStorageId = location.parentLocationId();
    request.storageTypeId = location.locationTypeId(); request.name = location.name();
    request.description = location.description(); request.allowsInventory = location.allowsInventory();
    request.allowsCollection = location.allowsCollection();
    request.expected = HostStorageMutationService::expectedState(location); return request;
}
}

int main(int argc, char** argv)
{
    QCoreApplication application(argc, argv); bool ok = true;
    QSqlDatabase defaultDatabase = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"));
    defaultDatabase.setDatabaseName(QStringLiteral(":memory:"));
    ok &= require(defaultDatabase.open(), "default isolation database opens");
    QSqlQuery defaultQuery(defaultDatabase);
    ok &= require(defaultQuery.exec("CREATE TABLE isolation_probe(value INTEGER)")
                  && defaultQuery.exec("INSERT INTO isolation_probe VALUES(35)"), "default sentinel seeded");

    Fixture fixture;
    if (!require(fixture.initialize(), "fixture initialized")) return 1;
    HostStorageMutationService service(fixture.database);
    QSqlQuery q(fixture.database);

    auto root = service.add(addRequest(fixture.workspace, fixture.type, QStringLiteral("Room")));
    ok &= require(root.success && root.location.parentLocationId() == 0
                  && root.location.allowsInventory() && root.location.allowsCollection()
                  && root.displayPath == QStringLiteral("Room"), "root creation and result");
    auto childRequest = addRequest(fixture.workspace, fixture.type, QStringLiteral("Shelf"));
    childRequest.parentStorageId = root.location.id(); childRequest.description = QStringLiteral("Middle");
    auto child = service.add(childRequest);
    ok &= require(child.success && child.location.description() == QStringLiteral("Middle")
                  && child.displayPath == QStringLiteral("Room / Shelf"), "child creation and path");

    auto other = service.add(addRequest(fixture.otherWorkspace, fixture.type, QStringLiteral("Other")));
    auto invalidParent = addRequest(fixture.workspace, fixture.type, QStringLiteral("Bad"));
    invalidParent.parentStorageId = other.location.id();
    ok &= require(service.add(invalidParent).error.code == HostStorageMutationService::ErrorCode::ParentWrongWorkspace,
                  "cross-workspace parent rejected");
    invalidParent.parentStorageId = 999999;
    ok &= require(service.add(invalidParent).error.code == HostStorageMutationService::ErrorCode::ParentMissing,
                  "missing parent rejected");
    auto inactiveType = addRequest(fixture.workspace, fixture.inactiveType, QStringLiteral("Bad type"));
    ok &= require(service.add(inactiveType).error.code == HostStorageMutationService::ErrorCode::TypeInactive,
                  "inactive type rejected");
    inactiveType.storageTypeId = 999999;
    ok &= require(service.add(inactiveType).error.code == HostStorageMutationService::ErrorCode::TypeMissing,
                  "missing type rejected");
    ok &= require(q.exec(QStringLiteral("UPDATE workspace SET is_active=0 WHERE id=%1")
                             .arg(fixture.otherWorkspace)), "inactive Workspace arranged");
    ok &= require(service.add(addRequest(fixture.otherWorkspace, fixture.type, QStringLiteral("Bad Workspace"))).error.code
                      == HostStorageMutationService::ErrorCode::WorkspaceInactive,
                  "inactive Workspace rejected");
    ok &= require(q.exec(QStringLiteral("UPDATE workspace SET is_active=1 WHERE id=%1")
                             .arg(fixture.otherWorkspace)), "other Workspace restored");

    auto self = editRequest(child.location); self.parentStorageId = child.location.id();
    ok &= require(service.edit(self).error.code == HostStorageMutationService::ErrorCode::SelfParent,
                  "self parent rejected");
    auto cycle = editRequest(root.location); cycle.parentStorageId = child.location.id();
    ok &= require(service.edit(cycle).error.code == HostStorageMutationService::ErrorCode::DescendantCycle,
                  "descendant cycle rejected");

    auto edit = editRequest(child.location); edit.name = QStringLiteral("Bin");
    edit.description = QStringLiteral("Renamed"); edit.parentStorageId = 0;
    auto edited = service.edit(edit);
    ok &= require(edited.success && edited.displayPath == QStringLiteral("Bin")
                  && edited.location.description() == QStringLiteral("Renamed"), "edit and child-to-root");
    auto reparent = editRequest(edited.location); reparent.parentStorageId = root.location.id();
    reparent.storageTypeId = fixture.alternateType;
    reparent.allowsInventory = false;
    auto reparented = service.edit(reparent);
    ok &= require(reparented.success && reparented.displayPath == QStringLiteral("Room / Bin"),
                  "root-to-child reparent");
    ok &= require(reparented.location.locationTypeId() == fixture.alternateType
                  && !reparented.location.allowsInventory(), "type and capability edit");

    auto grandchildRequest = addRequest(fixture.workspace, fixture.type, QStringLiteral("Drawer"));
    grandchildRequest.parentStorageId = reparented.location.id();
    auto grandchild = service.add(grandchildRequest);
    auto deepCycle = editRequest(root.location); deepCycle.parentStorageId = grandchild.location.id();
    ok &= require(service.edit(deepCycle).error.code == HostStorageMutationService::ErrorCode::DescendantCycle,
                  "deep descendant cycle rejected");
    auto grandchildOff = service.setActive({fixture.workspace, grandchild.location.id(), false,
        HostStorageMutationService::expectedState(grandchild.location)});
    ok &= require(grandchildOff.success, "grandchild deactivated after cycle test");

    auto renameParent = editRequest(root.location); renameParent.name = QStringLiteral("Bedroom");
    auto renamedParent = service.edit(renameParent);
    auto childReload = service.edit(editRequest(reparented.location));
    ok &= require(renamedParent.success && childReload.success
                  && childReload.displayPath == QStringLiteral("Bedroom / Bin"), "derived descendant path changes");

    ok &= require(q.exec(QStringLiteral("UPDATE storage_location SET is_active=0 WHERE id=%1").arg(other.location.id())),
                  "inactive parent arranged");
    auto inactiveParent = addRequest(fixture.otherWorkspace, fixture.type, QStringLiteral("Below inactive"));
    inactiveParent.parentStorageId = other.location.id();
    ok &= require(service.add(inactiveParent).error.code == HostStorageMutationService::ErrorCode::ParentInactive,
                  "inactive parent rejected");

    auto deactivateParent = HostStorageMutationService::SetActiveRequest{fixture.workspace,
        renamedParent.location.id(), false, HostStorageMutationService::expectedState(renamedParent.location)};
    ok &= require(service.setActive(deactivateParent).error.code == HostStorageMutationService::ErrorCode::ActiveChildren,
                  "active child blocks deactivation");

    auto leaf = service.add(addRequest(fixture.workspace, fixture.type, QStringLiteral("Occupied")));
    const int manufacturer = scalar(fixture.database, "SELECT id FROM manufacturer WHERE is_active=1 LIMIT 1");
    ok &= require(q.exec("INSERT INTO color(name,rgb,is_transparent,rebrickable_id,created_utc,modified_utc) "
                         "VALUES('Storage Test','000000',0,987654,CURRENT_TIMESTAMP,CURRENT_TIMESTAMP)"), "color seeded");
    const int color = q.lastInsertId().toInt();
    ok &= require(q.exec("INSERT INTO part(part_number,name,material,is_active,created_utc,modified_utc) "
                         "VALUES('storage-test','Storage Test','Plastic',1,CURRENT_TIMESTAMP,CURRENT_TIMESTAMP)"), "part seeded");
    const int part = q.lastInsertId().toInt();
    q.prepare("INSERT INTO inventory_record(workspace_id,part_id,color_id,storage_location_id,manufacturer_id,condition,ownership_type,quantity,created_utc,modified_utc) VALUES(?,?,?,?,?,'Used','Owned',1,CURRENT_TIMESTAMP,CURRENT_TIMESTAMP)");
    for (int value : {fixture.workspace, part, color, leaf.location.id(), manufacturer}) q.addBindValue(value);
    ok &= require(q.exec(), "inventory occupancy seeded");
    auto removeInventory = editRequest(leaf.location); removeInventory.allowsInventory = false;
    ok &= require(service.edit(removeInventory).error.code == HostStorageMutationService::ErrorCode::InventoryOccupied,
                  "inventory capability removal blocked");

    ok &= require(q.exec("INSERT INTO set_catalog(set_number,name,year,theme_id,num_parts,image_url,created_utc,modified_utc) VALUES('storage-1','Storage Set',2026,1,1,'',CURRENT_TIMESTAMP,CURRENT_TIMESTAMP)"), "Set seeded");
    const int setId = q.lastInsertId().toInt();
    q.prepare("INSERT INTO collection_item(workspace_id,item_type,set_catalog_id,state,condition,completeness,storage_location_id,allow_parts_source,is_active,created_utc,modified_utc) VALUES(?,'Set',?,'Assembled','Used','Unknown',?,0,1,CURRENT_TIMESTAMP,CURRENT_TIMESTAMP)");
    q.addBindValue(fixture.workspace); q.addBindValue(setId); q.addBindValue(leaf.location.id());
    ok &= require(q.exec(), "Collection occupancy seeded");
    auto removeCollection = editRequest(leaf.location); removeCollection.allowsCollection = false;
    ok &= require(service.edit(removeCollection).error.code == HostStorageMutationService::ErrorCode::CollectionOccupied,
                  "Collection capability removal blocked");
    auto deactivateLeaf = HostStorageMutationService::SetActiveRequest{fixture.workspace, leaf.location.id(), false,
        HostStorageMutationService::expectedState(leaf.location)};
    ok &= require(service.setActive(deactivateLeaf).error.code == HostStorageMutationService::ErrorCode::InventoryOccupied,
                  "occupied location deactivation blocked");
    ok &= require(q.exec(QStringLiteral("DELETE FROM inventory_record WHERE storage_location_id=%1")
                             .arg(leaf.location.id())), "inventory occupancy removed");
    ok &= require(service.setActive(deactivateLeaf).error.code == HostStorageMutationService::ErrorCode::CollectionOccupied,
                  "Collection occupancy blocks deactivation");

    ok &= require(fixture.database.transaction(), "query-failure transaction begins");
    ok &= require(q.exec("ALTER TABLE collection_item RENAME TO collection_item_unavailable"),
                  "Collection table hidden for failure test");
    ok &= require(service.edit(removeCollection).error.code == HostStorageMutationService::ErrorCode::DatabaseFailure,
                  "Collection occupancy query failure fails closed");
    ok &= require(fixture.database.rollback(), "query-failure fixture rollback");

    ok &= require(fixture.database.transaction(), "hierarchy failure transaction begins");
    ok &= require(q.exec("ALTER TABLE storage_location RENAME TO storage_location_unavailable"),
                  "Storage table hidden for hierarchy failure test");
    StorageLocationRepository unavailableRepository(fixture.database);
    ok &= require(unavailableRepository.isDescendantChecked(1, 2)
                      == StorageLocationRepository::CheckResult::Error,
                  "hierarchy query failure is distinguishable and fails closed");
    ok &= require(fixture.database.rollback(), "hierarchy failure fixture rollback");

    auto empty = service.add(addRequest(fixture.workspace, fixture.type, QStringLiteral("Empty")));
    ok &= require(fixture.database.transaction(), "outer transaction begins");
    auto rolled = service.add(addRequest(fixture.workspace, fixture.type, QStringLiteral("Rolled back")));
    ok &= require(rolled.success && fixture.database.rollback(), "caller rollback succeeds");
    ok &= require(scalar(fixture.database, "SELECT COUNT(*) FROM storage_location WHERE name='Rolled back'") == 0,
                  "service does not commit caller transaction");
    ok &= require(fixture.database.transaction(), "commit transaction begins");
    auto committed = service.add(addRequest(fixture.workspace, fixture.type, QStringLiteral("Committed")));
    ok &= require(committed.success && fixture.database.commit(), "caller commit succeeds");
    ok &= require(scalar(fixture.database, "SELECT COUNT(*) FROM storage_location WHERE name='Committed'") == 1,
                  "caller commit persists service mutation");
    StorageLocationRepository namedRepository(fixture.database);
    const auto beforeEditRollback = namedRepository.getById(committed.location.id());
    ok &= require(fixture.database.transaction(), "edit rollback transaction begins");
    auto rollbackEdit = editRequest(*beforeEditRollback); rollbackEdit.name = QStringLiteral("Edited then rolled back");
    ok &= require(service.edit(rollbackEdit).success && fixture.database.rollback(),
                  "edit succeeds inside caller transaction and rolls back");
    ok &= require(namedRepository.getById(committed.location.id())->name() == QStringLiteral("Committed"),
                  "edit rollback restores Storage state");
    const auto beforeActiveRollback = namedRepository.getById(committed.location.id());
    ok &= require(fixture.database.transaction(), "active rollback transaction begins");
    ok &= require(service.setActive({fixture.workspace, committed.location.id(), false,
        HostStorageMutationService::expectedState(*beforeActiveRollback)}).success
                      && fixture.database.rollback(),
                  "active-state mutation succeeds inside caller transaction and rolls back");
    ok &= require(namedRepository.getById(committed.location.id())->isActive(),
                  "active-state rollback restores Storage state");
    auto deactivateEmpty = HostStorageMutationService::SetActiveRequest{fixture.workspace, empty.location.id(), false,
        HostStorageMutationService::expectedState(empty.location)};
    auto inactive = service.setActive(deactivateEmpty);
    ok &= require(inactive.success && !inactive.location.isActive(), "deactivate succeeds");
    auto reactivate = HostStorageMutationService::SetActiveRequest{fixture.workspace, empty.location.id(), true,
        HostStorageMutationService::expectedState(inactive.location)};
    ok &= require(service.setActive(reactivate).success, "reactivate succeeds");

    auto inactiveParentRoot = service.add(addRequest(fixture.workspace, fixture.type, QStringLiteral("Inactive parent")));
    auto inactiveChildRequest = addRequest(fixture.workspace, fixture.type, QStringLiteral("Inactive child"));
    inactiveChildRequest.parentStorageId = inactiveParentRoot.location.id();
    auto inactiveChild = service.add(inactiveChildRequest);
    auto childOff = service.setActive({fixture.workspace, inactiveChild.location.id(), false,
        HostStorageMutationService::expectedState(inactiveChild.location)});
    auto parentOff = service.setActive({fixture.workspace, inactiveParentRoot.location.id(), false,
        HostStorageMutationService::expectedState(inactiveParentRoot.location)});
    ok &= require(childOff.success && parentOff.success, "inactive hierarchy arranged");
    ok &= require(service.setActive({fixture.workspace, inactiveChild.location.id(), true,
        HostStorageMutationService::expectedState(childOff.location)}).error.code
                      == HostStorageMutationService::ErrorCode::ParentInactive,
                  "reactivation beneath inactive parent rejected");

    auto stale = editRequest(empty.location); stale.name = QStringLiteral("Stale");
    ok &= require(service.edit(stale).error.code == HostStorageMutationService::ErrorCode::StaleExpectedState,
                  "stale expected state rejected");
    ok &= require(scalar(defaultDatabase, "SELECT value FROM isolation_probe") == 35,
                  "default connection untouched");

    RemoteStorageMutationDto::Request protocolAdd; protocolAdd.workspaceId=fixture.workspace;
    protocolAdd.mutationId=RemoteMutationDto::newMutationId();protocolAdd.name=QStringLiteral("Protocol root");
    protocolAdd.description=QStringLiteral("Typed DTO");protocolAdd.storageTypeId=fixture.type;
    protocolAdd.allowsInventory=true;protocolAdd.allowsCollection=false;
    const auto addMetadata=RemoteStorageMutationDto::toMetadata(QStringLiteral("storage.add"),protocolAdd);
    RemoteMutationDto::Error dtoError;auto mutation=HostStorageProtocolMutationService::createMutation(QStringLiteral("storage.add"),addMetadata,&dtoError);
    ok &= require(bool(mutation)&&fixture.database.transaction(),"protocol add accepted on caller transaction");
    const auto protocolResult=mutation(fixture.database);
    ok &= require(protocolResult.success&&protocolResult.authoritative.value("created").toBool()
                  &&protocolResult.authoritative.value("storage").toObject().value("displayPath").toString()==QStringLiteral("Protocol root"),"protocol add returns authoritative detail");
    ok &= require(fixture.database.rollback(),"protocol adapter leaves commit ownership to executor");
    auto malformed=addMetadata;malformed.mutation.insert(QStringLiteral("unknown"),true);
    ok &= require(!HostStorageProtocolMutationService::createMutation(QStringLiteral("storage.add"),malformed,&dtoError)
                  &&dtoError.code==QStringLiteral("INVALID_ARGUMENT"),"unknown Storage mutation fields rejected");

    fixture.close(); defaultDatabase.close(); defaultDatabase = {};
    QSqlDatabase::removeDatabase(QSqlDatabase::defaultConnection);
    if (ok) std::puts("StorageMutationServiceTest passed");
    return ok ? 0 : 1;
}
