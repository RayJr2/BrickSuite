#include "../src/database/DatabaseSchema.h"
#include "../src/services/application/HostInventoryMutationService.h"
#include "../src/services/application/HostMutationProtocolService.h"
#include "../src/services/application/RemoteInventoryMutationApplicationService.h"
#include "../src/services/application/RemoteMutationApplicationServices.h"
#include "../src/services/application/dto/RemoteInventoryMutationDtos.h"
#include "../src/network/BrickSuiteHostIdentity.h"
#include "../src/network/BrickSuiteWebSocketClient.h"
#include "../src/network/BrickSuiteWebSocketServer.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QHostAddress>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTimer>
#include <QUuid>
#include <cstdio>

namespace {
bool require(bool value, const char* message) { if(!value) std::fprintf(stderr,"%s\n",message); return value; }
QVariant scalar(const QSqlDatabase& db,const QString& sql){QSqlQuery q(db);return q.exec(sql)&&q.next()?q.value(0):QVariant();}
RemoteInventoryMutationDto::ExpectedState expected(const QSqlDatabase& db,int id)
{
    QSqlQuery q(db);q.prepare("SELECT quantity,storage_location_id,modified_utc FROM inventory_record WHERE id=?");
    q.addBindValue(id);q.exec();q.next();
    return {id,q.value(0).toInt(),q.value(1).toInt(),q.value(2).toString(),{ },-1,{ },{ },{ }};
}
HostWriteExecutor::MutationOutcome execute(const QSqlDatabase& db,const QString& operation,
                                            const RemoteInventoryMutationDto::Request& request)
{
    RemoteMutationDto::Error error;
    auto mutation=HostInventoryMutationService::createMutation(
        operation,RemoteInventoryMutationDto::toMetadata(request),&error);
    if(!mutation) return {};
    return mutation(db);
}
bool decodes(const QString& operation,const RemoteInventoryMutationDto::Request& request,
             const HostWriteExecutor::MutationOutcome& outcome,bool replayed=false)
{
    RemoteMutationDto::Result wire;wire.mutationId=request.mutationId;wire.operation=operation;
    wire.replayed=replayed;wire.authoritative=outcome.authoritative;
    RemoteInventoryMutationDto::Result result;RemoteMutationDto::Error error;
    return RemoteInventoryMutationDto::resultFromMutation(wire,&result,&error)
        && result.operation==operation&&result.mutationId==request.mutationId
        && result.replayed==replayed;
}
bool connectClient(BrickSuiteWebSocketClient& client)
{
    bool connected=false;QEventLoop loop;QTimer timeout;timeout.setSingleShot(true);
    QObject::connect(&timeout,&QTimer::timeout,&loop,&QEventLoop::quit);
    QObject::connect(&client,&BrickSuiteWebSocketClient::testConnectionCompleted,&loop,
        [&](bool success,const QString&){connected=success;loop.quit();});
    timeout.start(5000);client.connectToHost();loop.exec();return connected;
}
}

