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

#include <atomic>
#include <functional>
#include <memory>

#include <QDebug>

using RebrickableRowProgress = std::function<void(qint64)>;

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
