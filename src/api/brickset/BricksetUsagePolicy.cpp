#include "BricksetUsagePolicy.h"

#include <QDebug>
#include <utility>

BricksetUsagePolicy& BricksetUsagePolicy::instance()
{
    static BricksetUsagePolicy policy;
    return policy;
}

QDateTime BricksetUsagePolicy::nowUtc() const
{
#ifdef BRICKSUITE_TESTING
    if (m_testNowUtc.isValid()) return m_testNowUtc;
#endif
    return QDateTime::currentDateTimeUtc();
}

bool BricksetUsagePolicy::snapshotFresh() const
{
    const QDateTime now = nowUtc();
    return m_snapshotAvailable && m_providerDate == now.date()
        && m_lastSuccessfulRefreshUtc.isValid()
        && m_lastSuccessfulRefreshUtc.msecsTo(now) <= SuccessfulSnapshotTtlMs;
}

int BricksetUsagePolicy::effectiveCount() const
{
    if (!m_snapshotAvailable || m_providerDate != nowUtc().date()) return -1;
    return m_authoritativeCount + qMax(0, m_sessionCalls - m_sessionCallsAtSnapshot);
}

BricksetUsagePolicy::ObservableState BricksetUsagePolicy::observableState() const
{
    return {m_snapshotAvailable, m_refreshInFlight, m_quotaSuppressed,
            m_authoritativeCount, m_sessionCalls, m_sessionCallsAtSnapshot,
            m_providerDate, m_lastSuccessfulRefreshUtc};
}

void BricksetUsagePolicy::notifyIfChanged(const ObservableState& previousState)
{
    if (!(observableState() == previousState)) emit stateChanged();
}

BricksetUsagePolicy::State BricksetUsagePolicy::state(int threshold) const
{
    State result;
    result.snapshotAvailable = snapshotFresh() && !m_quotaSuppressed;
    result.authoritativeCount = result.snapshotAvailable ? m_authoritativeCount : 0;
    result.effectiveCount = result.snapshotAvailable ? effectiveCount() : -1;
    result.threshold = threshold;
    result.safeCallsRemaining = result.effectiveCount < 0
        ? 0 : qMax(0, threshold - result.effectiveCount);
    result.providerDate = result.snapshotAvailable ? m_providerDate : QDate();
    result.lastSuccessfulRefreshUtc = m_lastSuccessfulRefreshUtc;
    if (m_refreshInFlight) result.status = Status::Refreshing;
    else if (m_quotaSuppressed || !result.snapshotAvailable) result.status = Status::Unavailable;
    else if (result.effectiveCount >= threshold) result.status = Status::ThresholdReached;
    else result.status = Status::Available;
    return result;
}

void BricksetUsagePolicy::requestAdmission(
    int threshold, Completion completion, RefreshStarter startRefresh)
{
    if (snapshotFresh() && !m_quotaSuppressed) {
        if (effectiveCount() >= threshold) {
            if (completion) completion(Decision::ThresholdReached);
            return;
        }
        const ObservableState previousState = observableState();
        ++m_sessionCalls; // Reserve at admission so concurrent callers cannot oversubscribe.
        notifyIfChanged(previousState);
        if (completion) completion(Decision::Admitted);
        return;
    }

    m_waiting.append([this, threshold, completion = std::move(completion)](Decision refreshDecision) {
        if (refreshDecision != Decision::Admitted) {
            if (completion) completion(Decision::UsageUnavailable);
            return;
        }
        if (effectiveCount() >= threshold) {
            if (completion) completion(Decision::ThresholdReached);
            return;
        }
        const ObservableState previousState = observableState();
        ++m_sessionCalls;
        notifyIfChanged(previousState);
        if (completion) completion(Decision::Admitted);
    });
    requestRefresh({}, std::move(startRefresh), false);
}

