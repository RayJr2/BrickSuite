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
    const QString epoch = m_client.dataEpochSupported() ? m_client.dataEpoch() : QString();
    const auto retainedEpoch = m_epochByUnknownMutation.constFind(metadata.mutationId);
    if (retainedEpoch != m_epochByUnknownMutation.constEnd() && *retainedEpoch != epoch) {
        if (failure) {
            RemoteMutationDto::Error error{
                QStringLiteral("STALE_DATA_EPOCH"),
                QStringLiteral("The Host database has changed. This uncertain request cannot be retried safely."),
                false, RemoteMutationDto::Outcome::DefinitiveFailure};
            error.mutationId = metadata.mutationId;
            failure(error);
        }
        return {};
    }
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
    m_epochByUnknownMutation.insert(metadata.mutationId, epoch);
    QJsonObject payload{{QStringLiteral("workspaceId"), metadata.workspaceId},
                              {QStringLiteral("mutationId"), metadata.mutationId},
                              {QStringLiteral("expected"), metadata.expected},
                              {QStringLiteral("mutation"), metadata.mutation}};
    if (!epoch.isEmpty()) payload.insert(QStringLiteral("dataEpoch"), epoch);
    return m_client.sendRequest(operation, payload, context,
        [this, operation, mutationId=metadata.mutationId, completion, failure](const QJsonObject& json) {
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
                m_epochByUnknownMutation.remove(mutationId);
                qDebug() << "Remote mutation response decoded" << operation
                         << mutationId.left(8) << "replayed" << result.replayed;
                if (completion) completion(result);
            }
        },
        [this, operation, failure, mutationId=metadata.mutationId](const BrickSuiteProtocol::Error& error) {
            const bool unknown = error.code == QStringLiteral("TIMEOUT")
                                 || error.code == QStringLiteral("DISCONNECTED");
            if (!unknown) m_epochByUnknownMutation.remove(mutationId);
            if (!failure) return;
            RemoteMutationDto::Error mapped{error.code, error.message, error.retryable,
                unknown ? RemoteMutationDto::Outcome::Unknown
                        : RemoteMutationDto::Outcome::DefinitiveFailure};
            mapped.mutationId = mutationId;
            qDebug() << "Remote mutation request failed" << operation << mutationId.left(8)
                     << error.code << (unknown ? "unknown outcome" : "definitive failure");
            failure(mapped);
        });
}
