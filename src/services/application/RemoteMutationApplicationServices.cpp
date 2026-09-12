#include "RemoteMutationApplicationServices.h"
#include "../../network/BrickSuiteWebSocketClient.h"
#include <QDebug>

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
            const bool maintenance = m_client.status().state
                == BrickSuiteConnectionState::HostMaintenance;
            RemoteMutationDto::Error error{
                maintenance ? QStringLiteral("HOST_MAINTENANCE") : QStringLiteral("FORBIDDEN"),
                maintenance
                    ? QStringLiteral("BrickSuite Host is temporarily in maintenance. Try again after it returns.")
                    : QStringLiteral("The connected Host does not permit this operation."),
                maintenance};
            error.outcome = RemoteMutationDto::Outcome::DefinitiveFailure;
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
        [operation, mutationId=metadata.mutationId, completion, failure](const QJsonObject& json) {
            qDebug() << "Remote mutation response received" << operation << mutationId.left(8);
            RemoteMutationDto::Result result;
            RemoteMutationDto::Error error;
            if (!RemoteMutationDto::resultFromJson(json, &result, &error)) {
                error.outcome = RemoteMutationDto::Outcome::Unknown;
                error.mutationId = mutationId;
                qWarning() << "Remote mutation success response could not be decoded"
                           << operation << mutationId.left(8) << error.message;
                if (failure) failure(error);
            } else if (result.operation != operation || result.mutationId != mutationId) {
                error = {QStringLiteral("INTERNAL_ERROR"),
                    QStringLiteral("The Host returned a mismatched mutation result."), false,
                    RemoteMutationDto::Outcome::Unknown};
                error.mutationId = mutationId;
                qWarning() << "Remote mutation result rejected for identity mismatch"
                           << operation << mutationId.left(8);
                if (failure) failure(error);
            } else {
                qDebug() << "Remote mutation response decoded" << operation
                         << mutationId.left(8) << "replayed" << result.replayed;
                if (completion) completion(result);
            }
        },
        [operation, failure, mutationId=metadata.mutationId](const BrickSuiteProtocol::Error& error) {
            if (!failure) return;
            const bool unknown = error.code == QStringLiteral("TIMEOUT")
                                 || error.code == QStringLiteral("DISCONNECTED");
            RemoteMutationDto::Error mapped{error.code, error.message, error.retryable,
                unknown ? RemoteMutationDto::Outcome::Unknown
                        : RemoteMutationDto::Outcome::DefinitiveFailure};
            mapped.mutationId = mutationId;
            qDebug() << "Remote mutation request failed" << operation << mutationId.left(8)
                     << error.code << (unknown ? "unknown outcome" : "definitive failure");
            failure(mapped);
        });
}
