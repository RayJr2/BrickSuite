#include "../src/database/DatabaseSchema.h"
#include "../src/services/application/HostPartReferenceMutationService.h"
#include "../src/services/application/dto/RemotePartReferenceMutationDtos.h"
#include "../src/services/parts/PartReferenceManifest.h"

#include <QCoreApplication>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <cstdio>

namespace {
bool require(bool value,const char* message){if(!value)std::fprintf(stderr,"%s\n",message);return value;}
HostWriteExecutor::MutationOutcome execute(const QSqlDatabase& db,const QString& operation,
    const RemotePartReferenceMutationDto::Request& request)
{
    RemoteMutationDto::Error error;
    auto mutation=HostPartReferenceMutationService::createMutation(
        operation,RemotePartReferenceMutationDto::toMetadata(operation,request),&error);
    return mutation?mutation(db):HostWriteExecutor::MutationOutcome{};
}
}

int main(int argc,char**argv)
{
    QCoreApplication app(argc,argv);QTemporaryDir temporary;bool ok=true;
    const QString name="part-reference-remote-"+QUuid::createUuid().toString();
    QSqlDatabase db=QSqlDatabase::addDatabase("QSQLITE",name);db.setDatabaseName(temporary.filePath("test.db"));
    ok&=require(db.open()&&DatabaseSchema::initialize(db),"schema initialization failed");
    QSqlQuery query(db);ok&=require(query.exec("INSERT INTO workspace(name,description,created_utc,modified_utc,is_active) VALUES('Test','',CURRENT_TIMESTAMP,CURRENT_TIMESTAMP,1)"),"workspace seed failed");
    const int workspace=query.lastInsertId().toInt();
    ok&=require(query.exec("INSERT INTO part(part_number,name,is_active,created_utc,modified_utc,material) VALUES('remote-custom-part','Remote Custom Part',1,CURRENT_TIMESTAMP,CURRENT_TIMESTAMP,'Plastic')"),"Part seed failed");
    PartReferenceManifest manifest;QString manifestError;ok&=require(manifest.load(&manifestError)&&!manifest.entries().isEmpty(),"manifest load failed");
    const auto destination=manifest.entries().first();
    RemotePartReferenceMutationDto::Request add;add.workspaceId=workspace;add.mutationId=RemoteMutationDto::newMutationId();add.partNumber="remote-custom-part";add.catalog=destination.catalog;add.section=destination.section;add.placement=PartReferencePlacement::Append;
    db.transaction();auto outcome=execute(db,"partReference.customizations.add",add);ok&=require(outcome.success,"Append failed");const int id=outcome.authoritative.value("customizationId").toInt();db.rollback();
    ok&=require(id>0&&query.exec("SELECT COUNT(*) FROM user_part_reference_entry")&&query.next()&&query.value(0).toInt()==0,"Add did not roll back");
    db.transaction();outcome=execute(db,"partReference.customizations.add",add);ok&=require(outcome.success,"committed Add failed");db.commit();
    RemotePartReferenceMutationDto::Result decoded;RemoteMutationDto::Result wire;wire.mutationId=add.mutationId;wire.operation="partReference.customizations.add";wire.authoritative=outcome.authoritative;RemoteMutationDto::Error decodeError;
    ok&=require(RemotePartReferenceMutationDto::resultFromMutation(wire,&decoded,&decodeError)&&decoded.customizationId>0,"authoritative Add result invalid");
    auto duplicate=add;duplicate.mutationId=RemoteMutationDto::newMutationId();db.transaction();auto duplicateOutcome=execute(db,"partReference.customizations.add",duplicate);db.rollback();ok&=require(!duplicateOutcome.success&&duplicateOutcome.error.code=="CONFLICT","duplicate was not rejected");
    RemotePartReferenceMutationDto::Request remove;remove.workspaceId=workspace;remove.mutationId=RemoteMutationDto::newMutationId();remove.customizationId=decoded.customizationId;remove.expected.modifiedUtc=decoded.modifiedUtc;remove.expected.partNumber=decoded.partNumber;remove.expected.catalog=decoded.catalog;remove.expected.section=decoded.section;remove.expected.placement=decoded.placement;remove.expected.anchorPartNumber=decoded.anchorPartNumber;
    auto stale=remove;stale.expected.modifiedUtc="2000-01-01T00:00:00.000Z";db.transaction();auto staleOutcome=execute(db,"partReference.customizations.remove",stale);db.rollback();ok&=require(!staleOutcome.success&&staleOutcome.error.code=="STALE_VERSION","stale Remove was not rejected");
    db.transaction();auto removeOutcome=execute(db,"partReference.customizations.remove",remove);ok&=require(removeOutcome.success,"Remove failed");db.rollback();
    ok&=require(query.exec("SELECT COUNT(*) FROM user_part_reference_entry")&&query.next()&&query.value(0).toInt()==1,"Remove did not roll back");
    db.transaction();removeOutcome=execute(db,"partReference.customizations.remove",remove);ok&=require(removeOutcome.success,"committed Remove failed");db.commit();
    ok&=require(query.exec("SELECT COUNT(*) FROM user_part_reference_entry")&&query.next()&&query.value(0).toInt()==0,"Remove did not commit");
    db.close();db={};QSqlDatabase::removeDatabase(name);return ok?0:1;
}
