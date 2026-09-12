#pragma once

#include "HostWriteExecutor.h"

#include <QObject>
#include <functional>

class BrickSuiteOperationDispatcher;

// Domain-neutral registration boundary. Production operational mutations are
// intentionally not registered until their application services exist.
class HostMutationProtocolService : public QObject
{
public:
    explicit HostMutationProtocolService(const QString& databasePath,
                                          HostWriteExecutor::Publisher publisher = {},
                                          const QString& dataEpoch = QString(),
                                          QObject* parent = nullptr);
    HostWriteExecutor& executor();

    void registerInternalOperation(BrickSuiteOperationDispatcher& dispatcher,
        const QString& operation, const QString& capability,
        HostWriteExecutor::Mutation mutation);
    using MutationFactory = std::function<HostWriteExecutor::Mutation(
        const RemoteMutationDto::Metadata&, RemoteMutationDto::Error*)>;
    void registerOperation(BrickSuiteOperationDispatcher& dispatcher,
        const QString& operation, const QString& capability, MutationFactory factory);

private:
    HostWriteExecutor m_executor;
    QString m_dataEpoch;
};
