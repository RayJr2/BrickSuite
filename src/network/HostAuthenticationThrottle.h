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

#include <QHash>
#include <QString>

class HostAuthenticationThrottle
{
public:
    static constexpr int FailureThreshold = 3;
    static constexpr qint64 LockoutMs = 30 * 1000;
    static constexpr qint64 QuietExpiryMs = 10 * 60 * 1000;
    static constexpr int MaximumTrackedKeys = 1024;

    struct Decision {
        bool allowed = true;
        qint64 retryAfterMs = 0;
        bool expiredLockout = false;
    };

    Decision check(const QString& key, qint64 nowMs);
    bool recordFailure(const QString& key, qint64 nowMs);
    bool recordSuccess(const QString& key);
    void clear();

private:
    struct Entry {
        int failures = 0;
        qint64 lastFailureMs = 0;
        qint64 lockedUntilMs = 0;
    };

    void prune(qint64 nowMs);
    QHash<QString, Entry> m_entries;
};
