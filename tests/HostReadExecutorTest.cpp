#include "../src/database/DatabaseManager.h"
#include "../src/database/DatabaseSchema.h"
#include "../src/services/application/HostReadExecutor.h"
#include "../src/services/application/HostReadProtocolService.h"
#include "../src/services/application/RemoteReadApplicationServices.h"
#include "../src/services/application/dto/RemoteReadJson.h"
#include "../src/network/BrickSuiteOperationDispatcher.h"
#include "../src/network/BrickSuiteWebSocketServer.h"
#include "../src/network/BrickSuiteWebSocketClient.h"
#include "../src/network/BrickSuiteHostIdentity.h"
#include "../src/network/RemoteSessionState.h"

#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QSqlQuery>
#include <QSqlError>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTimer>
#include <QHostAddress>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QJsonArray>
#include <cstdio>
#include <algorithm>

namespace {
bool check(bool value, const char* message)
{
    if (!value) std::fprintf(stderr, "FAILED: %s\n", message);
    return value;
}

bool waitFor(const std::function<void(QEventLoop&)>& start)
{
    QEventLoop loop;
    bool timedOut = false;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, [&]() { timedOut = true; loop.quit(); });
    timer.start(5000);
    start(loop);
    loop.exec();
    return !timedOut;
}

bool waitUntil(const std::function<bool()>& predicate, int timeoutMs = 5000)
{
    QElapsedTimer timer;
    timer.start();
    while (!predicate() && timer.elapsed() < timeoutMs)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    return predicate();
}

bool connectClient(BrickSuiteWebSocketClient& client)
{
    bool success = false;
    return waitFor([&](QEventLoop& loop) {
        QObject::connect(&client, &BrickSuiteWebSocketClient::testConnectionCompleted,
                         &loop, [&](bool value, const QString&) { success = value; loop.quit(); });
        client.connectToHost();
    }) && success;
}

bool requestClient(BrickSuiteWebSocketClient& client, const QString& operation,
                   const QJsonObject& payload, QJsonObject* response)
{
    bool success = false;
    return waitFor([&](QEventLoop& loop) {
        const QString id = client.sendRequest(operation, payload);
        QObject::connect(&client, &BrickSuiteWebSocketClient::requestCompleted, &loop,
            [&](const QString& resultId, const QJsonObject& result) {
                if (resultId != id) return; *response = result; success = true; loop.quit();
            });
        QObject::connect(&client, &BrickSuiteWebSocketClient::requestFailed, &loop,
            [&](const QString& resultId, const auto&) { if (resultId == id) loop.quit(); });
    }) && success;
}

