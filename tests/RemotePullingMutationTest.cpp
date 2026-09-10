#include "../src/database/DatabaseSchema.h"
#include "../src/services/application/HostPullingMutationService.h"
#include "../src/services/application/HostWriteExecutor.h"
#include "../src/services/application/dto/RemotePullingMutationDtos.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTimer>
#include <QUuid>
#include <iostream>

namespace {
bool require(bool value, const char* message) { if (!value) std::cerr << "FAIL: " << message << '\n'; return value; }
struct Awaited { bool success=false; RemoteMutationDto::Result result; RemoteMutationDto::Error error; };
Awaited run(HostWriteExecutor& executor, const RemoteMutationDto::Metadata& metadata)
{
    Awaited value; QEventLoop loop; RemoteMutationDto::Error parseError;
    auto mutation=HostPullingMutationService::createMutation(metadata,&parseError);
    if(!mutation){value.error=parseError;return value;}
    RemoteMutationDto::RequestContext context{"builds.pulling.record",metadata.workspaceId,
        metadata.mutationId,"test-client",1,2};
    executor.enqueue(context,RemoteMutationDto::requestHash(context.operation,metadata),mutation,&loop,
        [&](const auto&r){value.success=true;value.result=r;loop.quit();},
        [&](const auto&e){value.error=e;loop.quit();});
    QTimer::singleShot(15000,&loop,&QEventLoop::quit);loop.exec();return value;
}
int scalar(QSqlDatabase& db,const QString& sql){QSqlQuery q(db);return q.exec(sql)&&q.next()?q.value(0).toInt():-1;}
}

