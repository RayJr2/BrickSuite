#pragma once
#include "dto/RemoteStorageMutationDtos.h"
#include <QObject>
#include <functional>
class RemoteMutationApplicationServices;
class RemoteStorageMutationApplicationService : public QObject
{
public:
    using Completion=std::function<void(const RemoteStorageMutationDto::Result&)>;
    using Failure=std::function<void(const RemoteMutationDto::Error&)>;
    explicit RemoteStorageMutationApplicationService(RemoteMutationApplicationServices&,QObject*parent=nullptr);
    bool isAvailableFor(const QString&operation)const;
    QString add(const RemoteStorageMutationDto::Request&,QObject*,Completion,Failure);
    QString edit(const RemoteStorageMutationDto::Request&,QObject*,Completion,Failure);
    QString setActive(const RemoteStorageMutationDto::Request&,QObject*,Completion,Failure);
private:
    QString submit(const QString&,const RemoteStorageMutationDto::Request&,QObject*,Completion,Failure);
    RemoteMutationApplicationServices&m_mutations;
};
