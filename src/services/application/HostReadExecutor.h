#pragma once

#include "ApplicationServices.h"
#include "../../models/StorageLocation.h"

#include <QObject>
#include <QThread>
#include <atomic>
#include <functional>

class PartReferenceManifest;

// Serialized, transport-neutral execution surface for Host-authoritative reads.
// The database connection and connection-bound service graph are created, used,
// and destroyed exclusively on m_thread.
class HostReadExecutor : public QObject
{
    Q_OBJECT
public:
    static constexpr int MaximumQueuedReads = 64;

    explicit HostReadExecutor(const QString& databasePath, QObject* parent = nullptr);
    ~HostReadExecutor() override;

    QString connectionName() const;
    bool isAccepting() const;
    int queuedReadCount() const;
    void shutdown();

    using ErrorCallback = std::function<void(const QString&)>;
    void listWorkspaces(QObject* context,
                        std::function<void(const QList<Workspace>&)> completion,
                        ErrorCallback failure = {});
    void getWorkspace(int workspaceId, QObject* context,
                      std::function<void(const std::optional<Workspace>&)> completion,
                      ErrorCallback failure = {});
    void listStorage(int workspaceId, QObject* context,
                     std::function<void(const QList<StorageLocation>&)> completion,
                     ErrorCallback failure = {});
    void searchInventory(const InventorySearchCriteria& criteria, QObject* context,
                         std::function<void(const InventoryApplicationService::Page&)> completion,
                         ErrorCallback failure = {});
    void getInventory(int inventoryRecordId, QObject* context,
                      std::function<void(const std::optional<InventoryRecord>&)> completion,
                      ErrorCallback failure = {});
    void inventoryHistory(int workspaceId, int partId, int colorId, QObject* context,
                          std::function<void(const QList<InventoryHistoryResult>&)> completion,
                          ErrorCallback failure = {});
    void listBuilds(int workspaceId, bool includeArchived, QObject* context,
                    std::function<void(const QList<Build>&)> completion,
                    ErrorCallback failure = {});
    void getBuild(int buildId, QObject* context,
                  std::function<void(const std::optional<Build>&)> completion,
                  ErrorCallback failure = {});
    void buildRequirements(int buildId, QObject* context,
                           std::function<void(const QList<BuildRequirement>&)> completion,
                           ErrorCallback failure = {});
    void missingParts(int workspaceId, int buildId, QObject* context,
                      std::function<void(const QList<MissingPartsService::MissingPart>&)> completion,
                      ErrorCallback failure = {});
    void pullingView(int buildId, QObject* context,
                     std::function<void(const BuildPullingService::PullingView&)> completion,
                     ErrorCallback failure = {});
    void searchCollection(const CollectionSearchCriteria& criteria, QObject* context,
                          std::function<void(const CollectionApplicationService::Page&)> completion,
                          ErrorCallback failure = {});
    void getCollection(int itemId, QObject* context,
                       std::function<void(const std::optional<CollectionSearchResult>&)> completion,
                       ErrorCallback failure = {});
    void effectivePartReference(const PartReferenceManifest& manifest, QObject* context,
                                std::function<void(const QList<PartReferenceEntry>&,
                                                   const QString&)> completion,
                                ErrorCallback failure = {});

private:
    class Worker;
    using Task = std::function<void(ApplicationServices&, const QSqlDatabase&)>;
    void enqueue(const QString& label, Task task, QObject* context, ErrorCallback failure);
    static void deliverFailure(QObject* context, const ErrorCallback& failure,
                               const QString& message);

    QThread m_thread;
    Worker* m_worker = nullptr;
    std::atomic_bool m_accepting{true};
    std::atomic_int m_queued{0};
    QString m_connectionName;
};
