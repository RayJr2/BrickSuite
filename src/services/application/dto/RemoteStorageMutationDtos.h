#pragma once

#include "RemoteMutationDtos.h"
#include "RemoteReadDtos.h"

namespace RemoteStorageMutationDto {

struct ExpectedState {
    QString modifiedUtc;
    qint64 parentStorageId = 0;
    qint64 storageTypeId = 0;
    QString name;
    QString description;
    int sortOrder = 0;
    bool active = true;
    bool allowsInventory = true;
    bool allowsCollection = false;
};
struct Request {
    qint64 workspaceId = 0;
    qint64 storageId = 0;
    QString mutationId;
    QString name;
    QString description;
    qint64 parentStorageId = 0;
    qint64 storageTypeId = 0;
    bool allowsInventory = true;
    bool allowsCollection = false;
    bool active = true;
    ExpectedState expected;
};
struct Result {
    QString mutationId;
    QString operation;
    bool replayed = false;
    bool created = false;
    RemoteReadDto::StorageDetail storage;
};

RemoteMutationDto::Metadata toMetadata(const QString& operation,const Request& request);
bool fromMetadata(const QString& operation,const RemoteMutationDto::Metadata& metadata,
                  Request* request,RemoteMutationDto::Error* error=nullptr);
bool resultFromMutation(const RemoteMutationDto::Result& source,Result* result,
                        RemoteMutationDto::Error* error=nullptr);
}
