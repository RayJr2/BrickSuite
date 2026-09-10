#include "HostStorageProtocolMutationService.h"
#include "HostStorageMutationService.h"
#include "dto/RemoteStorageMutationDtos.h"
#include "dto/RemoteReadJson.h"
#include "../../repositories/StorageLocationTypeRepository.h"

namespace {
QString code(HostStorageMutationService::ErrorCode value)
{
    using E=HostStorageMutationService::ErrorCode;
    switch(value){case E::InvalidArgument:return "INVALID_ARGUMENT";case E::WorkspaceMissing:case E::StorageMissing:case E::ParentMissing:case E::TypeMissing:return "NOT_FOUND";case E::WorkspaceInactive:case E::ParentInactive:case E::ParentWrongWorkspace:case E::TypeInactive:case E::SelfParent:case E::DescendantCycle:case E::InventoryOccupied:case E::CollectionOccupied:case E::ActiveChildren:return "CONFLICT";case E::StaleExpectedState:return "STALE_VERSION";case E::DatabaseFailure:return "INTERNAL_ERROR";default:return "INTERNAL_ERROR";}
}
HostWriteExecutor::MutationOutcome fail(const HostStorageMutationService::Error&e){HostWriteExecutor::MutationOutcome o;o.error={code(e.code),e.message,false};if(o.error.code=="CONFLICT"||o.error.code=="STALE_VERSION")o.error.conflict={{"refreshRequired",true}};return o;}
HostStorageMutationService::ExpectedState expected(const RemoteStorageMutationDto::ExpectedState&v){HostStorageMutationService::ExpectedState e;e.provided=true;e.modifiedUtc=QDateTime::fromString(v.modifiedUtc,Qt::ISODateWithMs).toUTC();e.parentStorageId=int(v.parentStorageId);e.storageTypeId=int(v.storageTypeId);e.name=v.name;e.description=v.description;e.sortOrder=v.sortOrder;e.active=v.active;e.allowsInventory=v.allowsInventory;e.allowsCollection=v.allowsCollection;return e;}
}
HostWriteExecutor::Mutation HostStorageProtocolMutationService::createMutation(const QString&operation,const RemoteMutationDto::Metadata&metadata,RemoteMutationDto::Error*error)
{
    RemoteStorageMutationDto::Request request;if(!RemoteStorageMutationDto::fromMetadata(operation,metadata,&request,error))return {};
    return [operation,request](const QSqlDatabase&db){HostStorageMutationService service(db);HostStorageMutationService::Result result;
        if(operation=="storage.add")result=service.add({int(request.workspaceId),int(request.parentStorageId),int(request.storageTypeId),request.name,request.description,request.allowsInventory,request.allowsCollection});
        else if(operation=="storage.edit"){HostStorageMutationService::EditRequest r;r.workspaceId=int(request.workspaceId);r.storageId=int(request.storageId);r.parentStorageId=int(request.parentStorageId);r.storageTypeId=int(request.storageTypeId);r.name=request.name;r.description=request.description;r.allowsInventory=request.allowsInventory;r.allowsCollection=request.allowsCollection;r.expected=expected(request.expected);result=service.edit(r);}
        else {HostStorageMutationService::SetActiveRequest r;r.workspaceId=int(request.workspaceId);r.storageId=int(request.storageId);r.active=request.active;r.expected=expected(request.expected);result=service.setActive(r);}
        if(!result.success)return fail(result.error);
        const auto type=StorageLocationTypeRepository(db).getById(result.location.locationTypeId());if(!type)return fail({HostStorageMutationService::ErrorCode::DatabaseFailure,QStringLiteral("Unable to load the Storage location type.")});
        RemoteReadDto::StorageDetail detail;detail.storageId=result.location.id();detail.parentStorageId=result.location.parentLocationId();detail.name=result.location.name();detail.displayPath=result.displayPath;detail.typeName=type->name();detail.sortOrder=result.location.sortOrder();detail.active=result.location.isActive();detail.allowsInventory=result.location.allowsInventory();detail.allowsCollection=result.location.allowsCollection();detail.workspaceId=result.location.workspaceId();detail.storageTypeId=result.location.locationTypeId();detail.description=result.location.description();detail.createdUtc=result.location.createdUtc();detail.modifiedUtc=result.location.modifiedUtc();
        HostWriteExecutor::MutationOutcome outcome;outcome.success=true;outcome.authoritative={{"created",operation=="storage.add"},{"storage",RemoteReadJson::toJson(detail)}};outcome.publicationWorkflow=HostMutationPublicationService::Workflow::Storage;outcome.publicationScope.workspaceId=request.workspaceId;outcome.publicationScope.storageLocationId=detail.storageId;return outcome;};
}
