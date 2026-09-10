#pragma once

#include "HostWriteExecutor.h"
#include "dto/RemotePullingMutationDtos.h"

class HostPullingMutationService
{
public:
    static HostWriteExecutor::Mutation createMutation(
        const RemoteMutationDto::Metadata& metadata, RemoteMutationDto::Error* error = nullptr);
};
