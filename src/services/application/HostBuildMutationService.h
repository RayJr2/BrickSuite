#pragma once

#include "HostWriteExecutor.h"

class HostBuildMutationService
{
public:
    static HostWriteExecutor::Mutation createMutation(
        const QString& operation, const RemoteMutationDto::Metadata& metadata,
        RemoteMutationDto::Error* error = nullptr);
};
