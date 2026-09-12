#include "HostBuildMutationService.h"

#include "dto/RemoteBuildMutationDtos.h"
#include "../builds/BuildLifecycleService.h"
#include "../builds/BuildMutationService.h"
#include "../builds/BuildRequirementMutationService.h"
#include "../builds/BuildAllocationMutationService.h"
#include "../../repositories/BuildAllocationRepository.h"
#include "../../repositories/BuildRepository.h"
#include "../../repositories/BuildRequirementRepository.h"
#include "../../repositories/ManufacturerRepository.h"
#include "../../repositories/SetCatalogRepository.h"
#include "../../repositories/PartRepository.h"
#include "../../repositories/ColorRepository.h"
#include "../../repositories/InventoryRecordRepository.h"
#include "../../repositories/StorageLocationRepository.h"

#include <QJsonArray>
#include <QHash>
#include <algorithm>

namespace {
HostWriteExecutor::MutationOutcome failure(const QString& code, const QString& message,
                                           bool refresh = false)
{
    HostWriteExecutor::MutationOutcome value;
    value.error={code,message,false};
    if(refresh)value.error.conflict={{"refreshRequired",true}};
    return value;
}

QString manufacturerDisplay(const QSqlDatabase& db, const Build& build)
{
    if(build.inventoryMode()!=QStringLiteral("CompleteSet"))return {};
    const auto manufacturer=ManufacturerRepository(db).getById(build.manufacturerId());
    return manufacturer?manufacturer->name():QString();
}

QString reference(const Build& build)
{ return build.sourceReference().isEmpty()?build.setNumber():build.sourceReference(); }

QJsonObject buildJson(const QSqlDatabase& db,const Build& build)
{
    return {{"buildId",build.id()},{"workspaceId",build.workspaceId()},
        {"buildType",build.buildType()},{"reference",reference(build)},
        {"inventoryMode",build.inventoryMode()},{"manufacturer",manufacturerDisplay(db,build)},
        {"name",build.name()},{"status",build.status()},{"notes",build.notes()},
        {"active",build.isActive()},{"catalogLinked",build.setCatalogId()>0||build.minifigCatalogId()>0},
        {"createdUtc",build.createdUtc().toUTC().toString(Qt::ISODateWithMs)},
        {"modifiedUtc",build.modifiedUtc().toUTC().toString(Qt::ISODateWithMs)}};
}

bool matches(const QSqlDatabase& db,const Build& build,
             const RemoteBuildMutationDto::ExpectedState& expected)
{
    return build.modifiedUtc().toUTC().toString(Qt::ISODateWithMs)==expected.modifiedUtc
        && build.buildType()==expected.buildType&&reference(build)==expected.reference
        && build.inventoryMode()==expected.inventoryMode
        && manufacturerDisplay(db,build)==expected.manufacturer&&build.name()==expected.name
        && build.status()==expected.status&&build.notes()==expected.notes
        && build.isActive()==expected.active;
}

QJsonObject requirementJson(const QSqlDatabase&db,const BuildRequirement&r)
{
    const auto part=PartRepository(db).getById(r.partId());const auto color=ColorRepository(db).getById(r.colorId());
    const auto subPart=PartRepository(db).getById(r.substitutePartId());const auto subColor=ColorRepository(db).getById(r.substituteColorId());
    return{{"requirementId",r.id()},{"buildId",r.buildId()},{"partNumber",part?part->partNumber():QString()},{"rebrickableColorId",color?color->rebrickableId():0},{"substitutePartNumber",subPart?subPart->partNumber():QString()},{"substituteRebrickableColorId",subColor?subColor->rebrickableId():-1},{"effectivePartNumber",subPart?subPart->partNumber():(part?part->partNumber():QString())},{"effectiveRebrickableColorId",subColor?subColor->rebrickableId():(color?color->rebrickableId():0)},{"quantityRequired",r.quantityRequired()},{"quantityPulled",r.quantityPulled()},{"quantityReleased",r.quantityReleased()},{"spare",r.isSpare()},{"modifiedUtc",r.modifiedUtc().toUTC().toString(Qt::ISODateWithMs)}};
}
QJsonObject allocationJson(const BuildAllocation&a){return{{"allocationId",a.id()},{"buildId",a.buildId()},{"requirementId",a.buildRequirementId()},{"inventoryRecordId",a.inventoryRecordId()},{"quantity",a.quantityAllocated()},{"modifiedUtc",a.modifiedUtc().toUTC().toString(Qt::ISODateWithMs)}};}
bool requirementMatches(const QSqlDatabase&db,const BuildRequirement&r,const RemoteBuildMutationDto::RequirementExpectedState&e)
{
    const auto j=requirementJson(db,r);return r.id()==e.requirementId&&r.buildId()==e.buildId&&j["modifiedUtc"].toString()==e.modifiedUtc&&j["partNumber"].toString()==e.partNumber&&j["rebrickableColorId"].toInt()==e.rebrickableColorId&&j["substitutePartNumber"].toString()==e.substitutePartNumber&&j["substituteRebrickableColorId"].toInt()==e.substituteRebrickableColorId&&r.quantityRequired()==e.quantityRequired&&r.quantityPulled()==e.quantityPulled&&r.quantityReleased()==e.quantityReleased&&r.isSpare()==e.spare;
}
bool resolveIdentity(const QSqlDatabase&db,const QString&partNumber,int colorId,int*part,int*color)
{const auto p=PartRepository(db).getByPartNumber(partNumber.trimmed());const auto c=ColorRepository(db).getByRebrickableId(colorId);if(!p||!c)return false;*part=p->id();*color=c->id();return true;}
HostWriteExecutor::MutationOutcome requirementResult(const QSqlDatabase&db,const BuildRequirementMutationService::Result&r,qint64 workspaceId)
{if(!r.success){const QString code=r.error==BuildRequirementMutationService::Error::NotFound?"NOT_FOUND":r.error==BuildRequirementMutationService::Error::DatabaseFailure?"INTERNAL_ERROR":r.error==BuildRequirementMutationService::Error::InvalidState?"CONFLICT":"INVALID_ARGUMENT";return failure(code,r.message,code=="CONFLICT");}HostWriteExecutor::MutationOutcome out;out.success=true;out.authoritative={{"requirement",requirementJson(db,r.requirement)}};out.publicationWorkflow=HostMutationPublicationService::Workflow::BuildRequirements;out.publicationScope.workspaceId=workspaceId;out.publicationScope.buildId=r.requirement.buildId();return out;}
HostWriteExecutor::MutationOutcome allocationResult(const QSqlDatabase&db,const BuildAllocationMutationService::Result&r,const Build&build)
{if(!r.success){const QString code=r.error==BuildAllocationMutationService::Error::NotFound?"NOT_FOUND":r.error==BuildAllocationMutationService::Error::DatabaseFailure?"INTERNAL_ERROR":r.error==BuildAllocationMutationService::Error::InvalidState?"CONFLICT":"INVALID_ARGUMENT";return failure(code,r.message,code=="CONFLICT");}QJsonArray rows;for(const auto&a:r.allocations)rows.append(allocationJson(a));HostWriteExecutor::MutationOutcome out;out.success=true;out.authoritative={{"build",buildJson(db,build)},{"allocations",rows},{"effects",QJsonObject{{"piecesAdded",r.piecesAdded},{"allocationsCreated",r.allocationsCreated},{"allocationsUpdated",r.allocationsUpdated}}}};out.publicationWorkflow=HostMutationPublicationService::Workflow::BuildRequirements;out.publicationScope.workspaceId=build.workspaceId();out.publicationScope.buildId=build.id();return out;}

int manufacturerId(const QSqlDatabase& db,const QString& name,bool required,QString* error)
{
    ManufacturerRepository repository(db);
    if(!required)return repository.legoManufacturerId();
    const auto manufacturer=repository.getByName(name.trimmed());
    if(!manufacturer||!manufacturer->isActive()){
        if(error)*error=QStringLiteral("The selected Host manufacturer is unavailable.");
        return 0;
    }
    return manufacturer->id();
}

HostWriteExecutor::MutationOutcome mapMutation(const QSqlDatabase& db,
                                                const BuildMutationService::Result& result)
{
    if(!result.success){QString code=result.error==BuildMutationService::Error::NotFound?"NOT_FOUND":result.error==BuildMutationService::Error::DatabaseFailure?"INTERNAL_ERROR":"INVALID_ARGUMENT";return failure(code,result.message);}
    HostWriteExecutor::MutationOutcome out;out.success=true;out.authoritative={{"build",buildJson(db,result.build)}};out.publicationWorkflow=HostMutationPublicationService::Workflow::BuildMetadata;out.publicationScope.workspaceId=result.build.workspaceId();out.publicationScope.buildId=result.build.id();return out;
}

bool validInventoryDestination(const QSqlDatabase& db, int workspaceId, int storageId)
{
    StorageLocationRepository storage(db);
    const auto destination=storage.getById(storageId);
    const auto children=storage.hasActiveChildrenChecked(storageId);
    return destination&&destination->workspaceId()==workspaceId&&destination->isActive()
        &&destination->allowsInventory()&&children==StorageLocationRepository::CheckResult::No;
}

HostWriteExecutor::MutationOutcome lifecycleResult(
    const QSqlDatabase& db,const BuildLifecycleService::Result& result,
    HostMutationPublicationService::Workflow workflow)
{
    if(!result.success){const QString code=result.error==BuildLifecycleService::Error::NotFound?"NOT_FOUND":result.error==BuildLifecycleService::Error::DatabaseFailure?"INTERNAL_ERROR":"CONFLICT";return failure(code,result.message,code==QStringLiteral("CONFLICT"));}
    HostWriteExecutor::MutationOutcome out;out.success=true;
    QJsonArray requirementIds,inventoryIds,allocationIds;
    for(int value:result.affectedRequirementIds)requirementIds.append(value);
    for(int value:result.affectedInventoryIds)inventoryIds.append(value);
    for(int value:result.affectedAllocationIds)allocationIds.append(value);
    out.authoritative={{"build",buildJson(db,result.build)},{"effects",QJsonObject{{"requirementIds",requirementIds},{"inventoryIds",inventoryIds},{"allocationIds",allocationIds},{"returnedPieces",result.returnedPieces},{"collectionChanged",result.collectionChanged}}}};
    out.publicationWorkflow=workflow;out.publicationScope.workspaceId=result.build.workspaceId();
    out.publicationScope.buildId=result.build.id();out.publicationScope.inventoryChanged=result.returnedPieces>0;
    out.publicationScope.collectionChanged=result.collectionChanged;return out;
}
}

