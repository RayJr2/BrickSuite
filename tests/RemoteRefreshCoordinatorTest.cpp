#include "../src/services/application/RemoteRefreshCoordinator.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QThread>
#include <QDebug>

namespace {
bool check(bool condition, const char* message)
{
    if (!condition) qCritical().noquote() << message;
    return condition;
}

bool waitUntil(const std::function<bool()>& condition, int timeoutMs = 1000)
{
    QElapsedTimer timer;
    timer.start();
    while (!condition() && timer.elapsed() < timeoutMs) {
        QCoreApplication::processEvents();
        QThread::msleep(5);
    }
    return condition();
}

OperationalInvalidation event(OperationalInvalidationDomain domain, int workspace = 7)
{
    OperationalInvalidation value;
    value.sequence = 1;
    value.domains = {domain};
    if (domain != OperationalInvalidationDomain::Workspaces
        && domain != OperationalInvalidationDomain::PartReferenceCustomizations)
        value.workspaceId = workspace;
    return value;
}
}

int main(int argc, char* argv[])
{
    QCoreApplication application(argc, argv);
    using P = RemoteRefreshCoordinator::Projection;
    RemoteRefreshCoordinator coordinator;
    coordinator.resetContext(true, 1, 1);

    bool inventoryVisible = true;
    int inventoryStarts = 0;
    RemoteRefreshCoordinator::Completion inventoryCompletion;
    coordinator.registerProjection(P::Inventory, [&] { return inventoryVisible; },
        [&](const OperationalInvalidation&, auto completion) {
            ++inventoryStarts;
            inventoryCompletion = std::move(completion);
        });

    bool storageVisible = false;
    int storageStarts = 0;
    coordinator.registerProjection(P::Storage, [&] { return storageVisible; },
        [&](const OperationalInvalidation&, auto completion) {
            ++storageStarts;
            completion(true);
        });

    int locationStarts = 0;
    coordinator.registerProjection(P::InventoryLocations, [] { return true; },
        [&](const OperationalInvalidation&, auto completion) {
            ++locationStarts;
            completion(true);
        });

    bool ok = true;
    coordinator.receiveInvalidation(event(OperationalInvalidationDomain::Inventory));
    coordinator.receiveInvalidation(event(OperationalInvalidationDomain::Inventory));
    ok &= check(waitUntil([&] { return inventoryStarts == 1; }),
                "Repeated domains in one burst must coalesce to one refresh.");
    ok &= check(coordinator.isInFlight(P::Inventory),
                "Projection must remain in flight until its read completes.");

    coordinator.receiveInvalidation(event(OperationalInvalidationDomain::Pulling));
    coordinator.receiveInvalidation(event(OperationalInvalidationDomain::Builds));
    QCoreApplication::processEvents();
    QThread::msleep(200);
    QCoreApplication::processEvents();
    ok &= check(inventoryStarts == 1,
                "Invalidation during an active read must not start a duplicate read.");
    inventoryCompletion(true);
    ok &= check(waitUntil([&] { return inventoryStarts == 2; }),
                "Several in-flight invalidations must produce exactly one rerun.");
    inventoryCompletion(true);

    inventoryVisible = false;
    coordinator.receiveInvalidation(event(OperationalInvalidationDomain::Storage));
    ok &= check(waitUntil([&] { return locationStarts == 1; }),
                "Storage invalidation must refresh visible dependent location choices.");
    ok &= check(storageStarts == 0 && coordinator.isDirty(P::Storage),
                "A hidden Storage surface must remain dirty without loading.");
    storageVisible = true;
    coordinator.surfaceBecameRelevant(P::Storage);
    ok &= check(storageStarts == 1 && !coordinator.isDirty(P::Storage),
                "Showing a dirty surface must refresh it once.");

    coordinator.receiveInvalidation(event(OperationalInvalidationDomain::Inventory));
    coordinator.resetContext(true, 2, 2);
    QThread::msleep(200);
    QCoreApplication::processEvents();
    ok &= check(!coordinator.isDirty(P::Inventory) && inventoryStarts == 2,
                "Session/Workspace transition must discard a queued old plan.");

    coordinator.receiveInvalidation(event(OperationalInvalidationDomain::Inventory, 8));
    coordinator.resetContext(true, 2, 3);
    ok &= check(!coordinator.isDirty(P::Inventory),
                "Workspace transition must clear old Workspace dirty state.");

    // A completion captured before a context transition must not mutate the
    // new Host/Workspace refresh state.
    inventoryVisible = true;
    coordinator.receiveInvalidation(event(OperationalInvalidationDomain::Inventory, 8));
    ok &= check(waitUntil([&] { return inventoryStarts == 3; }),
                "A current-context refresh should start before transition.");
    const auto obsoleteCompletion = inventoryCompletion;
    coordinator.resetContext(true, 3, 4);
    obsoleteCompletion(true);
    ok &= check(!coordinator.isDirty(P::Inventory)
                    && !coordinator.isInFlight(P::Inventory)
                    && inventoryStarts == 3,
                "Obsolete in-flight completion must not affect the new context.");

    // Two authenticated Clients own independent coordinators. The same Host
    // burst must coalesce once in each without shared mutable refresh state.
    RemoteRefreshCoordinator secondClient;
    secondClient.resetContext(true, 1, 1);
    int secondClientStarts = 0;
    secondClient.registerProjection(P::Inventory, [] { return true; },
        [&](const OperationalInvalidation&, auto completion) {
            ++secondClientStarts;
            completion(true);
        });
    coordinator.receiveInvalidation(event(OperationalInvalidationDomain::Inventory));
    secondClient.receiveInvalidation(event(OperationalInvalidationDomain::Inventory));
    secondClient.receiveInvalidation(event(OperationalInvalidationDomain::Inventory));
    ok &= check(waitUntil([&] { return inventoryStarts == 4 && secondClientStarts == 1; }),
                "Each Client must coalesce and refresh independently.");
    inventoryCompletion(true);

    coordinator.setConnected(false);
    coordinator.receiveInvalidation(event(OperationalInvalidationDomain::Inventory));
    ok &= check(!coordinator.isDirty(P::Inventory),
                "Disconnected coordinator must not schedule Host reads.");

    return ok ? 0 : 1;
}
