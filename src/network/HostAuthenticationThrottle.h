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
