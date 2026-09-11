#pragma once

#include "dto/RemotePartReferenceMutationDtos.h"
#include <QObject>
#include <functional>

class RemoteMutationApplicationServices;
class RemotePartReferenceMutationApplicationService : public QObject
{
public:
    using Completion=std::function<void(const RemotePartReferenceMutationDto::Result&)>;
    using Failure=std::function<void(const RemoteMutationDto::Error&)>;
    explicit RemotePartReferenceMutationApplicationService(RemoteMutationApplicationServices&,
                                                            QObject* parent=nullptr);
    bool isAvailableFor(const QString& operation) const;
    QString add(const RemotePartReferenceMutationDto::Request&,QObject*,Completion,Failure);
    QString remove(const RemotePartReferenceMutationDto::Request&,QObject*,Completion,Failure);
private:
    QString submit(const QString&,const RemotePartReferenceMutationDto::Request&,QObject*,Completion,Failure);
    RemoteMutationApplicationServices& m_mutations;
};
