#pragma once
#include "HostWriteExecutor.h"
class HostCollectionMutationService { public: static HostWriteExecutor::Mutation createMutation(const QString&,const RemoteMutationDto::Metadata&,RemoteMutationDto::Error* = nullptr); };
