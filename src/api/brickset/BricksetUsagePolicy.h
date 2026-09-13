#pragma once

#include <QDate>
#include <QDateTime>
#include <QList>
#include <QObject>
#include <functional>

class BricksetUsagePolicy : public QObject
{
    Q_OBJECT
public:
    enum class Status { Unavailable, Refreshing, Available, ThresholdReached };
    enum class Decision { Admitted, ThresholdReached, UsageUnavailable };
    struct State {
        Status status = Status::Unavailable;
        bool snapshotAvailable = false;
        int authoritativeCount = 0;
        int effectiveCount = -1;
        int threshold = 0;
        int safeCallsRemaining = 0;
        QDate providerDate;
        QDateTime lastSuccessfulRefreshUtc;
    };
    using Completion = std::function<void(Decision)>;
    using RefreshStarter = std::function<void()>;

    static constexpr qint64 SuccessfulSnapshotTtlMs = 15 * 60 * 1000;
    static constexpr qint64 FailureCooldownMs = 2 * 60 * 1000;

    static BricksetUsagePolicy& instance();
    State state(int threshold) const;
    int sessionCallCount() const { return m_sessionCalls; }
    quint64 credentialGeneration() const { return m_credentialGeneration; }
    void requestAdmission(int threshold, Completion completion, RefreshStarter startRefresh);
    void requestRefresh(Completion completion, RefreshStarter startRefresh, bool bypassCooldown);
    void completeRefresh(quint64 credentialGeneration, bool success,
                         int authoritativeCount, const QDate& providerDate);
    void invalidateForCredentialChange();
    void noteQuotaResponse();

#ifdef BRICKSUITE_TESTING
    void resetForTesting();
    void setNowForTesting(const QDateTime& nowUtc);
#endif

signals:
    void stateChanged();

private:
    struct ObservableState {
        bool snapshotAvailable = false;
        bool refreshInFlight = false;
        bool quotaSuppressed = false;
        int authoritativeCount = 0;
        int sessionCalls = 0;
        int sessionCallsAtSnapshot = 0;
        QDate providerDate;
        QDateTime lastSuccessfulRefreshUtc;

        bool operator==(const ObservableState& other) const
        {
            return snapshotAvailable == other.snapshotAvailable
                && refreshInFlight == other.refreshInFlight
                && quotaSuppressed == other.quotaSuppressed
                && authoritativeCount == other.authoritativeCount
                && sessionCalls == other.sessionCalls
                && sessionCallsAtSnapshot == other.sessionCallsAtSnapshot
                && providerDate == other.providerDate
                && lastSuccessfulRefreshUtc == other.lastSuccessfulRefreshUtc;
        }
    };

    BricksetUsagePolicy() = default;
    QDateTime nowUtc() const;
    bool snapshotFresh() const;
    int effectiveCount() const;
    ObservableState observableState() const;
    void notifyIfChanged(const ObservableState& previousState);
    void finishWaiting(Decision decision);

    bool m_snapshotAvailable = false;
    bool m_refreshInFlight = false;
    bool m_quotaSuppressed = false;
    int m_authoritativeCount = 0;
    int m_sessionCalls = 0;
    int m_sessionCallsAtSnapshot = 0;
    QDate m_providerDate;
    QDateTime m_lastSuccessfulRefreshUtc;
    QDateTime m_lastFailedRefreshUtc;
    QList<Completion> m_waiting;
    quint64 m_credentialGeneration = 1;
#ifdef BRICKSUITE_TESTING
    QDateTime m_testNowUtc;
#endif
};

Q_DECLARE_METATYPE(BricksetUsagePolicy::State)
