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

#include "../src/ui/inventory/EditInventorySaveState.h"

#include <QCoreApplication>
#include <QDebug>

namespace {
bool require(bool condition, const char* message)
{
    if (!condition)
        qCritical() << message;
    return condition;
}
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    bool ok = true;
    ok &= require(!EditInventorySaveState::canSave(false, false, 3),
                  "An unloaded Inventory record was saveable.");
    ok &= require(!EditInventorySaveState::canSave(true, true, 3),
                  "Save remained enabled while known Colors were loading.");
    ok &= require(!EditInventorySaveState::canSave(true, true, 0),
                  "The loading placeholder was saveable.");
    ok &= require(!EditInventorySaveState::canSave(true, false, 0),
                  "An invalid loaded Color was saveable.");
    ok &= require(EditInventorySaveState::canSave(true, false, 3),
                  "A valid original Color did not enable Save after success.");
    ok &= require(EditInventorySaveState::canSave(true, false, 179),
                  "A valid fallback Color did not enable Save after failure.");

    if (!ok)
        return 1;

    qInfo() << "Edit Inventory Save-state validation passed.";
    return 0;
}