void BricksetUsagePolicy::requestRefresh(
    Completion completion, RefreshStarter startRefresh, bool bypassCooldown)
{
    if (completion) m_waiting.append(std::move(completion));
    if (m_refreshInFlight) return;
    const QDateTime now = nowUtc();
    if (!bypassCooldown && m_lastFailedRefreshUtc.isValid()
        && m_lastFailedRefreshUtc.msecsTo(now) < FailureCooldownMs) {
        finishWaiting(Decision::UsageUnavailable);
        return;
    }
    const ObservableState previousState = observableState();
    m_refreshInFlight = true;
    notifyIfChanged(previousState);
    if (startRefresh) startRefresh();
    else completeRefresh(m_credentialGeneration, false, 0, {});
}

void BricksetUsagePolicy::completeRefresh(
    quint64 credentialGeneration, bool success,
    int authoritativeCount, const QDate& providerDate)
{
    if (credentialGeneration != m_credentialGeneration) return;
    const ObservableState previousState = observableState();
    m_refreshInFlight = false;
    if (success && authoritativeCount >= 0 && providerDate == nowUtc().date()) {
        const bool recovered = m_lastFailedRefreshUtc.isValid() || m_quotaSuppressed;
        m_snapshotAvailable = true;
        m_quotaSuppressed = false;
        m_authoritativeCount = authoritativeCount;
        m_providerDate = providerDate;
        m_sessionCallsAtSnapshot = m_sessionCalls;
        m_lastSuccessfulRefreshUtc = nowUtc();
        m_lastFailedRefreshUtc = {};
        qInfo() << "Brickset authoritative getSets usage refreshed; count"
                << authoritativeCount << "date" << providerDate.toString(Qt::ISODate);
        if (recovered) qInfo() << "Brickset getSets usage verification recovered.";
        notifyIfChanged(previousState);
        finishWaiting(Decision::Admitted);
        return;
    }
    const bool retainedFreshSnapshot = snapshotFresh() && !m_quotaSuppressed;
    if (!retainedFreshSnapshot) m_snapshotAvailable = false;
    m_lastFailedRefreshUtc = nowUtc();
    qWarning() << (retainedFreshSnapshot
        ? "Brickset usage refresh failed; retaining the still-fresh authoritative snapshot."
        : "Brickset getSets usage is unavailable; charged request suppressed.");
    notifyIfChanged(previousState);
    finishWaiting(retainedFreshSnapshot ? Decision::Admitted : Decision::UsageUnavailable);
}

void BricksetUsagePolicy::finishWaiting(Decision decision)
{
    const auto waiting = std::exchange(m_waiting, {});
    for (const auto& completion : waiting) if (completion) completion(decision);
}

void BricksetUsagePolicy::invalidateForCredentialChange()
{
    const ObservableState previousState = observableState();
    ++m_credentialGeneration;
    m_refreshInFlight = false;
    finishWaiting(Decision::UsageUnavailable);
    m_snapshotAvailable = false;
    m_quotaSuppressed = false;
    m_authoritativeCount = 0;
    m_providerDate = {};
    m_sessionCalls = 0;
    m_sessionCallsAtSnapshot = 0;
    m_lastSuccessfulRefreshUtc = {};
    m_lastFailedRefreshUtc = {};
    notifyIfChanged(previousState);
}

void BricksetUsagePolicy::noteQuotaResponse()
{
    const ObservableState previousState = observableState();
    m_snapshotAvailable = false;
    m_quotaSuppressed = true;
    m_lastFailedRefreshUtc = nowUtc();
    qWarning() << "Brickset getSets quota response invalidated usage state.";
    notifyIfChanged(previousState);
}

#ifdef BRICKSUITE_TESTING
void BricksetUsagePolicy::resetForTesting()
{
    const ObservableState previousState = observableState();
    m_snapshotAvailable=false; m_refreshInFlight=false; m_quotaSuppressed=false;
    m_authoritativeCount=0; m_sessionCalls=0; m_sessionCallsAtSnapshot=0;
    m_providerDate={}; m_lastSuccessfulRefreshUtc={}; m_lastFailedRefreshUtc={};
    m_waiting.clear(); m_credentialGeneration=1; m_testNowUtc={};
    notifyIfChanged(previousState);
}
void BricksetUsagePolicy::setNowForTesting(const QDateTime& value) { m_testNowUtc=value.toUTC(); }
#endif
