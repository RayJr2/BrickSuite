#include "../src/ui/inventory/MyInventoryAddSessionRefresh.h"

#include <QCoreApplication>
#include <QObject>

#include <cstdio>

namespace {

bool require(bool condition, const char* message)
{
    if (!condition)
        std::fprintf(stderr, "%s\n", message);
    return condition;
}

void processQueuedRefresh()
{
    QCoreApplication::sendPostedEvents(nullptr, 0);
    QCoreApplication::processEvents();
}

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    bool ok = true;

    QObject context;
    int presentationRefreshes = 0;
    int sessionNotifications = 0;
    QString activeFilter = QStringLiteral("Storage A");
    QString evaluatedFilter;
    MyInventoryAddSessionRefresh refresh(
        &context,
        [&] {
            ++presentationRefreshes;
            evaluatedFilter = activeFilter;
        },
        [&] { ++sessionNotifications; });

    refresh.inventoryAdded();
    ok &= require(presentationRefreshes == 0,
                  "presentation refresh was not queued");
    activeFilter = QStringLiteral("Storage B");
    processQueuedRefresh();
    ok &= require(presentationRefreshes == 1,
                  "successful Add did not refresh the open presentation");
    ok &= require(evaluatedFilter == QStringLiteral("Storage B"),
                  "queued refresh used stale captured filter state");
    ok &= require(sessionNotifications == 0,
                  "expensive session notification ran after an individual Add");

    refresh.inventoryAdded();
    refresh.inventoryAdded();
    refresh.inventoryAdded();
    processQueuedRefresh();
    ok &= require(presentationRefreshes == 2,
                  "rapid Add refresh requests were not coalesced");
    ok &= require(sessionNotifications == 0,
                  "rapid Adds published session changes early");

    refresh.inventoryAdded();
    refresh.sessionFinished();
    ok &= require(presentationRefreshes == 3,
                  "session completion did not flush the final pending refresh");
    ok &= require(sessionNotifications == 1,
                  "session completion did not publish exactly one change");
    processQueuedRefresh();
    ok &= require(presentationRefreshes == 3,
                  "queued callback duplicated the close-time refresh");
    refresh.sessionFinished();
    ok &= require(sessionNotifications == 1,
                  "unchanged session published a duplicate notification");

    int destroyedContextRefreshes = 0;
    auto* transientContext = new QObject;
    auto* transientRefresh = new MyInventoryAddSessionRefresh(
        transientContext,
        [&] { ++destroyedContextRefreshes; },
        [] {});
    transientRefresh->inventoryAdded();
    delete transientContext;
    delete transientRefresh;
    processQueuedRefresh();
    ok &= require(destroyedContextRefreshes == 0,
                  "queued refresh outlived its QObject context");

    return ok ? 0 : 1;
}