HostWriteExecutor::Mutation HostBuildMutationService::createMutation(
    const QString& operation,const RemoteMutationDto::Metadata& metadata,
    RemoteMutationDto::Error* error)
{
    RemoteBuildMutationDto::Request request;
    if(!RemoteBuildMutationDto::fromMetadata(operation,metadata,&request,error))return {};
    return [operation,request](const QSqlDatabase& db) {
        BuildMutationService mutations(db);
        const bool requirementOperation=operation.startsWith(QStringLiteral("builds.requirements."));
        if(requirementOperation||operation==QStringLiteral("builds.allocations.set")){
            std::optional<BuildRequirement> requirement;
            if(operation!=QStringLiteral("builds.requirements.add")){
                if(!BuildRequirementRepository(db).tryGetById(int(request.requirementId),requirement))return failure("INTERNAL_ERROR","The Host could not load the requirement.");
                if(!requirement)return failure("NOT_FOUND","The requirement was not found.");
                const auto build=BuildRepository(db).getById(requirement->buildId());if(!build||build->workspaceId()!=request.workspaceId||request.expectedRequirement.buildId!=build->id())return failure("NOT_FOUND","The requirement was not found in this Workspace.");
                if(!requirementMatches(db,*requirement,request.expectedRequirement))return failure("STALE_VERSION","The requirement changed on the Host. Refresh and try again.",true);
            }
            if(operation==QStringLiteral("builds.requirements.add")){
                const auto build=BuildRepository(db).getById(int(request.buildId));if(!build||build->workspaceId()!=request.workspaceId)return failure("NOT_FOUND","The Build was not found in this Workspace.");
                int part=0,color=0,subPart=0,subColor=0;if(!resolveIdentity(db,request.partNumber,request.rebrickableColorId,&part,&color))return failure("NOT_FOUND","The original Part or Color is unavailable on the Host.");if(!request.substitutePartNumber.isEmpty()){const auto p=PartRepository(db).getByPartNumber(request.substitutePartNumber.trimmed());if(!p)return failure("NOT_FOUND","The substitute Part is unavailable on the Host.");subPart=p->id();}if(request.substituteRebrickableColorId>=0){const auto c=ColorRepository(db).getByRebrickableId(request.substituteRebrickableColorId);if(!c)return failure("NOT_FOUND","The substitute Color is unavailable on the Host.");subColor=c->id();}BuildRequirement r;r.setBuildId(build->id());r.setPartId(part);r.setColorId(color);r.setSubstitutePartId(subPart);r.setSubstituteColorId(subColor);r.setQuantityRequired(request.quantityRequired);r.setIsSpare(request.spare);return requirementResult(db,BuildRequirementMutationService(db).addInCurrentTransaction(r),request.workspaceId);
            }
            if(operation==QStringLiteral("builds.requirements.remove"))return requirementResult(db,BuildRequirementMutationService(db).removeInCurrentTransaction(requirement->id()),request.workspaceId);
            if(operation==QStringLiteral("builds.requirements.edit")){
                int subPart=0,subColor=0;if(!request.substitutePartNumber.isEmpty()){const auto p=PartRepository(db).getByPartNumber(request.substitutePartNumber.trimmed());if(!p)return failure("NOT_FOUND","The substitute Part is unavailable on the Host.");subPart=p->id();}if(request.substituteRebrickableColorId>=0){const auto c=ColorRepository(db).getByRebrickableId(request.substituteRebrickableColorId);if(!c)return failure("NOT_FOUND","The substitute Color is unavailable on the Host.");subColor=c->id();}if(subPart==requirement->partId())subPart=0;if(subColor==requirement->colorId())subColor=0;return requirementResult(db,BuildRequirementMutationService(db).editInCurrentTransaction(requirement->id(),subPart,subColor,request.quantityRequired,request.spare),request.workspaceId);
            }
            BuildAllocationRepository allocationRepository(db);const auto current=allocationRepository.getByRequirement(requirement->id());
            if(current.size()!=std::count_if(request.allocations.cbegin(),request.allocations.cend(),[](const auto&r){return r.allocationId>0;}))return failure("STALE_VERSION","Allocations changed on the Host. Refresh and try again.",true);
            QList<BuildAllocation> desired;
            for(const auto&row:request.allocations){
                const auto inventory=InventoryRecordRepository(db).getById(int(row.inventoryRecordId));if(!inventory||inventory->workspaceId()!=request.workspaceId)return failure("NOT_FOUND","An Inventory record was not found in this Workspace.");
                if(inventory->quantity()!=row.inventoryQuantity||(!row.inventoryModifiedUtc.isEmpty()&&inventory->modifiedUtc().toUTC().toString(Qt::ISODateWithMs)!=row.inventoryModifiedUtc))return failure("STALE_VERSION","Inventory changed on the Host. Refresh and try again.",true);
                const auto old=std::find_if(current.cbegin(),current.cend(),[&](const auto&a){return a.id()==row.allocationId&&a.inventoryRecordId()==row.inventoryRecordId;});
                if((row.allocationId>0&&(old==current.cend()||old->quantityAllocated()!=row.expectedQuantity||(!row.modifiedUtc.isEmpty()&&old->modifiedUtc().toUTC().toString(Qt::ISODateWithMs)!=row.modifiedUtc)))||(row.allocationId==0&&std::any_of(current.cbegin(),current.cend(),[&](const auto&a){return a.inventoryRecordId()==row.inventoryRecordId;})))return failure("STALE_VERSION","Allocations changed on the Host. Refresh and try again.",true);
                if(row.quantity>0){BuildAllocation a;a.setInventoryRecordId(int(row.inventoryRecordId));a.setQuantityAllocated(row.quantity);desired.append(a);}
            }
            const auto build=BuildRepository(db).getById(requirement->buildId());return allocationResult(db,BuildAllocationMutationService(db).replaceForRequirementInCurrentTransaction(requirement->id(),desired),*build);
        }
        if(operation==QStringLiteral("builds.allocateAvailable")){
            const auto build=BuildRepository(db).getById(int(request.buildId));if(!build||build->workspaceId()!=request.workspaceId)return failure("NOT_FOUND","The Build was not found in this Workspace.");if(!matches(db,*build,request.expected))return failure("STALE_VERSION","The Build changed on the Host. Refresh and try again.",true);return allocationResult(db,BuildAllocationMutationService(db).allocateAvailableInCurrentTransaction(build->id(),int(request.preferredStorageId)),*build);
        }
        if(operation==QStringLiteral("builds.add")){
            if((request.buildType!="Set"&&request.buildType!="MOC")
                ||(request.inventoryMode!="Stock"&&request.inventoryMode!="CompleteSet")
                ||request.initialStatus!="Planned")
                return failure("INVALID_ARGUMENT","Remote Builds must be Set or MOC Builds created in Planned status.");
            if(request.reference.trimmed().isEmpty())return failure("INVALID_ARGUMENT","Enter a Set or MOC reference.");
            if(request.buildType=="MOC"&&request.inventoryMode!="Stock")return failure("INVALID_ARGUMENT","MOC Builds must use Build from Stock.");
            QString manufacturerError;const int manufacturer=manufacturerId(db,request.manufacturer,request.inventoryMode=="CompleteSet",&manufacturerError);
            if(manufacturer<=0)return failure("NOT_FOUND",manufacturerError);
            Build build;build.setWorkspaceId(int(request.workspaceId));build.setBuildType(request.buildType);build.setSetNumber(request.reference.trimmed());build.setSourceReference(request.reference.trimmed());build.setInventoryMode(request.inventoryMode);build.setManufacturerId(manufacturer);build.setStatus("Planned");build.setName(request.name);build.setNotes(request.notes);build.setIsActive(true);
            if(request.buildType=="Set"){
                bool ok=false;const auto matches=SetCatalogRepository(db).getExactMatchesBySetNumber(request.reference.trimmed(),&ok);
                if(!ok)return failure("INTERNAL_ERROR","The Host could not resolve the Set reference.");
                if(matches.size()>1)return failure("CONFLICT","The Set reference is ambiguous on the Host.",true);
                if(matches.size()==1)build.setSetCatalogId(matches.first().id());
            }
            return mapMutation(db,mutations.createInCurrentTransaction(build));
        }
        std::optional<Build> build;
        if(!BuildRepository(db).tryGetById(int(request.buildId),build))return failure("INTERNAL_ERROR","The Host could not load the Build.");
        if(!build||build->workspaceId()!=request.workspaceId)return failure("NOT_FOUND","The Build was not found in this Workspace.");
        if(operation!=QStringLiteral("builds.spare.store")&&!matches(db,*build,request.expected))return failure("STALE_VERSION","The Build changed on the Host. Refresh and try again.",true);
        if(operation==QStringLiteral("builds.spare.store")){
            if(!build->isActive())return failure("CONFLICT","Only an active Complete Set can store a spare.",true);
            const auto requirement=BuildRequirementRepository(db).getById(int(request.requirementId));
            if(!requirement||!requirementMatches(db,*requirement,request.expectedRequirement))return failure("STALE_VERSION","The spare requirement changed on the Host. Refresh and try again.",true);
            if(!validInventoryDestination(db,build->workspaceId(),int(request.preferredStorageId)))return failure("CONFLICT","The selected Host Storage destination is no longer available.",true);
            auto out=lifecycleResult(db,BuildLifecycleService(db).storeCompleteSetSpareInCurrentTransaction(
                build->id(),requirement->id(),int(request.preferredStorageId),request.quantity),
                HostMutationPublicationService::Workflow::CompleteSetSpare);
            if(out.success){const auto updated=BuildRequirementRepository(db).getById(requirement->id());if(updated)out.authoritative["requirement"]=requirementJson(db,*updated);}
            return out;
        }
        if(operation==QStringLiteral("builds.edit")){
            QString manufacturerError;const int manufacturer=manufacturerId(db,request.manufacturer,build->inventoryMode()=="CompleteSet",&manufacturerError);
            if(manufacturer<=0)return failure("NOT_FOUND",manufacturerError);
            return mapMutation(db,mutations.updateMetadataInCurrentTransaction(build->id(),request.name,manufacturer,request.notes));
        }
        if(operation==QStringLiteral("builds.setActive"))
            return mapMutation(db,mutations.setActiveInCurrentTransaction(build->id(),request.desiredActive));
        if(operation==QStringLiteral("builds.complete")){
            if(!build->isActive()||(build->status()!="Planned"&&build->status()!="Pulling"))return failure("INVALID_ARGUMENT","Only an active Planned or Pulling Build can be completed.");
            auto out=mapMutation(db,mutations.completeInCurrentTransaction(build->id()));
            if(out.success)out.publicationWorkflow=HostMutationPublicationService::Workflow::BuildRequirements;
            return out;
        }
        if((operation==QStringLiteral("builds.cancel")&&(build->status()!=QStringLiteral("Planned")&&build->status()!=QStringLiteral("Pulling")))
            ||(operation==QStringLiteral("builds.disassemble")&&(!build->isActive()||build->status()!=QStringLiteral("Complete")||(build->inventoryMode()!=QStringLiteral("Stock")&&build->inventoryMode()!=QStringLiteral("CompleteSet")))))
            return failure("CONFLICT","The Build is no longer eligible for this lifecycle operation.",true);
        QList<BuildLifecycleService::DisassemblyReturn> returns;
        for(const auto& row:request.returns){
            std::optional<BuildRequirement> requirement;
            if(!BuildRequirementRepository(db).tryGetById(int(row.requirementId),requirement))return failure("INTERNAL_ERROR","The Host could not validate a return requirement.");
            if(!requirement||requirement->buildId()!=build->id())return failure("INVALID_ARGUMENT","A cancellation return requirement is invalid.");
            if(!validInventoryDestination(db,build->workspaceId(),int(row.storageId)))return failure("CONFLICT","A selected Host Storage destination is no longer available.",true);
            QString manufacturerError;const int manufacturer=manufacturerId(db,row.manufacturer,true,&manufacturerError);
            if(manufacturer<=0)return failure("NOT_FOUND",manufacturerError);
            const int returnedPartId=build->inventoryMode()==QStringLiteral("Stock")
                ?requirement->effectivePartId():requirement->partId();
            const int returnedColorId=build->inventoryMode()==QStringLiteral("Stock")
                ?requirement->effectiveColorId():requirement->colorId();
            returns.append({int(row.requirementId),returnedPartId,returnedColorId,manufacturer,int(row.storageId),row.quantity,row.spare});
        }
        const auto state=collectionItemStateFromString(request.linkedCollectionState);
        if(state==CollectionItemState::Invalid)return failure("INVALID_ARGUMENT","The linked Collection state is invalid.");
        if(operation==QStringLiteral("builds.disassemble")){
            QHash<QString,int> expectedRows,submittedRows;
            const auto plan=BuildLifecycleService(db).disassemblyReturnPlan(build->id());
            if(!plan.success)return failure("STALE_VERSION",plan.message+QStringLiteral(" Refresh and try again."),true);
            for(const auto& row:plan.rows)expectedRows[QString("%1|%2|%3").arg(row.requirementId).arg(row.manufacturerId).arg(row.spare)]+=row.quantity;
            for(const auto& row:returns)submittedRows[QString("%1|%2|%3").arg(row.requirementId).arg(row.manufacturerId).arg(row.spare)]+=row.quantity;
            if(expectedRows!=submittedRows)return failure("STALE_VERSION","The disassembly return plan changed on the Host. Refresh and try again.",true);
            return lifecycleResult(db,BuildLifecycleService(db).disassembleInCurrentTransaction(build->id(),returns,state),HostMutationPublicationService::Workflow::BuildDisassembly);
        }
        const auto cancelled=BuildLifecycleService(db).cancelInCurrentTransaction(build->id(),returns,state);
        return lifecycleResult(db,cancelled,HostMutationPublicationService::Workflow::BuildCancellation);
    };
}
