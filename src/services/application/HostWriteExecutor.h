/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 *
 * This file is part of BrickSuite.
 *
 * BrickSuite is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, version 3 of the License.
 *
 * BrickSuite is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with BrickSuite. If not, see <https://www.gnu.org/licenses/>.
 */

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
