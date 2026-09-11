#pragma once

#include "dto/RemoteBuildMutationDtos.h"

#include <QObject>
#include <functional>

class RemoteMutationApplicationServices;

class RemoteBuildMutationApplicationService : public QObject
{
public:
    using Completion=std::function<void(const RemoteBuildMutationDto::Result&)>;
    using Failure=std::function<void(const RemoteMutationDto::Error&)>;
    explicit RemoteBuildMutationApplicationService(RemoteMutationApplicationServices& mutations,
                                                   QObject* parent=nullptr);
    bool isAvailableFor(const QString& operation) const;
    QString submit(const QString& operation,const RemoteBuildMutationDto::Request& request,
                   QObject* context,Completion completion,Failure failure);
private:
    RemoteMutationApplicationServices& m_mutations;
};
