#pragma once

#include "HostWriteExecutor.h"

#include <QObject>

class BrickSuiteOperationDispatcher;

// Domain-neutral registration boundary. Production operational mutations are
// intentionally not registered until their application services exist.
class HostMutationProtocolService : public QObject
{
public:
    explicit HostMutationProtocolService(const QString& databasePath,
                                          HostWriteExecutor::Publisher publisher = {},
                                          QObject* parent = nullptr);
    HostWriteExecutor& executor();

    void registerInternalOperation(BrickSuiteOperationDispatcher& dispatcher,
        const QString& operation, const QString& capability,
        HostWriteExecutor::Mutation mutation);

private:
    HostWriteExecutor m_executor;
};
