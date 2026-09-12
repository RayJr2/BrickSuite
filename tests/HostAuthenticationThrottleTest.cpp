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