int main(int argc,char** argv)
{
    QCoreApplication app(argc,argv);QTemporaryDir temporary;
    const QString connection="remote-inventory-"+QUuid::createUuid().toString();
    QSqlDatabase db=QSqlDatabase::addDatabase("QSQLITE",connection);
    db.setDatabaseName(temporary.filePath("inventory.db"));
    bool ok=require(db.open()&&DatabaseSchema::initialize(db),"schema initialization failed");
    QSqlQuery q(db);
    ok&=require(q.exec("INSERT INTO workspace(name,description,created_utc,modified_utc,is_active) VALUES('Remote','',CURRENT_TIMESTAMP,CURRENT_TIMESTAMP,1)"),"workspace seed");
    const int workspace=q.lastInsertId().toInt();
    const int type=scalar(db,"SELECT id FROM storage_location_type WHERE is_active=1 LIMIT 1").toInt();
    auto storage=[&](const char* name){q.prepare("INSERT INTO storage_location(workspace_id,location_type_id,name,is_active,allows_inventory,created_utc,modified_utc) VALUES(?,?,?,1,1,CURRENT_TIMESTAMP,CURRENT_TIMESTAMP)");q.addBindValue(workspace);q.addBindValue(type);q.addBindValue(name);return q.exec()?q.lastInsertId().toInt():0;};
    const int firstStorage=storage("First"),secondStorage=storage("Second");
    q.exec("INSERT INTO color(name,rgb,is_transparent,rebrickable_id,created_utc,modified_utc) VALUES('Remote Color','112233',0,765432,CURRENT_TIMESTAMP,CURRENT_TIMESTAMP)");
    auto part=[&](const char* number){q.prepare("INSERT INTO part(part_number,name,is_active,created_utc,modified_utc,material) VALUES(?,?,1,CURRENT_TIMESTAMP,CURRENT_TIMESTAMP,'Plastic')");q.addBindValue(number);q.addBindValue(number);return q.exec()?q.lastInsertId().toInt():0;};
    part("remote-a");part("remote-b");
    const QString manufacturer=scalar(db,"SELECT name FROM manufacturer WHERE is_active=1 LIMIT 1").toString();
    auto base=[&](){RemoteInventoryMutationDto::Request r;r.workspaceId=workspace;r.mutationId=RemoteMutationDto::newMutationId();r.partNumber="remote-a";r.colorExternalId=765432;r.manufacturerName=manufacturer;r.storageLocationId=firstStorage;r.quantity=5;r.condition="Used";r.ownershipType="Owned";return r;};

    auto request=base();db.transaction();auto outcome=execute(db,"inventory.add",request);ok&=require(outcome.success&&decodes("inventory.add",request,outcome),"add failed or result did not decode");db.commit();
    const int record=int(outcome.authoritative.value("survivingRecordId").toDouble());
    ok&=require(record>0&&outcome.authoritative.value("created").toBool(),"add result invalid");

    request=base();request.inventoryRecordId=record;request.expected=expected(db,record);request.quantity=6;
    db.transaction();outcome=execute(db,"inventory.edit",request);ok&=require(outcome.success&&decodes("inventory.edit",request,outcome),"edit failed or result did not decode");db.commit();
    request=base();request.inventoryRecordId=record;request.expected=expected(db,record);request.quantity=2;request.destinationStorageLocationId=secondStorage;
    db.transaction();outcome=execute(db,"inventory.move",request);ok&=require(outcome.success&&decodes("inventory.move",request,outcome),"move failed or result did not decode");db.commit();
    const int destination=int(outcome.authoritative.value("destinationRecordId").toDouble());

    request=base();request.inventoryRecordId=record;request.expected=expected(db,record);request.quantity=1;request.partNumber="remote-b";
    db.transaction();outcome=execute(db,"inventory.correct",request);ok&=require(outcome.success&&decodes("inventory.correct",request,outcome),"correct failed or result did not decode");db.commit();
    request=base();request.inventoryRecordId=record;request.expected=expected(db,record);request.quantity=1;
    db.transaction();outcome=execute(db,"inventory.remove",request);ok&=require(outcome.success&&decodes("inventory.remove",request,outcome),"remove failed or result did not decode");db.commit();
    request=base();request.inventoryRecordId=record;request.expected=expected(db,record);request.quantity=1;
    db.transaction();outcome=execute(db,"inventory.markLost",request);ok&=require(outcome.success&&decodes("inventory.markLost",request,outcome),"mark Lost failed or result did not decode");db.commit();
    request=base();request.quantity=1;request.destinationStorageLocationId=secondStorage;
    db.transaction();outcome=execute(db,"inventory.markFound",request);ok&=require(outcome.success&&decodes("inventory.markFound",request,outcome),"mark Found failed or result did not decode");db.commit();
    ok&=require(decodes("inventory.markFound",request,outcome,true),"replayed Inventory success did not decode");

    ok&=require(scalar(db,QString("SELECT quantity FROM inventory_record WHERE id=%1").arg(record)).toInt()==1,"source quantity incorrect");
    ok&=require(scalar(db,QString("SELECT quantity FROM inventory_record WHERE id=%1").arg(destination)).toInt()==3,"Found did not merge into move destination");
    ok&=require(scalar(db,"SELECT COUNT(*) FROM inventory_movement WHERE movement_type IN ('InitialAdd','QuantityIncrease','Move','CorrectionRemoved','CorrectionAdded','EntryRemoved','Lost','Found')").toInt()==8,"movement history incomplete");

    request=base();request.inventoryRecordId=record;request.expected=expected(db,record);request.expected.quantity=999;request.quantity=1;
    db.transaction();outcome=execute(db,"inventory.remove",request);ok&=require(!outcome.success&&outcome.error.code=="STALE_VERSION","stale conflict not rejected");db.rollback();

    request=base();request.inventoryRecordId=record;request.expected=expected(db,record);request.quantity=2;
    db.close();db={};QSqlDatabase::removeDatabase(connection);

    const auto identity=BrickSuiteHostIdentity::generateEphemeral();
    BrickSuiteWebSocketServer server;QString serverError;
    const QString token="remote-inventory-correlation-test-token";
    ok&=require(identity.success&&server.startWithIdentity(QHostAddress::LocalHost,0,token,identity,&serverError),
                "correlation loopback server failed");
    HostMutationProtocolService protocol(temporary.filePath("inventory.db"),
        [&](auto workflow,const auto& scope){
            const auto invalidation=HostMutationPublicationService::invalidationFor(workflow,scope);
            QMetaObject::invokeMethod(&server,[&server,invalidation]{server.broadcastInvalidation(invalidation);},Qt::QueuedConnection);
        });
    const QStringList operations{"inventory.add","inventory.edit","inventory.move","inventory.correct",
        "inventory.remove","inventory.markLost","inventory.markFound"};
    for(const QString& operation:operations)protocol.registerOperation(server.operationDispatcher(),operation,operation,
        [operation](const auto& metadata,auto* error){return HostInventoryMutationService::createMutation(operation,metadata,error);});
    BrickSuiteWebSocketClient client;client.configure(QUrl(QString("wss://127.0.0.1:%1").arg(server.serverPort())),identity.fingerprint,token,false);
    ok&=require(connectClient(client),"correlation loopback client failed to authenticate");
    RemoteMutationApplicationServices mutations(client);RemoteInventoryMutationApplicationService inventory(mutations);
    bool completed=false,failed=false,timedOut=false,frameReceived=false;
    bool invalidationReceived=false,responseCompletedBeforeSlowRefresh=false;
    QString transportRequestId;QEventLoop loop;
    QObject::connect(&client,&BrickSuiteWebSocketClient::requestCompleted,&loop,
        [&](const QString& id,const QJsonObject&){if(id==transportRequestId)frameReceived=true;});
    QObject::connect(&client,&BrickSuiteWebSocketClient::invalidationReceived,&loop,
        [&](const OperationalInvalidation&,quint64){
            invalidationReceived=true;
            responseCompletedBeforeSlowRefresh=completed;
            // Model an expensive synchronous projection refresh. The correlated
            // response must already have completed before invalidation consumers run.
            QElapsedTimer slowRefresh;slowRefresh.start();
            while(slowRefresh.elapsed()<250){}
        });
    transportRequestId=inventory.edit(request,&loop,[&](const auto&){completed=true;loop.quit();},
        [&](const auto& error){failed=true;timedOut=error.outcome==RemoteMutationDto::Outcome::Unknown;loop.quit();});
    QTimer::singleShot(5000,&loop,&QEventLoop::quit);loop.exec();
    ok&=require(completed&&!failed&&!timedOut,"correlated Edit did not complete definitively")
        &&require(frameReceived,"Client did not receive the correlated Edit response frame")
        &&require(invalidationReceived,"Edit invalidation was not delivered")
        &&require(responseCompletedBeforeSlowRefresh,
                   "slow invalidation refresh started before the correlated Edit completed")
        &&require(client.pendingRequestCountForTesting()==0,"successful Edit remained in pending request table");
    QEventLoop settle;QTimer::singleShot(100,&settle,&QEventLoop::quit);settle.exec();
    ok&=require(!failed&&client.pendingRequestCountForTesting()==0,"completed Edit later entered timeout path");
    client.disconnectFromHost();server.stop();
    return ok?0:1;
}
