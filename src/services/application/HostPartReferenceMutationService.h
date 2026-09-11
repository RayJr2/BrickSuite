#pragma once

#include "HostWriteExecutor.h"

class HostPartReferenceMutationService
{
public:
    static HostWriteExecutor::Mutation createMutation(
        const QString& operation, const RemoteMutationDto::Metadata& metadata,
        RemoteMutationDto::Error* error = nullptr);
};
