#include "../src/database/DatabaseSchema.h"
#include "../src/repositories/CollectionRepository.h"
#include "../src/services/application/HostCollectionMutationService.h"
#include "../src/services/application/dto/RemoteCollectionMutationDtos.h"
#include <QCoreApplication>
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
 RemoteCollectionMutationDto::Request add;add.workspaceId=workspace;add.mutationId=RemoteMutationDto::newMutationId();add.sourceType="SetCatalog";add.setNumber="1000-1";add.state="Sealed";add.condition="New";add.completeness="Complete";add.storageId=location;
 db.transaction();auto outcome=run(db,"collection.add",add);ok&=need(outcome.success,"Set Add failed");db.commit();QJsonObject item=outcome.authoritative["item"].toObject();
 RemoteCollectionMutationDto::Request parts;parts.workspaceId=workspace;parts.mutationId=RemoteMutationDto::newMutationId();parts.collectionItemId=item["collectionItemId"].toInt();parts.allowPartsSource=true;parts.expected.modifiedUtc=item["modifiedUtc"].toString();parts.expected.allowPartsSource=false;
 RemoteMutationDto::Error parseError;RemoteCollectionMutationDto::Request decoded;
 ok&=need(RemoteCollectionMutationDto::fromMetadata("collection.partsSource.set",RemoteCollectionMutationDto::toMetadata("collection.partsSource.set",parts),&decoded,&parseError)&&decoded.allowPartsSource,"parts-source DTO round trip failed");
 db.transaction();outcome=run(db,"collection.partsSource.set",parts);ok&=need(outcome.success&&outcome.authoritative["item"].toObject()["allowPartsSource"].toBool(),"parts-source update failed");db.commit();
 auto stale=parts;stale.mutationId=RemoteMutationDto::newMutationId();db.transaction();outcome=run(db,"collection.partsSource.set",stale);db.rollback();ok&=need(!outcome.success&&outcome.error.code=="STALE_VERSION","stale accepted");
 ok&=need(q.exec(QString("SELECT allow_parts_source FROM collection_item WHERE id=%1").arg(parts.collectionItemId))&&q.next()&&q.value(0).toBool(),"parts-source persistence failed");
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
