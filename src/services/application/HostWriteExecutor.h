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
    Q_OBJECT
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
    int activeMutationCount() const;
    bool isIdle() const;
    void stopAccepting();
    void startAccepting();
    void closeConnectionAsync(std::function<void(bool, const QString&)> completion);
    QString connectionName() const;
    void enqueue(const RemoteMutationDto::RequestContext& context, const QString& requestHash,
                 Mutation mutation, QObject* callbackContext,
                 Completion completion, Failure failure);
    void shutdown();

signals:
    void activityChanged();
    void drained();
    void connectionClosed(bool success, const QString& error);

private:
    class Worker;
    QThread m_thread;
    Worker* m_worker = nullptr;
    Publisher m_publisher;
    QString m_connectionName;
    std::atomic_bool m_accepting{true};
    std::atomic_int m_queued{0};
    std::atomic_int m_active{0};
};
