#pragma once

#include "dto/RemotePullingMutationDtos.h"

#include <QObject>
#include <functional>

class RemoteMutationApplicationServices;

class RemotePullingApplicationService : public QObject
{
public:
    explicit RemotePullingApplicationService(RemoteMutationApplicationServices& mutations,
                                             QObject* parent = nullptr);
    bool isAvailable() const;
    QString record(const RemotePullingMutationDto::Request& request, QObject* context,
                   std::function<void(const RemotePullingMutationDto::Result&)> completion,
                   std::function<void(const RemoteMutationDto::Error&)> failure);
private:
    RemoteMutationApplicationServices& m_mutations;
};
