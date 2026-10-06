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
