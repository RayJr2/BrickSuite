#pragma once

#include "dto/RemoteInventoryMutationDtos.h"

#include <QObject>
#include <functional>

class RemoteMutationApplicationServices;

class RemoteInventoryMutationApplicationService : public QObject
{
public:
    using Completion = std::function<void(const RemoteInventoryMutationDto::Result&)>;
    using Failure = std::function<void(const RemoteMutationDto::Error&)>;

    explicit RemoteInventoryMutationApplicationService(RemoteMutationApplicationServices& mutations,
                                                        QObject* parent = nullptr);
    bool isAvailableFor(const QString& operation) const;
    QString add(const RemoteInventoryMutationDto::Request&, QObject*, Completion, Failure);
    QString edit(const RemoteInventoryMutationDto::Request&, QObject*, Completion, Failure);
    QString move(const RemoteInventoryMutationDto::Request&, QObject*, Completion, Failure);
    QString correct(const RemoteInventoryMutationDto::Request&, QObject*, Completion, Failure);
    QString remove(const RemoteInventoryMutationDto::Request&, QObject*, Completion, Failure);
    QString markLost(const RemoteInventoryMutationDto::Request&, QObject*, Completion, Failure);
    QString markFound(const RemoteInventoryMutationDto::Request&, QObject*, Completion, Failure);

private:
    QString submit(const QString&, const RemoteInventoryMutationDto::Request&, QObject*, Completion, Failure);
    RemoteMutationApplicationServices& m_mutations;
};
