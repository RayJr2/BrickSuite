#include "RemoteMutationApplicationServices.h"
#include "../../network/BrickSuiteWebSocketClient.h"

RemoteMutationApplicationServices::RemoteMutationApplicationServices(
    BrickSuiteWebSocketClient& client, QObject* parent)
    : QObject(parent), m_client(client) {}

bool RemoteMutationApplicationServices::isAvailableFor(const QString& operation,
                                                         const QString& capability) const
{
    return m_client.status().state == BrickSuiteConnectionState::ConnectedAuthenticated
           && m_client.supportsOperation(operation)
           && m_client.supportsCapability(capability);
}

QString RemoteMutationApplicationServices::submit(
    const QString& operation, const QString& capability,
    const RemoteMutationDto::Metadata& metadata, QObject* context,
    std::function<void(const RemoteMutationDto::Result&)> completion,
    std::function<void(const RemoteMutationDto::Error&)> failure)
{
    if (!isAvailableFor(operation, capability)) {
        if (failure) {
            RemoteMutationDto::Error error{QStringLiteral("FORBIDDEN"),
                QStringLiteral("The connected Host does not permit this operation."), false};
            error.mutationId = metadata.mutationId;
            failure(error);
        }
        return {};
    }
    const QJsonObject payload{{QStringLiteral("workspaceId"), metadata.workspaceId},
                              {QStringLiteral("mutationId"), metadata.mutationId},
                              {QStringLiteral("expected"), metadata.expected},
                              {QStringLiteral("mutation"), metadata.mutation}};
    return m_client.sendRequest(operation, payload, context,
        [completion, failure](const QJsonObject& json) {
            RemoteMutationDto::Result result;
            RemoteMutationDto::Error error;
            if (!RemoteMutationDto::resultFromJson(json, &result, &error)) {
                if (failure) failure(error);
            } else if (completion) completion(result);
        },
        [failure, mutationId=metadata.mutationId](const BrickSuiteProtocol::Error& error) {
            if (!failure) return;
            const bool unknown = error.code == QStringLiteral("TIMEOUT")
                                 || error.code == QStringLiteral("DISCONNECTED");
            RemoteMutationDto::Error mapped{error.code, error.message, error.retryable,
                unknown ? RemoteMutationDto::Outcome::Unknown
                        : RemoteMutationDto::Outcome::DefinitiveFailure};
            mapped.mutationId = mutationId;
            failure(mapped);
        });
}
