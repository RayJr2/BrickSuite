#include "HostBuildMutationService.h"

#include "dto/RemoteBuildMutationDtos.h"
#include "../builds/BuildLifecycleService.h"
#include "../builds/BuildMutationService.h"
#include "../../repositories/BuildRepository.h"
#include "../../repositories/BuildRequirementRepository.h"
#include "../../repositories/ManufacturerRepository.h"
#include "../../repositories/SetCatalogRepository.h"

#include <QJsonArray>

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
}

HostWriteExecutor::Mutation HostBuildMutationService::createMutation(
    const QString& operation,const RemoteMutationDto::Metadata& metadata,
    RemoteMutationDto::Error* error)
{
    RemoteBuildMutationDto::Request request;
    if(!RemoteBuildMutationDto::fromMetadata(operation,metadata,&request,error))return {};
    return [operation,request](const QSqlDatabase& db) {
        BuildMutationService mutations(db);
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
        if(!matches(db,*build,request.expected))return failure("STALE_VERSION","The Build changed on the Host. Refresh and try again.",true);
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
        QList<BuildLifecycleService::DisassemblyReturn> returns;
        for(const auto& row:request.returns){
            std::optional<BuildRequirement> requirement;
            if(!BuildRequirementRepository(db).tryGetById(int(row.requirementId),requirement))return failure("INTERNAL_ERROR","The Host could not validate a return requirement.");
            if(!requirement||requirement->buildId()!=build->id())return failure("INVALID_ARGUMENT","A cancellation return requirement is invalid.");
            QString manufacturerError;const int manufacturer=manufacturerId(db,row.manufacturer,true,&manufacturerError);
            if(manufacturer<=0)return failure("NOT_FOUND",manufacturerError);
            returns.append({int(row.requirementId),requirement->partId(),requirement->colorId(),manufacturer,int(row.storageId),row.quantity,row.spare});
        }
        const auto state=collectionItemStateFromString(request.linkedCollectionState);
        if(state==CollectionItemState::Invalid)return failure("INVALID_ARGUMENT","The linked Collection state is invalid.");
        const auto cancelled=BuildLifecycleService(db).cancelInCurrentTransaction(build->id(),returns,state);
        if(!cancelled.success){QString code=cancelled.error==BuildLifecycleService::Error::NotFound?"NOT_FOUND":cancelled.error==BuildLifecycleService::Error::DatabaseFailure?"INTERNAL_ERROR":"CONFLICT";return failure(code,cancelled.message,code==QStringLiteral("CONFLICT"));}
        HostWriteExecutor::MutationOutcome out;out.success=true;
        QJsonArray requirementIds,inventoryIds,allocationIds;for(int id:cancelled.affectedRequirementIds)requirementIds.append(id);for(int id:cancelled.affectedInventoryIds)inventoryIds.append(id);for(int id:cancelled.affectedAllocationIds)allocationIds.append(id);
        out.authoritative={{"build",buildJson(db,cancelled.build)},{"effects",QJsonObject{{"requirementIds",requirementIds},{"inventoryIds",inventoryIds},{"allocationIds",allocationIds},{"returnedPieces",cancelled.returnedPieces},{"collectionChanged",cancelled.collectionChanged}}}};
        out.publicationWorkflow=HostMutationPublicationService::Workflow::BuildCancellation;
        out.publicationScope.workspaceId=cancelled.build.workspaceId();
        out.publicationScope.buildId=cancelled.build.id();
        out.publicationScope.inventoryChanged=cancelled.returnedPieces>0;
        out.publicationScope.collectionChanged=cancelled.collectionChanged;
        return out;
    };
}
