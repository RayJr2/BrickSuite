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
