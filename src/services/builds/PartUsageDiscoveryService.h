#pragma once

#include "../../models/PartUsageDiscovery.h"
#include <QObject>
#include <functional>
#include <memory>
#include <atomic>

class QThread;
class PartUsageDiscoveryWorker;

class PartUsageDiscoveryService : public QObject
{
    Q_OBJECT
public:
    using Completion = std::function<void(quint64, const PartUsageSearchResult&)>;
    explicit PartUsageDiscoveryService(const QString& databasePath, QObject* parent = nullptr);
    ~PartUsageDiscoveryService() override;

    quint64 search(const PartUsageSearch& request, QObject* context, Completion completion);
    void cancel();

private:
    QThread* m_thread = nullptr;
    PartUsageDiscoveryWorker* m_worker = nullptr;
    std::shared_ptr<std::atomic<quint64>> m_generation;
};
