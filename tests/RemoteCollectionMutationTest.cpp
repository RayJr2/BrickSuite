#include "../src/database/DatabaseSchema.h"
#include "../src/repositories/CollectionRepository.h"
#include "../src/services/application/HostCollectionMutationService.h"
#include "../src/services/application/CollectionDisassemblyPlanService.h"
#include "../src/services/application/dto/RemoteCollectionMutationDtos.h"
#include <QCoreApplication>
#include <QJsonArray>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <cstdio>

namespace {
bool need(bool value,const char* message){if(!value)std::fprintf(stderr,"%s\n",message);return value;}
HostWriteExecutor::MutationOutcome run(const QSqlDatabase&db,const QString&operation,const RemoteCollectionMutationDto::Request&request){RemoteMutationDto::Error error;auto mutation=HostCollectionMutationService::createMutation(operation,RemoteCollectionMutationDto::toMetadata(operation,request),&error);return mutation?mutation(db):HostWriteExecutor::MutationOutcome{};}
RemoteCollectionMutationDto::ExpectedState expected(const QJsonObject&i){return{i["modifiedUtc"].toString(),i["type"].toString(),i["setNumber"].toString(),i["minifigNumber"].toString(),i["state"].toString(),i["condition"].toString(),i["completeness"].toString(),i["nickname"].toString(),i["notes"].toString(),qint64(i["storageId"].toDouble()),qint64(i["sourceBuildId"].toDouble()),i["active"].toBool()};}
}

