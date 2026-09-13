#include "../src/services/application/RebrickableCoordinationService.h"
#include "../src/api/ApiProviderStatusRegistry.h"

#include <QCoreApplication>
#include <QDebug>
#include <limits>

namespace { bool check(bool ok, const char* text) { if (!ok) qCritical() << text; return ok; } }

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    using C = RebrickableCoordinationService;
    bool ok = true;
    ok &= check(C::MinimumProtocolMinor == 4,
                "Rebrickable coordination remains a Protocol 1.4 capability");
    ok &= check(C::coordinatedIntervalForParticipants(0) == 0, "zero participants");
    ok &= check(C::coordinatedIntervalForParticipants(1) == 1250, "one participant");
    ok &= check(C::coordinatedIntervalForParticipants(2) == 2250, "two participants");
    ok &= check(C::coordinatedIntervalForParticipants(3) == 3250, "three participants");
    ok &= check(C::coordinatedIntervalForParticipants(4) == 4250, "four participants");
    ok &= check(C::effectiveInterval(1250, 3250) == 3250, "coordination raises local default");
    ok &= check(C::effectiveInterval(2000, 3250) == 3250, "coordination remains stricter");
    ok &= check(C::effectiveInterval(5000, 3250) == 5000, "local value remains stricter");
    ok &= check(C::localIntervalEditable(0), "zero participants permits local editing");
    ok &= check(C::localIntervalEditable(1), "one participant permits local editing");
    ok &= check(!C::localIntervalEditable(2), "two participants lock local editing");
    ok &= check(!C::localIntervalEditable(3), "three participants lock local editing");

    const int configuredIntervalMs = 2000;
    ok &= check(!C::localIntervalEditable(2) && configuredIntervalMs == 2000,
                "one-to-two transition locks without changing configured value");
    ok &= check(C::localIntervalEditable(1) && configuredIntervalMs == 2000,
                "two-to-one transition unlocks without changing configured value");
    ok &= check(C::effectiveInterval(configuredIntervalMs,
                                     C::coordinatedIntervalForParticipants(3)) == 3250,
                "configured 2000 ms value remains coordinated at three participants");
    const int stricterLocalIntervalMs = 5000;
    ok &= check(!C::localIntervalEditable(3)
                    && C::effectiveInterval(stricterLocalIntervalMs,
                                            C::coordinatedIntervalForParticipants(3)) == 5000,
                "stricter local value remains effective while editing is locked");
    ok &= check(C::coordinatedIntervalForParticipants(std::numeric_limits<int>::max())
                    == std::numeric_limits<int>::max(), "arithmetic is bounded");

    auto& providerStatus = ApiProviderStatusRegistry::instance();
    providerStatus.setStatus(ApiProvider::Rebrickable, ApiConnectionStatus::Unknown);
    int providerChanges = 0;
    QObject::connect(&providerStatus, &ApiProviderStatusRegistry::statusChanged,
                     &app, [&providerChanges](ApiProvider, ApiConnectionStatus) {
        ++providerChanges;
    });
    providerStatus.setStatus(ApiProvider::Rebrickable, ApiConnectionStatus::Connected);
    ok &= check(providerStatus.isConnected(ApiProvider::Rebrickable),
                "successful provider transition enables participation");
    ok &= check(providerChanges == 1,
                "available transition emits one participation notification");
    providerStatus.setStatus(ApiProvider::Rebrickable, ApiConnectionStatus::Connected);
    ok &= check(providerChanges == 1,
                "repeated available status is idempotent");
    providerStatus.setStatus(ApiProvider::Rebrickable, ApiConnectionStatus::AuthenticationFailed);
    ok &= check(!providerStatus.isConnected(ApiProvider::Rebrickable),
                "unavailable provider transition disables participation");
    ok &= check(providerChanges == 2,
                "unavailable transition emits one participation notification");
    providerStatus.setStatus(ApiProvider::Rebrickable, ApiConnectionStatus::AuthenticationFailed);
    ok &= check(providerChanges == 2,
                "repeated unavailable status is idempotent");
    return ok ? 0 : 1;
}
