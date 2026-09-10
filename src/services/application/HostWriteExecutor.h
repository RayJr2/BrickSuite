#pragma once

#include "dto/RemoteMutationDtos.h"
#include "HostMutationPublicationService.h"

#include <QObject>
#include <QThread>
#include <atomic>
#include <functional>

class QSqlDatabase;

class HostWriteExecutor : public QObject
{
public:
    static constexpr int MaximumQueuedMutations = 16;
    static constexpr int ReceiptRetentionDays = 90;
    static constexpr int ReceiptCleanupBatch = 250;

    struct MutationOutcome {
        bool success = false;
        QJsonObject authoritative;
        RemoteMutationDto::Error error;
        HostMutationPublicationService::Workflow publicationWorkflow =
            HostMutationPublicationService::Workflow::Workspace;
        HostMutationPublicationService::Scope publicationScope;
    };
    using Mutation = std::function<MutationOutcome(const QSqlDatabase&)>;
    using Completion = std::function<void(const RemoteMutationDto::Result&)>;
    using Failure = std::function<void(const RemoteMutationDto::Error&)>;
    using Publisher = std::function<void(HostMutationPublicationService::Workflow,
                                         const HostMutationPublicationService::Scope&)>;

    explicit HostWriteExecutor(const QString& databasePath, Publisher publisher = {},
                               QObject* parent = nullptr);
    ~HostWriteExecutor() override;

    bool isAccepting() const;
    int queuedMutationCount() const;
    QString connectionName() const;
    void enqueue(const RemoteMutationDto::RequestContext& context, const QString& requestHash,
                 Mutation mutation, QObject* callbackContext,
                 Completion completion, Failure failure);
    void shutdown();

private:
    class Worker;
    QThread m_thread;
    Worker* m_worker = nullptr;
    Publisher m_publisher;
    QString m_connectionName;
    std::atomic_bool m_accepting{true};
    std::atomic_int m_queued{0};
};