int main(int argc,char**argv)
{
    QCoreApplication app(argc,argv);QTemporaryDir dir;
    if(!require(dir.isValid(),"temporary directory"))return 1;
    const QString path=dir.filePath("pulling.db");const QString name="pulling-test";
    QSqlDatabase db=QSqlDatabase::addDatabase("QSQLITE",name);db.setDatabaseName(path);
    if(!require(db.open()&&DatabaseSchema::initialize(db),"schema 35 database"))return 1;
    QSqlQuery q(db);const QString t="2026-01-01T00:00:00.000Z";
    const QStringList seed={
        QString("INSERT INTO workspace(id,name,created_utc,modified_utc,is_active) VALUES(1,'Workshop','%1','%1',1)").arg(t),
        QString("INSERT INTO color(id,name,rebrickable_id,created_utc,modified_utc) VALUES(1,'Red',4,'%1','%1')").arg(t),
        QString("INSERT INTO part(id,part_number,name,is_active,created_utc,modified_utc) VALUES(1,'3001','Brick',1,'%1','%1')").arg(t),
        QString("INSERT INTO storage_location(id,workspace_id,location_type_id,name,is_active,created_utc,modified_utc) VALUES(1,1,(SELECT id FROM storage_location_type LIMIT 1),'Bin',1,'%1','%1')").arg(t),
        QString("INSERT INTO storage_location(id,workspace_id,location_type_id,name,is_active,created_utc,modified_utc) VALUES(2,1,(SELECT id FROM storage_location_type LIMIT 1),'Drawer',1,'%1','%1')").arg(t),
        QString("INSERT INTO inventory_record(id,workspace_id,part_id,color_id,storage_location_id,manufacturer_id,condition,ownership_type,quantity,created_utc,modified_utc) VALUES(1,1,1,1,1,(SELECT id FROM manufacturer WHERE code='LEGO'),'Used','Owned',10,'%1','%1')").arg(t),
        QString("INSERT INTO inventory_record(id,workspace_id,part_id,color_id,storage_location_id,manufacturer_id,condition,ownership_type,quantity,created_utc,modified_utc) VALUES(2,1,1,1,2,(SELECT id FROM manufacturer WHERE code='LEGO'),'Used','Owned',6,'%1','%1')").arg(t),
        QString("INSERT INTO build(id,workspace_id,build_type,name,inventory_mode,status,is_active,created_utc,modified_utc,manufacturer_id) VALUES(1,1,'MOC','Test Build','Stock','Planned',1,'%1','%1',(SELECT id FROM manufacturer WHERE code='LEGO'))").arg(t),
        QString("INSERT INTO build_requirement(id,build_id,part_id,color_id,quantity_required,quantity_pulled,quantity_released,is_spare,created_utc,modified_utc) VALUES(1,1,1,1,5,0,0,0,'%1','%1')").arg(t),
        QString("INSERT INTO build_allocation(id,build_id,build_requirement_id,inventory_record_id,part_id,color_id,storage_location_id,quantity_allocated,created_utc,modified_utc) VALUES(1,1,1,1,1,1,1,4,'%1','%1')").arg(t),
        QString("INSERT INTO build_allocation(id,build_id,build_requirement_id,inventory_record_id,part_id,color_id,storage_location_id,quantity_allocated,created_utc,modified_utc) VALUES(2,1,1,2,1,1,2,1,'%1','%1')").arg(t)};
    for(const auto&s:seed)if(!require(q.exec(s),q.lastError().text().toUtf8().constData()))return 1;
    db.close();db={};QSqlDatabase::removeDatabase(name);

    int publications=0;HostWriteExecutor executor(path,[&](auto workflow,const auto&scope){
        if(workflow==HostMutationPublicationService::Workflow::Pulling&&scope.workspaceId==1&&scope.buildId==1)++publications;});
    RemotePullingMutationDto::Request request{1,RemoteMutationDto::newMutationId(),1,"Planned",
        {{1,2,4,10,0},{2,1,1,6,0}}};
    RemoteMutationDto::Error validationError;
    auto empty=request;empty.rows.clear();
    if(!require(!HostPullingMutationService::createMutation(
            RemotePullingMutationDto::toMetadata(empty),&validationError),"empty batch rejected"))return 1;
    auto duplicate=request;duplicate.rows[1].allocationId=duplicate.rows[0].allocationId;
    if(!require(!HostPullingMutationService::createMutation(
            RemotePullingMutationDto::toMetadata(duplicate),&validationError),"duplicate allocations rejected"))return 1;
    auto oversized=request;oversized.rows.clear();
    for(int i=0;i<=RemotePullingMutationDto::MaximumRows;++i)oversized.rows.append({i+1,1,1,1,0});
    if(!require(!HostPullingMutationService::createMutation(
            RemotePullingMutationDto::toMetadata(oversized),&validationError),"oversized batch rejected"))return 1;
    const auto metadata=RemotePullingMutationDto::toMetadata(request);
    const auto first=run(executor,metadata);
    if(!require(first.success,"remote pull succeeds")||!require(publications==1,"one invalidation"))return 1;
    const auto replay=run(executor,metadata);
    if(!require(replay.success&&replay.result.replayed,"same mutation replays")
       ||!require(publications==1,"replay does not republish"))return 1;
    auto changed=request;changed.rows[0].quantity=1;
    const auto idConflict=run(executor,RemotePullingMutationDto::toMetadata(changed));
    if(!require(!idConflict.success&&idConflict.error.code=="IDEMPOTENCY_CONFLICT","changed replay rejected"))return 1;
    executor.shutdown();

    const QString verifyName="pulling-verify";QSqlDatabase verify=QSqlDatabase::addDatabase("QSQLITE",verifyName);
    verify.setDatabaseName(path);verify.open();
    if(!require(scalar(verify,"SELECT quantity FROM inventory_record WHERE id=1")==8,"Inventory delta")
       ||!require(scalar(verify,"SELECT quantity FROM inventory_record WHERE id=2")==5,"second Inventory delta")
       ||!require(scalar(verify,"SELECT quantity_allocated FROM build_allocation WHERE id=1")==2,"allocation delta")
       ||!require(scalar(verify,"SELECT COUNT(*) FROM build_allocation WHERE id=2")==0,"completed allocation removed")
       ||!require(scalar(verify,"SELECT quantity_pulled FROM build_requirement WHERE id=1")==3,"collective requirement delta")
       ||!require(scalar(verify,"SELECT COUNT(*) FROM inventory_movement WHERE movement_type='BuildPull'")==2,"history rows")
       ||!require(scalar(verify,"SELECT quantity_pulled FROM build_part_provenance WHERE build_id=1")==3,"manufacturer provenance")
       ||!require(scalar(verify,"SELECT COUNT(*) FROM remote_mutation_receipt")==1,"one receipt"))return 1;
    verify.close();verify={};QSqlDatabase::removeDatabase(verifyName);

    HostWriteExecutor staleExecutor(path);request.mutationId=RemoteMutationDto::newMutationId();
    const auto stale=run(staleExecutor,RemotePullingMutationDto::toMetadata(request));
    if(!require(!stale.success&&stale.error.code=="CONFLICT","stale expected state conflicts"))return 1;
    RemotePullingMutationDto::Request atomic{1,RemoteMutationDto::newMutationId(),1,"Planned",
        {{1,1,2,8,3},{999,1,1,1,3}}};
    const auto rejected=run(staleExecutor,RemotePullingMutationDto::toMetadata(atomic));
    if(!require(!rejected.success&&rejected.error.code=="CONFLICT","bad batch rejected") )return 1;
    RemotePullingMutationDto::Request one{1,RemoteMutationDto::newMutationId(),1,"Planned",
        {{1,1,2,8,3}}};
    const auto oneResult=run(staleExecutor,RemotePullingMutationDto::toMetadata(one));
    if(!require(oneResult.success,"fresh one-row pull succeeds"))return 1;
    staleExecutor.shutdown();
    const QString finalName="pulling-final";QSqlDatabase finalDb=QSqlDatabase::addDatabase("QSQLITE",finalName);
    finalDb.setDatabaseName(path);finalDb.open();
    if(!require(scalar(finalDb,"SELECT quantity FROM inventory_record WHERE id=1")==7,"bad batch had no Inventory effect")
       ||!require(scalar(finalDb,"SELECT quantity_pulled FROM build_requirement WHERE id=1")==4,"fresh retry requirement total")
       ||!require(scalar(finalDb,"SELECT COUNT(*) FROM inventory_movement WHERE movement_type='BuildPull'")==3,"no bad-batch history")
       ||!require(scalar(finalDb,"SELECT COUNT(*) FROM remote_mutation_receipt")==2,"receipts only for commits"))return 1;
    finalDb.close();finalDb={};QSqlDatabase::removeDatabase(finalName);
    std::cout<<"Remote Pulling mutation tests passed.\n";return 0;
}
