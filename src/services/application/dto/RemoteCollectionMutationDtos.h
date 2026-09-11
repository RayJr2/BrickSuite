#pragma once
#include "RemoteMutationDtos.h"

namespace RemoteCollectionMutationDto {
struct ExpectedState { QString modifiedUtc,type,setNumber,minifigNumber,state,condition,completeness,nickname,notes; qint64 storageId=0,sourceBuildId=0; bool active=true; };
struct Request { qint64 workspaceId=0,collectionItemId=0,storageId=0,buildId=0; QString mutationId,sourceType,setNumber,minifigNumber,state,condition,completeness,nickname,notes; bool desiredActive=true; ExpectedState expected; };
struct Result { QString mutationId,operation; bool replayed=false; QJsonObject item; };
RemoteMutationDto::Metadata toMetadata(const QString&,const Request&);
bool fromMetadata(const QString&,const RemoteMutationDto::Metadata&,Request*,RemoteMutationDto::Error* = nullptr);
bool resultFromMutation(const RemoteMutationDto::Result&,Result*,RemoteMutationDto::Error* = nullptr);
}
