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

#include "../src/ui/parts/PartReferenceCardRegistry.h"

#include <QApplication>
#include <QPointer>
#include <QToolButton>
#include <QWidget>

#include <cstdio>

namespace {

bool check(bool value, const char* message)
{
    if (!value)
        std::fprintf(stderr, "%s\n", message);
    return value;
}

} // namespace

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    bool ok = true;

    // Rebuilding a page deletes its cards. Registry lookups must retain only
    // guarded null values and must never dereference the deleted widgets.
    PartReferenceCardRegistry registry;
    auto* page = new QWidget;
    auto* first = new QToolButton(page);
    auto* second = new QToolButton(page);
    registry.add(QStringLiteral("3001"), first);
    registry.add(QStringLiteral("3001"), second);
    ok &= check(registry.contains(QStringLiteral("3001")),
                "populated registry did not report live cards");

    delete page;
    ok &= check(!registry.contains(QStringLiteral("3001")),
                "deleted page left a live card in the registry");
    ok &= check(registry.cards(QStringLiteral("3001")).isEmpty(),
                "deleted cards remained visible through the registry");

    // A late image-ready-style lookup after a rebuild must safely observe no
    // live cards. Clearing the registry remains safe before QObject children
    // are destroyed during dialog teardown.
    auto* teardownPage = new QWidget;
    auto* teardownCard = new QToolButton(teardownPage);
    registry.add(QStringLiteral("64647"), teardownCard);
    registry.clear();
    delete teardownPage;
    ok &= check(!registry.contains(QStringLiteral("64647")),
                "cleared registry retained teardown card state");

    return ok ? 0 : 1;
}
