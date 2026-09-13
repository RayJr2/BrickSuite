#include "../src/api/brickset/BricksetUsagePolicy.h"
#include "../src/api/brickset/BricksetService.h"

#include <QCoreApplication>
#include <QDebug>
#include <QTimeZone>

namespace {
bool require(bool condition, const char* message)
{
    if (!condition) qCritical() << message;
    return condition;
}
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    bool ok = true;
    const QDate day(2026, 9, 13);

    const QByteArray success = R"({"status":"success","matches":3,"apiKeyUsage":[
        {"dateStamp":"2026-09-11T00:00:00","count":7},
        {"dateStamp":"2026-09-13T00:00:00","count":20},
        {"dateStamp":"2026-09-12T00:00:00","count":9}]})";
    const auto parsed = BricksetService::parseKeyUsageResponse(success, day);
    ok &= require(parsed.success && parsed.todayCount == 20 && parsed.entries.size() == 3,
                  "Current-day usage and historical entries must parse.");
    const auto noToday = BricksetService::parseKeyUsageResponse(
        R"({"status":"success","matches":1,"apiKeyUsage":[{"dateStamp":"2026-09-12","count":9}]})", day);
    ok &= require(noToday.success && noToday.todayCount == 0,
                  "A successful response without today's entry means zero provider-reported calls.");
    ok &= require(!BricksetService::parseKeyUsageResponse("not-json", day).success,
                  "Malformed JSON must fail.");
    ok &= require(!BricksetService::parseKeyUsageResponse(
        R"({"status":"success","matches":1,"apiKeyUsage":[{"dateStamp":3,"count":"bad"}]})", day).success,
        "Malformed usage entries must fail.");
    const auto providerError = BricksetService::parseKeyUsageResponse(
        R"({"status":"error","message":"Invalid API key"})", day);
    ok &= require(!providerError.success && providerError.error.type == ApiErrorType::Authentication,
                  "Provider authentication errors must retain their classification.");

    auto& policy = BricksetUsagePolicy::instance();
    const QDateTime now(day, QTime(10, 0), QTimeZone::UTC);
    policy.resetForTesting();
    policy.setNowForTesting(now);
    int stateChanges = 0;
    const QMetaObject::Connection stateChangeConnection = QObject::connect(
        &policy, &BricksetUsagePolicy::stateChanged, [&stateChanges] { ++stateChanges; });
    int refreshes = 0;
    QList<BricksetUsagePolicy::Decision> decisions;
    auto completion = [&decisions](BricksetUsagePolicy::Decision value) { decisions.append(value); };
    policy.requestAdmission(80, completion, [&refreshes] { ++refreshes; });
    policy.requestAdmission(80, completion, [&refreshes] { ++refreshes; });
    ok &= require(refreshes == 1 && decisions.isEmpty(),
                  "Concurrent stale callers must share one authoritative refresh.");
    policy.completeRefresh(policy.credentialGeneration(), true, 20, day);
    ok &= require(stateChanges == 4,
                  "Refresh start/completion and both admitted calls must publish material changes.");
    ok &= require(decisions.size() == 2
                      && decisions[0] == BricksetUsagePolicy::Decision::Admitted
                      && decisions[1] == BricksetUsagePolicy::Decision::Admitted
                      && policy.state(80).effectiveCount == 22,
                  "Waiting calls must reserve once after refresh.");
    policy.requestAdmission(80, completion, [&refreshes] { ++refreshes; });
    ok &= require(refreshes == 1 && policy.state(80).effectiveCount == 23,
                  "Fresh snapshot plus three local calls must equal 23.");
    ok &= require(stateChanges == 5,
                  "Each admitted waiting/direct call must publish its effective-usage change.");

    int queuedPresentationUpdates = 0;
    QObject presentation;
    QObject::connect(&policy, &BricksetUsagePolicy::stateChanged, &presentation,
                     [&queuedPresentationUpdates] { ++queuedPresentationUpdates; },
                     Qt::QueuedConnection);
    policy.requestAdmission(80, completion, [] {});
    ok &= require(queuedPresentationUpdates == 0,
                  "Queued Settings-style delivery must not run inline.");
    QCoreApplication::processEvents();
    ok &= require(queuedPresentationUpdates == 1 && policy.state(80).effectiveCount == 24,
                  "An open Settings-style observer must receive live admitted-call state.");

    stateChanges = 0;
    policy.completeRefresh(policy.credentialGeneration(), true, 24, day);
    stateChanges = 0;
    policy.completeRefresh(policy.credentialGeneration(), true, 24, day);
    ok &= require(stateChanges == 0,
                  "Repeating an externally identical policy state must not emit duplicate churn.");

    policy.requestRefresh({}, [&refreshes] { ++refreshes; }, true);
    policy.completeRefresh(policy.credentialGeneration(), true, 23, day);
    ok &= require(policy.state(80).effectiveCount == 23,
                  "A new authoritative count must reconcile the local baseline without double-counting.");
    policy.setNowForTesting(now.addMSecs(BricksetUsagePolicy::SuccessfulSnapshotTtlMs - 1));
    policy.requestAdmission(80, completion, [&refreshes] { ++refreshes; });
    ok &= require(refreshes == 2,
                  "A successful snapshot must be reused within the fifteen-minute TTL.");
    policy.setNowForTesting(now.addMSecs(BricksetUsagePolicy::SuccessfulSnapshotTtlMs + 1));
    policy.requestAdmission(80, completion, [&refreshes] { ++refreshes; });
    ok &= require(refreshes == 3,
                  "The next charged request after TTL expiry must refresh usage first.");
    policy.completeRefresh(policy.credentialGeneration(), true, 24, day);

    policy.resetForTesting(); policy.setNowForTesting(now);
    policy.requestRefresh({}, [] {}, true);
    policy.completeRefresh(policy.credentialGeneration(), true, 78, day);
    decisions.clear();
    policy.requestAdmission(80, completion, [] {});
    policy.requestAdmission(80, completion, [] {});
    policy.requestAdmission(80, completion, [] {});
    ok &= require(decisions.size() == 3
                      && decisions[0] == BricksetUsagePolicy::Decision::Admitted
                      && decisions[1] == BricksetUsagePolicy::Decision::Admitted
                      && decisions[2] == BricksetUsagePolicy::Decision::ThresholdReached
                      && policy.state(80).effectiveCount == 80,
                  "Atomic reservations must not oversubscribe the threshold.");

    policy.resetForTesting(); policy.setNowForTesting(now);
    refreshes = 0; decisions.clear();
    policy.requestAdmission(80, completion, [&refreshes] { ++refreshes; });
    policy.completeRefresh(policy.credentialGeneration(), false, 0, {});
    policy.requestAdmission(80, completion, [&refreshes] { ++refreshes; });
    ok &= require(refreshes == 1 && decisions.size() == 2
                      && decisions[0] == BricksetUsagePolicy::Decision::UsageUnavailable
                      && decisions[1] == BricksetUsagePolicy::Decision::UsageUnavailable,
                  "Failed refresh must suppress charged calls and enforce cooldown.");
    policy.setNowForTesting(now.addMSecs(BricksetUsagePolicy::FailureCooldownMs + 1));
    policy.requestAdmission(80, completion, [&refreshes] { ++refreshes; });
    ok &= require(refreshes == 2, "A later request must be able to recover after cooldown.");
    policy.completeRefresh(policy.credentialGeneration(), true, 5, day);

    policy.noteQuotaResponse(); decisions.clear(); refreshes = 0;
    policy.requestAdmission(80, completion, [&refreshes] { ++refreshes; });
    ok &= require(refreshes == 0 && decisions.value(0) == BricksetUsagePolicy::Decision::UsageUnavailable,
                  "Quota response must suppress immediate charged retries.");

    const quint64 oldGeneration = policy.credentialGeneration();
    policy.invalidateForCredentialChange();
    policy.completeRefresh(oldGeneration, true, 77, day);
    ok &= require(policy.state(80).effectiveCount == -1 && policy.sessionCallCount() == 0,
                  "Credential change must clear state and reject an old key's late response.");
    policy.setNowForTesting(now);
    policy.requestRefresh({}, [] {}, true);
    policy.completeRefresh(policy.credentialGeneration(), true, 4, day);
    policy.setNowForTesting(now.addDays(1));
    ok &= require(policy.state(80).effectiveCount == -1,
                  "UTC day rollover must not reuse the prior day's usage.");

    const int callsBeforeInstructions = policy.sessionCallCount();
    BricksetService instructionsService;
    instructionsService.getInstructions2({}, {});
    ok &= require(policy.sessionCallCount() == callsBeforeInstructions,
                  "Instruction requests must not consume or consult getSets admission.");

    policy.resetForTesting();
    QObject::disconnect(stateChangeConnection);
    return ok ? 0 : 1;
}
