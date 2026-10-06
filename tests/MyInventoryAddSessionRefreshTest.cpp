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
