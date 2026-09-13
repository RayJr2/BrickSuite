#pragma once

#include "../../models/InventoryBuildability.h"
#include <QObject>
#include <atomic>
#include <functional>
#include <memory>

class QThread;
class InventoryBuildabilityWorker;

class InventoryBuildabilityService : public QObject
{
    Q_OBJECT
public:
    using Completion=std::function<void(quint64,const InventoryBuildabilitySearchResult&)>;
    explicit InventoryBuildabilityService(const QString& databasePath,QObject* parent=nullptr);
    ~InventoryBuildabilityService() override;
    quint64 search(const InventoryBuildabilitySearch&,QObject* context,Completion);
    void cancel();
private:
    QThread* m_thread=nullptr;
    InventoryBuildabilityWorker* m_worker=nullptr;
    std::shared_ptr<std::atomic<quint64>> m_generation;
};
