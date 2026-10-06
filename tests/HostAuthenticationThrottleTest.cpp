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

#include "../src/network/HostAuthenticationThrottle.h"

#include <iostream>

namespace {
bool check(bool condition, const char* message)
{
    if (!condition) std::cerr << "FAIL: " << message << '\n';
    return condition;
}
}

int main()
{
    bool ok = true;
    HostAuthenticationThrottle throttle;
    const QString deviceA = QStringLiteral("device:a");
    const QString deviceB = QStringLiteral("device:b");
    qint64 now = 1000;

    ok &= check(throttle.check(deviceA, now).allowed, "new key is allowed");
    ok &= check(!throttle.recordFailure(deviceA, now), "first failure does not lock");
    ok &= check(!throttle.recordFailure(deviceA, now + 1), "second failure does not lock");
    ok &= check(throttle.recordFailure(deviceA, now + 2), "threshold activates lockout");
    const auto locked = throttle.check(deviceA, now + 3);
    ok &= check(!locked.allowed && locked.retryAfterMs > 0
                    && locked.retryAfterMs <= HostAuthenticationThrottle::LockoutMs,
                "lockout survives reconnect-equivalent checks and is bounded");
    ok &= check(throttle.check(deviceB, now + 3).allowed,
                "unrelated paired device remains allowed");

    const auto expired = throttle.check(
        deviceA, now + 2 + HostAuthenticationThrottle::LockoutMs);
    ok &= check(expired.allowed && expired.expiredLockout,
                "lockout expires automatically");
    ok &= check(!throttle.recordFailure(deviceA, now + 2 + HostAuthenticationThrottle::LockoutMs),
                "expired lockout starts with a fresh bounded budget");
    ok &= check(throttle.recordSuccess(deviceA)
                    && throttle.check(deviceA, now + 4).allowed,
                "successful authentication clears prior failure state");

    throttle.recordFailure(deviceA, now);
    const auto quiet = throttle.check(deviceA, now + HostAuthenticationThrottle::QuietExpiryMs);
    ok &= check(quiet.allowed && !throttle.recordSuccess(deviceA),
                "quiet-period expiry removes stale failure state");
    throttle.recordFailure(deviceA, now);
    throttle.clear();
    ok &= check(throttle.check(deviceA, now).allowed && !throttle.recordSuccess(deviceA),
                "Host restart clears transient throttle state");
    return ok ? 0 : 1;
}
