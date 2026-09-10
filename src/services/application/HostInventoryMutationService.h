#pragma once

#include "HostWriteExecutor.h"
#include "dto/RemoteInventoryMutationDtos.h"

class HostInventoryMutationService
{
public:
    static HostWriteExecutor::Mutation createMutation(
        const QString& operation, const RemoteMutationDto::Metadata& metadata,
        RemoteMutationDto::Error* error = nullptr);
};
