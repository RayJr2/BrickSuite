#pragma once
#include "dto/RemoteCollectionMutationDtos.h"
#include <QObject>
#include <functional>
class RemoteMutationApplicationServices;
class RemoteCollectionMutationApplicationService:public QObject{public:using Completion=std::function<void(const RemoteCollectionMutationDto::Result&)>;using Failure=std::function<void(const RemoteMutationDto::Error&)>;explicit RemoteCollectionMutationApplicationService(RemoteMutationApplicationServices&,QObject* = nullptr);bool isAvailableFor(const QString&)const;QString submit(const QString&,const RemoteCollectionMutationDto::Request&,QObject*,Completion,Failure);private:RemoteMutationApplicationServices&m_mutations;};