bool seedDatabase(const QString& path, const QString& workspaceName)
{
    const QString name = QStringLiteral("HostReadSeed_%1").arg(workspaceName);
    bool ok = false;
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), name);
        database.setDatabaseName(path);
        ok = database.open() && DatabaseSchema::initialize(database);
        QSqlQuery query(database);
        query.prepare(QStringLiteral(
            "INSERT INTO workspace(name,description,created_utc,modified_utc,is_active) "
            "VALUES(:name,'','2026-01-01T00:00:00.000Z','2026-01-01T00:00:00.000Z',1)"));
        query.bindValue(QStringLiteral(":name"), workspaceName);
        ok = ok && query.exec();
        const QString now = QStringLiteral("2026-01-01T00:00:00.000Z");
        const auto execute = [&query, &ok](const QString& sql) {
            if (!query.exec(sql)) {
                std::fprintf(stderr, "Seed SQL failed: %s\n",
                             qPrintable(query.lastError().text()));
                ok = false;
            }
        };
        execute(QStringLiteral("INSERT INTO part_category(name,rebrickable_id,created_utc,modified_utc) VALUES('Host Bricks',11,'%1','%1')").arg(now));
        execute(QStringLiteral("INSERT INTO part(part_number,name,part_category_id,rebrickable_part_id,is_active,created_utc,modified_utc,material) SELECT '3001','Host Brick',id,'3001',1,'%1','%1','Plastic' FROM part_category WHERE rebrickable_id=11").arg(now));
        execute(QStringLiteral("INSERT INTO color(name,rgb,is_transparent,rebrickable_id,created_utc,modified_utc) VALUES('Host Red','C91A09',0,4,'%1','%1')").arg(now));
        execute(QStringLiteral("INSERT INTO storage_location(workspace_id,parent_location_id,location_type_id,name,description,sort_order,is_active,allows_inventory,allows_collection,created_utc,modified_utc) VALUES(1,NULL,1,'Host Bin','',0,1,1,1,'%1','%1')").arg(now));
        execute(QStringLiteral("INSERT INTO storage_location(workspace_id,parent_location_id,location_type_id,name,description,sort_order,is_active,allows_inventory,allows_collection,created_utc,modified_utc) VALUES(1,NULL,1,'Collection Display','',1,1,0,1,'%1','%1')").arg(now));
        execute(QStringLiteral("INSERT INTO storage_location(workspace_id,parent_location_id,location_type_id,name,description,sort_order,is_active,allows_inventory,allows_collection,created_utc,modified_utc) SELECT 1,id,2,'Inactive Child','',5,0,0,1,'%1','%1' FROM storage_location WHERE name='Host Bin'").arg(now));
        execute(QStringLiteral("INSERT INTO workspace(name,description,created_utc,modified_utc,is_active) VALUES('Other Workspace','','%1','%1',1)").arg(now));
        execute(QStringLiteral("INSERT INTO manufacturer(code,name,website_url,supports_lego_element_ids,is_active,notes,created_utc,modified_utc,origin) VALUES('ALT','Alternate Bricks','',0,1,'','%1','%1','User')").arg(now));
        execute(QStringLiteral("INSERT INTO manufacturer(code,name,website_url,supports_lego_element_ids,is_active,notes,created_utc,modified_utc,origin) VALUES('OLD','Inactive Bricks','',0,0,'','%1','%1','User')").arg(now));
        execute(QStringLiteral("INSERT INTO storage_location(workspace_id,parent_location_id,location_type_id,name,description,sort_order,is_active,allows_inventory,allows_collection,created_utc,modified_utc) SELECT id,NULL,1,'Other Storage','',0,1,1,0,'%1','%1' FROM workspace WHERE name='Other Workspace'").arg(now));
        execute(QStringLiteral("INSERT INTO inventory_record(workspace_id,part_id,color_id,storage_location_id,manufacturer_id,condition,ownership_type,quantity,created_utc,modified_utc) SELECT 1,p.id,c.id,s.id,1,'Used','Owned',7,'%1','%1' FROM part p,color c,storage_location s WHERE p.part_number='3001' AND c.rebrickable_id=4 AND s.name='Host Bin'").arg(now));
        execute(QStringLiteral("INSERT INTO inventory_movement(workspace_id,inventory_record_id,part_id,color_id,movement_type,quantity_change,to_storage_location_id,condition,ownership_type,reference_type,reference_id,notes,created_utc) SELECT 1,i.id,i.part_id,i.color_id,'Add',7,i.storage_location_id,'Used','Owned','Test','seed','Host history','%1' FROM inventory_record i").arg(now));
        execute(QStringLiteral("INSERT INTO build(workspace_id,build_type,name,set_number,inventory_mode,status,is_active,created_utc,modified_utc) VALUES(1,'MOC','Host Build','MOC-HOST','Stock','Planned',1,'%1','%1')").arg(now));
        query.prepare(QStringLiteral("INSERT INTO build(workspace_id,build_type,name,set_number,inventory_mode,status,is_active,created_utc,modified_utc) VALUES(1,'MOC',:name,:number,'Stock','Planned',1,:now,:now)"));
        for (int i=1;i<=500 && ok;++i) {
            query.bindValue(QStringLiteral(":name"),QStringLiteral("Paged Build %1").arg(i,3,10,QChar('0')));
            query.bindValue(QStringLiteral(":number"),QStringLiteral("MOC-PAGED-%1").arg(i,3,10,QChar('0')));
            query.bindValue(QStringLiteral(":now"),now);
            ok=query.exec();
        }
        execute(QStringLiteral("INSERT INTO collection_item(workspace_id,item_type,state,condition,completeness,storage_location_id,source_build_id,nickname,notes,allow_parts_source,is_active,created_utc,modified_utc) SELECT 1,'MOC','Assembled','Used','Complete',s.id,b.id,'Host Collection','Host notes',0,1,'%1','%1' FROM storage_location s,build b WHERE b.name='Host Build' AND s.name='Host Bin'").arg(now));
        execute(QStringLiteral("INSERT INTO build_requirement(build_id,part_id,color_id,quantity_required,quantity_pulled,quantity_released,is_spare,created_utc,modified_utc) SELECT b.id,p.id,c.id,10,0,0,0,'%1','%1' FROM build b,part p,color c WHERE b.name='Host Build' AND p.part_number='3001' AND c.rebrickable_id=4").arg(now));
        execute(QStringLiteral("INSERT INTO build_allocation(build_id,build_requirement_id,inventory_record_id,part_id,color_id,storage_location_id,quantity_allocated,created_utc,modified_utc) SELECT b.id,r.id,i.id,i.part_id,i.color_id,i.storage_location_id,3,'%1','%1' FROM build b JOIN build_requirement r ON r.build_id=b.id CROSS JOIN inventory_record i WHERE b.name='Host Build'").arg(now));
        database.close();
    }
    QSqlDatabase::removeDatabase(name);
    return ok;
}
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("RFStateSideTests"));
    app.setApplicationName(QStringLiteral("HostReadExecutor"));
    QStandardPaths::setTestModeEnabled(true);
    QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)).removeRecursively();

    QTemporaryDir directory;
    bool ok = check(directory.isValid(), "temporary directory");
    const QString hostPath = directory.filePath(QStringLiteral("host.db"));
    ok &= check(seedDatabase(hostPath, QStringLiteral("Host Workspace")), "seed Host database");
    ok &= check(DatabaseManager::instance().initialize(), "initialize poison default database");
    QSqlQuery poison(DatabaseManager::instance().database());
    ok &= check(poison.exec(QStringLiteral(
        "INSERT INTO workspace(name,description,created_utc,modified_utc,is_active) "
        "VALUES('Poison Local','','2026-01-01','2026-01-01',1)")), "seed poison local database");

    QString connectionName;
    {
        HostReadExecutor executor(hostPath);
        connectionName = executor.connectionName();
        ok &= check(connectionName.startsWith(QStringLiteral("BrickSuite_HostRead_")),
                    "unique Host connection name");
        QList<Workspace> workspaces;
        QString failure;
        ok &= check(waitFor([&](QEventLoop& loop) {
            executor.listWorkspaces(&app, [&](const QList<Workspace>& result) {
                workspaces = result; loop.quit();
            }, [&](const QString& error) { failure = error; loop.quit(); });
        }), "Workspace read completion");
        ok &= check(failure.isEmpty() && workspaces.size() == 2
                        && workspaces.first().name() == QStringLiteral("Host Workspace"),
                    "worker uses Host database, never poison default database");
        QStringList manufacturerNames;
        ok &= check(waitFor([&](QEventLoop& loop) {
            executor.listManufacturerNames(&app, [&](const QStringList& result) {
                manufacturerNames = result; loop.quit();
            }, [&](const QString& error) { failure = error; loop.quit(); });
        }), "Manufacturer choice read completion");
        ok &= check(failure.isEmpty() && manufacturerNames.contains(QStringLiteral("LEGO"))
                        && manufacturerNames.contains(QStringLiteral("Alternate Bricks"))
                        && !manufacturerNames.contains(QStringLiteral("Inactive Bricks")),
                    "only active Host manufacturer choices returned by worker-owned connection");
        std::optional<QList<RemoteReadDto::StorageSummary>> activeStorage;
        ok &= check(waitFor([&](QEventLoop& loop) {
            executor.listStoragePortable(1, false, &app, [&](const auto& result) { activeStorage=result; loop.quit(); });
        }), "active Storage read completion");
        ok &= check(activeStorage && activeStorage->size()==2
                        && std::any_of(activeStorage->cbegin(),activeStorage->cend(),[](const auto& row) {
                            return row.name==QStringLiteral("Host Bin") && row.allowsInventory && row.allowsCollection;
                        })
                        && std::any_of(activeStorage->cbegin(),activeStorage->cend(),[](const auto& row) {
                            return row.name==QStringLiteral("Collection Display") && !row.allowsInventory && row.allowsCollection;
                        })
                        && std::none_of(activeStorage->cbegin(),activeStorage->cend(),[](const auto& row) {
                            return row.name==QStringLiteral("Other Storage");
                        }), "active Storage includes Collection-only locations from only the requested Workspace");
        std::optional<QList<RemoteReadDto::StorageSummary>> fullStorage;
        ok &= check(waitFor([&](QEventLoop& loop) {
            executor.listStoragePortable(1, true, &app, [&](const auto& result) { fullStorage=result; loop.quit(); });
        }), "complete Storage read completion");
        ok &= check(fullStorage && fullStorage->size()==3
                        && std::any_of(fullStorage->cbegin(),fullStorage->cend(),[](const auto& row) {
                            return row.name==QStringLiteral("Inactive Child") && !row.active && row.parentStorageId>0 && !row.typeName.isEmpty();
                        }),
                    "complete Storage hierarchy fidelity");
        std::optional<RemoteReadDto::StorageDetail> storageDetail;
        ok &= check(waitFor([&](QEventLoop& loop){executor.getStoragePortable(1,activeStorage->first().storageId,&app,[&](const auto&result){storageDetail=result;loop.quit();});}),"Storage detail completion");
        ok &= check(storageDetail&&storageDetail->workspaceId==1&&storageDetail->storageTypeId>0
                    &&storageDetail->displayPath==QStringLiteral("Host Bin")&&storageDetail->createdUtc.isValid()
                    &&storageDetail->modifiedUtc.isValid(),"Storage detail portable projection");
        QList<RemoteReadDto::StorageType> storageTypes;
        ok &= check(waitFor([&](QEventLoop&loop){executor.listStorageTypesPortable(&app,[&](const auto&result){storageTypes=result;loop.quit();});}),"Storage type list completion");
        ok &= check(!storageTypes.isEmpty()&&std::all_of(storageTypes.cbegin(),storageTypes.cend(),[](const auto&type){return type.storageTypeId>0&&type.active;}),"only active Host Storage types returned");
        std::optional<QList<RemoteReadDto::StorageSummary>> missingStorage = QList<RemoteReadDto::StorageSummary>{};
        ok &= check(waitFor([&](QEventLoop& loop) {
            executor.listStoragePortable(999, true, &app, [&](const auto& result) { missingStorage=result; loop.quit(); });
        }) && !missingStorage, "unknown Workspace Storage rejected");

        InventorySearchCriteria inventoryCriteria;
        inventoryCriteria.workspaceId = workspaces.first().id();
        InventoryApplicationService::Page inventory;
        ok &= check(waitFor([&](QEventLoop& loop) {
            executor.searchInventory(inventoryCriteria, &app,
                [&](const InventoryApplicationService::Page& result) { inventory = result; loop.quit(); },
                [&](const QString& error) { failure = error; loop.quit(); });
        }), "Inventory read completion");
        ok &= check(inventory.rows.size() == 1 && inventory.total == 1
                        && inventory.rows.first().partNumber == QStringLiteral("3001")
                        && inventory.rows.first().rebrickableCategoryId == 11
                        && inventory.rows.first().rebrickableColorId == 4,
                    "portable Inventory search projection");

        RemoteReadDto::InventorySearchRequest portableCriteria;
        portableCriteria.workspaceId = 1;
        portableCriteria.rebrickableCategoryId = 11;
        portableCriteria.rebrickableColorId = 4;
        InventoryApplicationService::Page filteredInventory;
        ok &= check(waitFor([&](QEventLoop& loop) {
            executor.searchInventoryPortable(portableCriteria, &app,
                [&](const auto& result) { filteredInventory = result; loop.quit(); });
        }), "portable Category/Color filter completion");
        ok &= check(filteredInventory.total == 1 && filteredInventory.rows.size() == 1,
                    "provider Category/Color identities map to Host internal IDs");

        std::optional<RemoteReadDto::InventoryDetail> detail;
        const int inventoryId = inventory.rows.isEmpty() ? 0 : inventory.rows.first().inventoryRecordId;
        ok &= check(waitFor([&](QEventLoop& loop) {
            executor.getInventoryPortable(1, inventoryId, &app,
                [&](const auto& result) { detail = result; loop.quit(); });
        }), "Inventory detail completion");
        ok &= check(detail && detail->partNumber == QStringLiteral("3001")
                        && detail->rebrickableColorId == 4,
                    "Inventory detail uses canonical identities");
        std::optional<RemoteReadDto::InventoryDetail> crossWorkspaceDetail;
        ok &= check(waitFor([&](QEventLoop& loop) {
            executor.getInventoryPortable(999, inventoryId, &app,
                [&](const auto& result) { crossWorkspaceDetail = result; loop.quit(); });
        }), "cross-Workspace Inventory detail completion");
        ok &= check(!crossWorkspaceDetail, "Inventory detail cannot probe another Workspace");

        QList<RemoteReadDto::InventoryHistoryRow> history;
        ok &= check(waitFor([&](QEventLoop& loop) {
            executor.inventoryHistoryPortable(1, QStringLiteral("3001"), 4, &app,
                [&](const auto& result) { history = result; loop.quit(); });
        }), "Inventory history completion");
        ok &= check(history.size() == 1 && history.first().quantityChange == 7
                        && history.first().toStoragePath == QStringLiteral("Host Bin"),
                    "Inventory history resolves the current full Storage path");

        QList<Build> builds;
        ok &= check(waitFor([&](QEventLoop& loop) {
            executor.listBuilds(workspaces.first().id(), false, &app,
                [&](const QList<Build>& result) { builds = result; loop.quit(); });
        }), "Build read completion");
        ok &= check(builds.size() == 501, "Build projection");
        const auto hostBuild=std::find_if(builds.cbegin(),builds.cend(),[](const Build& b){return b.name()==QStringLiteral("Host Build");});
        const int buildId = hostBuild==builds.cend() ? 0 : hostBuild->id();
        RemoteReadDto::Page<RemoteReadDto::BuildSummary> buildPage1,buildPage3;
        ok &= check(waitFor([&](QEventLoop& loop){executor.listBuildsPortable(1,false,{1,250},&app,[&](const auto&r){buildPage1=r;loop.quit();});}),"first Build page completion");
        ok &= check(waitFor([&](QEventLoop& loop){executor.listBuildsPortable(1,false,{3,250},&app,[&](const auto&r){buildPage3=r;loop.quit();});}),"later Build page completion");
        ok &= check(buildPage1.totalRows==501&&buildPage1.rows.size()==250
                    &&buildPage3.totalRows==501&&buildPage3.rows.size()==1,
                    ">500 Builds are returned through bounded pages");
        std::optional<Build> wrongWorkspaceBuild;
        ok &= check(waitFor([&](QEventLoop& loop) {
            executor.getBuild(999, buildId, &app,
                [&](const auto& result) { wrongWorkspaceBuild = result; loop.quit(); });
        }), "cross-Workspace Build detail completion");
        ok &= check(!wrongWorkspaceBuild,
                    "Build detail cannot probe another Workspace");
        RemoteReadDto::Page<RemoteReadDto::BuildRequirement> requirements;
        ok &= check(waitFor([&](QEventLoop& loop) {
            executor.buildRequirementsPortable(1, buildId, {1, 250}, &app,
                [&](const auto& result) { requirements = result; loop.quit(); });
        }), "Build requirements completion");
        ok &= check(requirements.rows.size() == 1
                        && !requirements.rows.isEmpty()
                        && requirements.rows.first().partNumber == QStringLiteral("3001")
                        && requirements.rows.first().rebrickableColorId == 4
                        && requirements.rows.first().owned == 7
                        && requirements.rows.first().thisRequirementAllocated == 3
                        && requirements.rows.first().otherAllocated == 0
                        && requirements.rows.first().available == 4
                        && requirements.rows.first().missing == 3,
                    "Build requirements preserve Host authoritative availability");
        RemoteReadDto::Page<RemoteReadDto::BuildRequirement> wrongRequirements;
        ok &= check(waitFor([&](QEventLoop& loop) {
            executor.buildRequirementsPortable(999, buildId, {1, 250}, &app,
                [&](const auto& result) { wrongRequirements = result; loop.quit(); });
        }), "cross-Workspace requirements completion");
        ok &= check(!wrongRequirements.resourceFound && wrongRequirements.rows.isEmpty(),
                    "requirements cannot probe another Workspace");
        RemoteReadDto::Page<RemoteReadDto::MissingPart> missing;
        ok &= check(waitFor([&](QEventLoop& loop) {
            executor.missingPartsPortable(1, buildId, {1, 250}, &app,
                [&](const auto& result) { missing = result; loop.quit(); });
        }), "Missing Parts completion");
        ok &= check(missing.rows.size() == 1 && !missing.rows.isEmpty()
                        && missing.rows.first().rebrickableColorId == 4
                        && missing.rows.first().remaining == 10
                        && missing.rows.first().owned == 7
                        && missing.rows.first().thisBuildAllocated == 3
                        && missing.rows.first().available == 4
                        && missing.rows.first().missing == 3,
                    "Missing Parts preserves Host authoritative quantities");
        RemoteReadDto::Page<RemoteReadDto::MissingPart> wrongMissing;
        ok &= check(waitFor([&](QEventLoop& loop) {
            executor.missingPartsPortable(999, buildId, {1, 250}, &app,
                [&](const auto& result) { wrongMissing = result; loop.quit(); });
        }), "cross-Workspace Missing Parts completion");
        ok &= check(!wrongMissing.resourceFound && wrongMissing.rows.isEmpty(),
                    "Missing Parts cannot probe another Workspace");
        RemoteReadDto::Page<RemoteReadDto::PullingRow> pulling;
        ok &= check(waitFor([&](QEventLoop& loop) {
            executor.pullingPortable(1, buildId, {1, 250}, &app,
                [&](const auto& result) { pulling = result; loop.quit(); });
        }), "Pulling completion");
        ok &= check(pulling.rows.size() == 1 && !pulling.rows.isEmpty()
                        && pulling.rows.first().partNumber == QStringLiteral("3001"),
                    "Pulling portable identity");
        RemoteReadDto::Page<RemoteReadDto::PullingRow> wrongPulling;
        ok &= check(waitFor([&](QEventLoop& loop) {
            executor.pullingPortable(999, buildId, {1, 250}, &app,
                [&](const auto& result) { wrongPulling = result; loop.quit(); });
        }), "cross-Workspace Pulling completion");
        ok &= check(!wrongPulling.resourceFound && wrongPulling.rows.isEmpty(),
                    "Pulling cannot probe another Workspace");

        CollectionSearchCriteria collectionCriteria;
        collectionCriteria.workspaceId = workspaces.first().id();
        CollectionApplicationService::Page collection;
        ok &= check(waitFor([&](QEventLoop& loop) {
            executor.searchCollection(collectionCriteria, &app,
                [&](const CollectionApplicationService::Page& result) { collection = result; loop.quit(); });
        }), "Collection read completion");
        ok &= check(collection.rows.size() == 1 && collection.total == 1
                        && collection.rows.first().item.nickname == QStringLiteral("Host Collection"),
                    "Collection projection");

        ok &= check(waitUntil([&] { return executor.isIdle(); })
                        && executor.activeReadCount() == 0
                        && executor.queuedReadCount() == 0,
                    "completed read executor reports idle counters");
        executor.stopAccepting();
        ok &= check(!executor.isAccepting(), "read admission can be stopped without shutdown");
        executor.startAccepting();
        ok &= check(executor.isAccepting(), "read admission can resume");

        executor.shutdown();
        ok &= check(!executor.isAccepting(), "shutdown stops task acceptance");
    }
    ok &= check(!QSqlDatabase::contains(connectionName), "named worker connection removed");
    {
        BrickSuiteOperationDispatcher dispatcher;
        HostReadProtocolService protocol(hostPath);
        protocol.registerOperations(dispatcher);
        const QStringList expected{
            QStringLiteral("workspace.list"), QStringLiteral("storage.list"),
            QStringLiteral("storage.get"), QStringLiteral("storage.types.list"),
            QStringLiteral("inventory.search"), QStringLiteral("inventory.get"),
            QStringLiteral("inventory.history"), QStringLiteral("inventory.lost.list"), QStringLiteral("manufacturers.list"), QStringLiteral("builds.list"),
            QStringLiteral("builds.get"), QStringLiteral("builds.requirements"),
            QStringLiteral("builds.missingParts"), QStringLiteral("builds.pulling"),
            QStringLiteral("collection.search"), QStringLiteral("collection.get"),
            QStringLiteral("partReference.customizations")};
        for (const QString& operation : expected)
            ok &= check(dispatcher.operations().contains(operation),
                        qPrintable(QStringLiteral("registered operation %1").arg(operation)));
        BrickSuiteProtocol::Message denied;
        dispatcher.dispatchAsync(BrickSuiteProtocol::request(QStringLiteral("inventory.get"),
            {{QStringLiteral("inventoryRecordId"), 1}}), false,
            [&](const auto& response) { denied = response; });
        ok &= check(denied.error.code == QStringLiteral("AUTH_REQUIRED"),
                    "business reads require authentication");
        BrickSuiteProtocol::Message invalidId;
        dispatcher.dispatchAsync(BrickSuiteProtocol::request(QStringLiteral("inventory.get"),
            {{QStringLiteral("inventoryRecordId"), 0}}), true,
            [&](const auto& response) { invalidId = response; });
        ok &= check(invalidId.error.code == QStringLiteral("INVALID_REQUEST"),
                    "invalid operational ID rejected before worker submission");
    }

    {
        BrickSuiteWebSocketServer server;
        HostReadProtocolService protocol(hostPath);
        protocol.registerOperations(server.operationDispatcher());
        // Delay these Host-global reads so a Workspace generation transition can
        // be exercised while the request is outstanding.
        server.operationDispatcher().registerAsyncOperation(
            QStringLiteral("storage.types.list"), true,
            [](const BrickSuiteProtocol::Message& request, auto completion) {
                RemoteReadDto::StorageType type;
                type.storageTypeId = 1; type.name = QStringLiteral("Bin");
                QTimer::singleShot(25, [request, completion = std::move(completion), type]() mutable {
                    completion(BrickSuiteProtocol::response(request,
                        {{QStringLiteral("rows"), QJsonArray{RemoteReadJson::toJson(type)}}}));
                });
            }, 2, QStringLiteral("storage.types.list"));
        server.operationDispatcher().registerAsyncOperation(
            QStringLiteral("partReference.customizations"), true,
            [](const BrickSuiteProtocol::Message& request, auto completion) {
                RemoteReadDto::PartReferenceCustomization row;
                row.customizationId = 1; row.partNumber = QStringLiteral("3001");
                row.catalog = QStringLiteral("Bricks"); row.section = QStringLiteral("Basic");
                QTimer::singleShot(25, [request, completion = std::move(completion), row]() mutable {
                    completion(BrickSuiteProtocol::response(request,
                        {{QStringLiteral("rows"), QJsonArray{RemoteReadJson::toJson(row)}}}));
                });
            }, 2, QStringLiteral("partReference.customizations.read"));
        const auto identity = BrickSuiteHostIdentity::generateEphemeral();
        QString error;
        const QString token = QStringLiteral("m264b-loopback-token");
        ok &= check(identity.success && server.startWithIdentity(QHostAddress::LocalHost, 0,
                    token, identity, &error), "secure Host read loopback starts");
        BrickSuiteWebSocketClient client;
        client.configure(QUrl(QStringLiteral("wss://127.0.0.1:%1").arg(server.serverPort())),
                         identity.fingerprint, token, false);
        ok &= check(connectClient(client), "secure Host read loopback authenticates");
        RemoteSessionState session;
        session.authenticated(identity.fingerprint);
        session.setWorkspaceId(1);
        RemoteReadApplicationServices remote(client, &session);
        bool storageTypesAccepted = false;
        ok &= check(waitFor([&](QEventLoop& loop) {
            remote.listStorageTypes(&app, [&](const auto& result) {
                storageTypesAccepted = result.succeeded(); loop.quit();
            });
            session.setWorkspaceId(2);
        }), "Host-global Storage Types completion after Workspace transition");
        ok &= check(storageTypesAccepted,
                    "Storage Types response was incorrectly rejected by Workspace generation");
        bool customizationsAccepted = false;
        ok &= check(waitFor([&](QEventLoop& loop) {
            remote.listPartReferenceCustomizations(&app, [&](const auto& result) {
                customizationsAccepted = result.succeeded(); loop.quit();
            });
            session.setWorkspaceId(3);
        }), "Host-global Part Reference completion after Workspace transition");
        ok &= check(customizationsAccepted,
                    "Part Reference response was incorrectly rejected by Workspace generation");

        int obsoleteHostCompletions = 0;
        remote.listStorageTypes(&app, [&](const auto&) { ++obsoleteHostCompletions; });
        session.authenticated(QString(64, QLatin1Char('B')));
        { QEventLoop loop; QTimer::singleShot(100, &loop, &QEventLoop::quit); loop.exec(); }
        ok &= check(obsoleteHostCompletions == 0,
                    "Host-global response from an obsolete Host/session was accepted");
        session.authenticated(identity.fingerprint);
        QStringList remoteManufacturers;
        ok &= check(waitFor([&](QEventLoop& loop) {
            remote.listManufacturerNames(&app, [&](const auto& result) {
                if (result.succeeded()) remoteManufacturers = *result.value;
                loop.quit();
            });
        }), "remote manufacturer choices completion");
        ok &= check(remoteManufacturers.contains(QStringLiteral("LEGO"))
                        && remoteManufacturers.contains(QStringLiteral("Alternate Bricks"))
                        && !remoteManufacturers.contains(QStringLiteral("Inactive Bricks")),
                    "remote manufacturer choices preserve Host active names");
        RemoteReadDto::InventoryDetail remoteDetail;
        ok &= check(waitFor([&](QEventLoop& loop) {
            remote.getInventory(1, 1, &app, [&](const auto& result) {
                if (result.succeeded()) remoteDetail = *result.value;
                loop.quit();
            });
        }), "remote typed Inventory detail completion");
        ok &= check(remoteDetail.partNumber == QStringLiteral("3001")
                        && remoteDetail.rebrickableColorId == 4,
                    "remote typed decode preserves canonical identity");
        RemoteReadDto::CollectionSearchRequest collectionRequest;
        collectionRequest.workspaceId = 1; collectionRequest.type = QStringLiteral("MOC");
        collectionRequest.state = QStringLiteral("Assembled");
        collectionRequest.condition = QStringLiteral("Used");
        collectionRequest.completeness = QStringLiteral("Complete");
        RemoteReadDto::Page<RemoteReadDto::CollectionSummary> remoteCollection;
        ok &= check(waitFor([&](QEventLoop& loop) {
            remote.searchCollection(collectionRequest, &app, [&](const auto& result) {
                if (result.succeeded()) remoteCollection = *result.value;
                loop.quit();
            });
        }), "remote typed Collection search completion");
        ok &= check(remoteCollection.totalRows == 1 && remoteCollection.rows.size() == 1
                        && remoteCollection.rows.first().nickname == QStringLiteral("Host Collection"),
                    "remote Collection filters and portable projection");
        bool crossWorkspaceRejected = false;
        ok &= check(waitFor([&](QEventLoop& loop) {
            remote.getCollection(999, remoteCollection.rows.first().collectionItemId, &app,
                [&](const auto& result) { crossWorkspaceRejected = result.error == AsyncReadError::NotFound; loop.quit(); });
        }), "cross-Workspace Collection detail completion");
        ok &= check(crossWorkspaceRejected, "Collection detail cannot probe another Workspace");
        const QList<QPair<QString,QJsonObject>> requests{
            {"workspace.list",{}}, {"manufacturers.list",{}}, {"storage.list",{{"workspaceId",1}}},
            {"storage.get",{{"workspaceId",1},{"storageId",1}}}, {"storage.types.list",{}},
            {"inventory.search",{{"workspaceId",1},{"text",""},{"storageId",0},{"rebrickableCategoryId",-1},{"rebrickableColorId",-1},{"page",1},{"pageSize",250}}},
            {"inventory.get",{{"workspaceId",1},{"inventoryRecordId",1}}},
            {"inventory.history",{{"workspaceId",1},{"partNumber","3001"},{"rebrickableColorId",4}}},
            {"inventory.lost.list",{{"workspaceId",1}}},
            {"builds.list",{{"workspaceId",1},{"includeArchived",false},{"page",1},{"pageSize",250}}},
            {"builds.get",{{"workspaceId",1},{"buildId",1}}},
            {"builds.requirements",{{"workspaceId",1},{"buildId",1},{"page",1},{"pageSize",250}}},
            {"builds.missingParts",{{"workspaceId",1},{"buildId",1},{"page",1},{"pageSize",250}}},
            {"builds.pulling",{{"workspaceId",1},{"buildId",1},{"page",1},{"pageSize",250}}},
            {"collection.search",{{"workspaceId",1},{"text",""},{"type",""},{"state",""},{"condition",""},{"completeness",""},{"storageId",0},{"activeState",1},{"page",1},{"pageSize",100}}},
            {"collection.get",{{"workspaceId",1},{"collectionItemId",1}}},
            {"partReference.customizations",{}}};
        for (const auto& request : requests) {
            QJsonObject response;
            QElapsedTimer timer;
            timer.start();
            ok &= check(requestClient(client, request.first, request.second, &response),
                        qPrintable(QStringLiteral("secure loopback %1").arg(request.first)));
            std::printf("Remote read %s loopbackMs=%lld responseBytes=%lld\n",
                        qPrintable(request.first), static_cast<long long>(timer.elapsed()),
                        static_cast<long long>(QJsonDocument(response).toJson(QJsonDocument::Compact).size()));
        }
        client.disconnectFromHost(); server.stop();
    }
    DatabaseManager::instance().close();
    QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)).removeRecursively();
    return ok ? 0 : 1;
}
