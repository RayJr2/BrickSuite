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

#include "HostAuthenticationThrottle.h"

HostAuthenticationThrottle::Decision
HostAuthenticationThrottle::check(const QString& key, qint64 nowMs)
{
    prune(nowMs);
    auto it = m_entries.find(key);
    if (it == m_entries.end()) return {};
    if (it->lockedUntilMs > nowMs)
        return {false, it->lockedUntilMs - nowMs, false};
    if (it->lockedUntilMs > 0) {
        m_entries.erase(it);
        return {true, 0, true};
    }
    return {};
}

bool HostAuthenticationThrottle::recordFailure(const QString& key, qint64 nowMs)
{
    prune(nowMs);
    if (!m_entries.contains(key) && m_entries.size() >= MaximumTrackedKeys) {
        auto oldest = m_entries.begin();
        for (auto it = m_entries.begin(); it != m_entries.end(); ++it)
            if (it->lastFailureMs < oldest->lastFailureMs) oldest = it;
        m_entries.erase(oldest);
    }
    Entry& entry = m_entries[key];
    entry.lastFailureMs = nowMs;
    ++entry.failures;
    if (entry.failures < FailureThreshold) return false;
    entry.lockedUntilMs = nowMs + LockoutMs;
    return true;
}

bool HostAuthenticationThrottle::recordSuccess(const QString& key)
{
    return m_entries.remove(key) > 0;
}

void HostAuthenticationThrottle::clear()
{
    m_entries.clear();
}

void HostAuthenticationThrottle::prune(qint64 nowMs)
{
    for (auto it = m_entries.begin(); it != m_entries.end();) {
        if (nowMs - it->lastFailureMs >= QuietExpiryMs) it = m_entries.erase(it);
        else ++it;
    }
}