int main(int argc,char**argv){
 QCoreApplication app(argc,argv);QTemporaryDir temporary;bool ok=true;
 const QString name="remote-collection-"+QUuid::createUuid().toString();
 QSqlDatabase db=QSqlDatabase::addDatabase("QSQLITE",name);db.setDatabaseName(temporary.filePath("db.sqlite"));
 ok&=need(db.open()&&DatabaseSchema::initialize(db),"schema failed");QSqlQuery q(db);
 ok&=need(q.exec("INSERT INTO workspace(name,description,created_utc,modified_utc,is_active) VALUES('W','',CURRENT_TIMESTAMP,CURRENT_TIMESTAMP,1)"),"workspace");const int workspace=q.lastInsertId().toInt();
 ok&=need(q.exec("INSERT INTO set_catalog(set_number,name,year,theme_id,num_parts,image_url,created_utc,modified_utc) VALUES('1000-1','Set',2026,1,1,'',CURRENT_TIMESTAMP,CURRENT_TIMESTAMP)"),"set");
 ok&=need(q.exec("INSERT INTO minifig_catalog(name,num_parts,image_url,is_active,created_utc,modified_utc) VALUES('Fig',1,'',1,CURRENT_TIMESTAMP,CURRENT_TIMESTAMP)"),"fig");const int fig=q.lastInsertId().toInt();
 ok&=need(q.exec(QString("INSERT INTO minifig_external_identifier(minifig_catalog_id,provider,external_id,source,is_active,created_utc,modified_utc) VALUES(%1,'Rebrickable','fig-1','test',1,CURRENT_TIMESTAMP,CURRENT_TIMESTAMP)").arg(fig)),"fig identity");
 int type=0;q.exec("SELECT id FROM storage_location_type LIMIT 1");if(q.next())type=q.value(0).toInt();
 ok&=need(q.exec(QString("INSERT INTO storage_location(workspace_id,parent_location_id,location_type_id,name,description,sort_order,is_active,allows_inventory,allows_collection,created_utc,modified_utc) VALUES(%1,NULL,%2,'Display','',0,1,0,1,CURRENT_TIMESTAMP,CURRENT_TIMESTAMP)").arg(workspace).arg(type)),"location");const int location=q.lastInsertId().toInt();
 ok&=need(q.exec(QString("INSERT INTO storage_location(workspace_id,parent_location_id,location_type_id,name,description,sort_order,is_active,allows_inventory,allows_collection,created_utc,modified_utc) VALUES(%1,NULL,%2,'Inventory','',1,1,1,0,CURRENT_TIMESTAMP,CURRENT_TIMESTAMP)").arg(workspace).arg(type)),"inventory location");const int inventoryLocation=q.lastInsertId().toInt();
 ok&=need(q.exec("INSERT INTO part(part_number,name,rebrickable_part_id,is_active,created_utc,modified_utc,material) VALUES('3001','Brick 2 x 4','3001',1,CURRENT_TIMESTAMP,CURRENT_TIMESTAMP,'Plastic')"),"part");const int partId=q.lastInsertId().toInt();
 ok&=need(q.exec("INSERT INTO color(name,rebrickable_id,created_utc,modified_utc) VALUES('Blue',1,CURRENT_TIMESTAMP,CURRENT_TIMESTAMP)"),"color");const int colorId=q.lastInsertId().toInt();
 ok&=need(q.exec(QString("INSERT INTO set_catalog_part(set_catalog_id,part_id,color_id,quantity_required,is_spare,provider,source,created_utc,modified_utc) SELECT id,%1,%2,2,0,'Rebrickable','test',CURRENT_TIMESTAMP,CURRENT_TIMESTAMP FROM set_catalog WHERE set_number='1000-1'").arg(partId).arg(colorId)),"set composition");
 RemoteCollectionMutationDto::Request add;add.workspaceId=workspace;add.mutationId=RemoteMutationDto::newMutationId();add.sourceType="SetCatalog";add.setNumber="1000-1";add.state="Sealed";add.condition="New";add.completeness="Complete";add.storageId=location;
 db.transaction();auto outcome=run(db,"collection.add",add);ok&=need(outcome.success,"Set Add failed");db.commit();QJsonObject item=outcome.authoritative["item"].toObject();
 RemoteCollectionMutationDto::Request parts;parts.workspaceId=workspace;parts.mutationId=RemoteMutationDto::newMutationId();parts.collectionItemId=item["collectionItemId"].toInt();parts.allowPartsSource=true;parts.expected.modifiedUtc=item["modifiedUtc"].toString();parts.expected.allowPartsSource=false;
 RemoteMutationDto::Error parseError;RemoteCollectionMutationDto::Request decoded;
 ok&=need(RemoteCollectionMutationDto::fromMetadata("collection.partsSource.set",RemoteCollectionMutationDto::toMetadata("collection.partsSource.set",parts),&decoded,&parseError)&&decoded.allowPartsSource,"parts-source DTO round trip failed");
 RemoteCollectionMutationDto::Request disassembly;disassembly.workspaceId=workspace;disassembly.mutationId=RemoteMutationDto::newMutationId();disassembly.collectionItemId=parts.collectionItemId;disassembly.planId="plan-hash";disassembly.expected.modifiedUtc="2026-01-01T00:00:00.000Z";disassembly.expected.type="Set";disassembly.expected.setNumber="1000-1";disassembly.expected.state="Assembled";disassembly.expected.condition="New";disassembly.expected.completeness="Complete";disassembly.expected.active=true;disassembly.returns={{1,location,2},{2,location,1}};
 ok&=need(RemoteCollectionMutationDto::fromMetadata("collection.disassemble",RemoteCollectionMutationDto::toMetadata("collection.disassemble",disassembly),&decoded,&parseError)&&decoded.planId==disassembly.planId&&decoded.returns.size()==2&&decoded.returns.first().storageId==location,"disassembly DTO round trip failed");
 auto duplicateRows=RemoteCollectionMutationDto::toMetadata("collection.disassemble",disassembly);auto duplicateArray=duplicateRows.mutation.value("returns").toArray();duplicateArray[1]=duplicateArray[0];duplicateRows.mutation.insert("returns",duplicateArray);ok&=need(!RemoteCollectionMutationDto::fromMetadata("collection.disassemble",duplicateRows,&decoded,&parseError),"duplicate disassembly row accepted");
 db.transaction();outcome=run(db,"collection.partsSource.set",parts);ok&=need(outcome.success&&outcome.authoritative["item"].toObject()["allowPartsSource"].toBool(),"parts-source update failed");db.commit();
 auto stale=parts;stale.mutationId=RemoteMutationDto::newMutationId();db.transaction();outcome=run(db,"collection.partsSource.set",stale);db.rollback();ok&=need(!outcome.success&&outcome.error.code=="STALE_VERSION","stale accepted");
 ok&=need(q.exec(QString("SELECT allow_parts_source FROM collection_item WHERE id=%1").arg(parts.collectionItemId))&&q.next()&&q.value(0).toBool(),"parts-source persistence failed");
 std::optional<CollectionItem> editable;ok&=need(CollectionRepository(db).tryGetById(parts.collectionItemId,editable)&&editable.has_value(),"Collection edit setup failed");
 RemoteCollectionMutationDto::Request assemble;assemble.workspaceId=workspace;assemble.mutationId=RemoteMutationDto::newMutationId();assemble.collectionItemId=parts.collectionItemId;assemble.state="Assembled";assemble.condition="New";assemble.completeness="Complete";assemble.storageId=location;assemble.expected.modifiedUtc=editable->modifiedUtc.toUTC().toString(Qt::ISODateWithMs);assemble.expected.type="Set";assemble.expected.setNumber="1000-1";assemble.expected.state="Sealed";assemble.expected.condition="New";assemble.expected.completeness="Complete";assemble.expected.storageId=location;assemble.expected.active=true;
 db.transaction();outcome=run(db,"collection.edit",assemble);ok&=need(outcome.success,"remote metadata edit to Assembled failed");db.commit();
 const QJsonObject assembled=outcome.authoritative["item"].toObject();auto bypass=assemble;bypass.mutationId=RemoteMutationDto::newMutationId();bypass.state="Unassembled";bypass.expected=expected(assembled);
 db.transaction();outcome=run(db,"collection.edit",bypass);db.rollback();ok&=need(!outcome.success&&outcome.error.code=="INVALID_ARGUMENT"&&outcome.error.message.contains("Disassemble to Inventory"),"remote generic edit bypass accepted");
 ok&=need(q.exec(QString("SELECT COUNT(*) FROM collection_item WHERE id=%1 AND state='Assembled'").arg(parts.collectionItemId))&&q.next()&&q.value(0).toInt()==1,"rejected remote edit changed Collection state");
 const auto hostPlan=CollectionDisassemblyPlanService(db).preview(workspace,parts.collectionItemId);ok&=need(hostPlan.success&&hostPlan.plan.authority=="Catalog"&&hostPlan.plan.rows.size()==1&&hostPlan.plan.rows.first().quantity==2,"Host catalog disassembly preview failed");
 RemoteCollectionMutationDto::Request executeDisassembly;executeDisassembly.workspaceId=workspace;executeDisassembly.mutationId=RemoteMutationDto::newMutationId();executeDisassembly.collectionItemId=parts.collectionItemId;executeDisassembly.planId=hostPlan.plan.planId;executeDisassembly.expected=expected(assembled);executeDisassembly.returns={{hostPlan.plan.rows.first().rowIndex,inventoryLocation,hostPlan.plan.rows.first().quantity}};
 db.transaction();outcome=run(db,"collection.disassemble",executeDisassembly);ok&=need(outcome.success&&outcome.publicationWorkflow==HostMutationPublicationService::Workflow::CollectionDisassembly,"remote catalog disassembly failed");db.commit();
 ok&=need(q.exec(QString("SELECT quantity FROM inventory_record WHERE workspace_id=%1 AND part_id=%2 AND color_id=%3 AND storage_location_id=%4").arg(workspace).arg(partId).arg(colorId).arg(inventoryLocation))&&q.next()&&q.value(0).toInt()==2,"remote disassembly Inventory result missing");
 ok&=need(q.exec(QString("SELECT COUNT(*) FROM inventory_movement WHERE movement_type='CollectionDisassembly' AND reference_id='%1'").arg(parts.collectionItemId))&&q.next()&&q.value(0).toInt()==1,"remote disassembly history missing");
 const int inventoryAfterDisassembly=q.exec("SELECT SUM(quantity) FROM inventory_record")&&q.next()?q.value(0).toInt():-1;const int movementsAfterDisassembly=q.exec("SELECT COUNT(*) FROM inventory_movement")&&q.next()?q.value(0).toInt():-1;
 db.transaction();outcome=run(db,"collection.disassemble",executeDisassembly);db.rollback();ok&=need(!outcome.success&&outcome.error.code=="STALE_VERSION"&&q.exec("SELECT SUM(quantity) FROM inventory_record")&&q.next()&&q.value(0).toInt()==inventoryAfterDisassembly&&q.exec("SELECT COUNT(*) FROM inventory_movement")&&q.next()&&q.value(0).toInt()==movementsAfterDisassembly,"safe replay guard failed");
 item["modifiedUtc"]=outcome.authoritative["item"].toObject()["modifiedUtc"];
 RemoteCollectionMutationDto::Request malformed=parts;auto metadata=RemoteCollectionMutationDto::toMetadata("collection.partsSource.set",malformed);metadata.mutation.insert("extra",1);ok&=need(!RemoteCollectionMutationDto::fromMetadata("collection.partsSource.set",metadata,&decoded,&parseError),"unknown mutation field accepted");
 RemoteCollectionMutationDto::Request minifig;minifig.workspaceId=workspace;minifig.mutationId=RemoteMutationDto::newMutationId();minifig.sourceType="MinifigCatalog";minifig.minifigNumber="fig-1";minifig.state="Assembled";minifig.condition="Used";minifig.completeness="Complete";
 db.transaction();outcome=run(db,"collection.add",minifig);ok&=need(outcome.success,"Minifig Add failed");db.rollback();
 auto unknown=add;unknown.mutationId=RemoteMutationDto::newMutationId();unknown.setNumber="missing";db.transaction();outcome=run(db,"collection.add",unknown);db.rollback();ok&=need(!outcome.success&&outcome.error.code=="NOT_FOUND","unknown identity accepted");
 std::optional<CollectionItem> current;ok&=need(CollectionRepository(db).tryGetById(parts.collectionItemId,current)&&current.has_value(),"Collection read failed");
 RemoteCollectionMutationDto::Request active;active.workspaceId=workspace;active.mutationId=RemoteMutationDto::newMutationId();active.collectionItemId=parts.collectionItemId;active.desiredActive=false;active.expected.modifiedUtc=current->modifiedUtc.toUTC().toString(Qt::ISODateWithMs);active.expected.type="Set";active.expected.setNumber="1000-1";active.expected.state=collectionItemStateToString(current->state);active.expected.condition=collectionItemConditionToString(current->condition);active.expected.completeness=collectionItemCompletenessToString(current->completeness);active.expected.storageId=current->storageLocationId;active.expected.sourceBuildId=current->sourceBuildId;active.expected.nickname=current->nickname;active.expected.notes=current->notes;active.expected.active=current->isActive;
 db.transaction();outcome=run(db,"collection.setActive",active);ok&=need(outcome.success,"archive failed");db.rollback();ok&=need(q.exec(QString("SELECT is_active FROM collection_item WHERE id=%1").arg(active.collectionItemId))&&q.next()&&q.value(0).toBool(),"lifecycle rollback failed");
 db.close();db={};QSqlDatabase::removeDatabase(name);return ok?0:1;
}
