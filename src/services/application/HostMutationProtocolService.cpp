#include "HostMutationProtocolService.h"
#include "../../network/BrickSuiteOperationDispatcher.h"

HostMutationProtocolService::HostMutationProtocolService(
    const QString& databasePath, HostWriteExecutor::Publisher publisher, QObject* parent)
    : QObject(parent), m_executor(databasePath, std::move(publisher), this) {}

HostWriteExecutor& HostMutationProtocolService::executor() { return m_executor; }

void HostMutationProtocolService::registerInternalOperation(
    BrickSuiteOperationDispatcher& dispatcher, const QString& operation,
    const QString& capability, HostWriteExecutor::Mutation mutation)
{
    dispatcher.registerAsyncOperation(operation, true,
        [this, operation, mutation=std::move(mutation)](
            const BrickSuiteProtocol::Message& request,
            BrickSuiteOperationDispatcher::Completion completion) {
            if (request.protocolMinor < 2) {
                completion(BrickSuiteProtocol::errorResponse(request, QStringLiteral("FORBIDDEN"),
                    QStringLiteral("This mutation requires BrickSuite protocol 1.2.")));
                return;
            }
            RemoteMutationDto::Metadata metadata;
            RemoteMutationDto::Error error;
            if (!RemoteMutationDto::parseMetadata(request.payload, &metadata, &error)) {
                completion(BrickSuiteProtocol::errorResponse(request, error.code, error.message,
                                                              error.retryable));
                return;
            }
            RemoteMutationDto::RequestContext context{operation, metadata.workspaceId,
                metadata.mutationId, QStringLiteral("FullBrickSuiteClient"),
                request.protocolMajor, request.protocolMinor};
            m_executor.enqueue(context, RemoteMutationDto::requestHash(operation, metadata),
                mutation, this,
                [request, completion](const RemoteMutationDto::Result& result) {
                    completion(BrickSuiteProtocol::response(request,
                        RemoteMutationDto::resultToJson(result)));
                },
                [request, completion](const RemoteMutationDto::Error& failure) {
                    completion(BrickSuiteProtocol::errorResponse(request, failure.code,
                        failure.message, failure.retryable));
                });
        }, 2, capability);
}

void HostMutationProtocolService::registerOperation(
    BrickSuiteOperationDispatcher& dispatcher, const QString& operation,
    const QString& capability, MutationFactory factory)
{
    dispatcher.registerAsyncOperation(operation, true,
        [this, operation, factory=std::move(factory)](
            const BrickSuiteProtocol::Message& request,
            BrickSuiteOperationDispatcher::Completion completion) {
            if (request.protocolMinor < 2) {
                completion(BrickSuiteProtocol::errorResponse(request, QStringLiteral("FORBIDDEN"),
                    QStringLiteral("This mutation requires BrickSuite protocol 1.2.")));
                return;
            }
            RemoteMutationDto::Metadata metadata;
            RemoteMutationDto::Error error;
            if (!RemoteMutationDto::parseMetadata(request.payload, &metadata, &error)) {
                completion(BrickSuiteProtocol::errorResponse(request, error.code, error.message,
                                                              error.retryable));
                return;
            }
            HostWriteExecutor::Mutation mutation = factory(metadata, &error);
            if (!mutation) {
                completion(BrickSuiteProtocol::errorResponse(request,
                    error.code.isEmpty() ? QStringLiteral("INVALID_REQUEST") : error.code,
                    error.message.isEmpty() ? QStringLiteral("The mutation request is invalid.")
                                            : error.message,
                    error.retryable));
                return;
            }
            RemoteMutationDto::RequestContext context{operation, metadata.workspaceId,
                metadata.mutationId, QStringLiteral("FullBrickSuiteClient"),
                request.protocolMajor, request.protocolMinor};
            m_executor.enqueue(context, RemoteMutationDto::requestHash(operation, metadata),
                std::move(mutation), this,
                [request, completion](const RemoteMutationDto::Result& result) {
                    completion(BrickSuiteProtocol::response(request,
                        RemoteMutationDto::resultToJson(result)));
                },
                [request, completion](const RemoteMutationDto::Error& failure) {
                    completion(BrickSuiteProtocol::errorResponse(request, failure.code,
                        failure.message, failure.retryable));
                });
        }, 2, capability);
}
