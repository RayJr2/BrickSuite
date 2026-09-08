#pragma once

#include <atomic>
#include <memory>

#include <QDebug>

class RebrickableImportCancellation
{
public:
    RebrickableImportCancellation() : m_cancelled(std::make_shared<std::atomic_bool>(false)) {}
    void requestCancellation() const
    {
        if (!m_cancelled->exchange(true, std::memory_order_relaxed))
            qInfo() << "Rebrickable import cancellation requested.";
    }
    bool isCancellationRequested() const { return m_cancelled->load(std::memory_order_relaxed); }

private:
    std::shared_ptr<std::atomic_bool> m_cancelled;
};
