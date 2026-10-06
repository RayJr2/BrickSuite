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

#pragma once

#include <QAction>
#include <QMenu>

namespace ToolsMenuLayout
{
// Reorder the existing actions without replacing their connections or state.
inline void apply(QMenu* menu, QAction* importData, QAction* partReference,
                  QAction* viewer, QAction* calibration, QAction* database,
                  QAction* referenceData)
{
    const QList<QAction*> primary{importData, partReference, viewer, calibration, database, referenceData};
    QList<QAction*> diagnostics;
    for (auto* action : menu->actions()) {
        menu->removeAction(action);
        if (action->isSeparator()) delete action;
        else if (!primary.contains(action)) diagnostics.append(action);
    }
    menu->addAction(importData);
    menu->addSeparator();
    menu->addAction(partReference);
    menu->addSeparator();
    menu->addAction(viewer);
    menu->addAction(calibration);
    menu->addSeparator();
    menu->addAction(database);
    menu->addAction(referenceData);
    if (!diagnostics.isEmpty()) {
        menu->addSeparator();
        menu->addActions(diagnostics);
    }
}
}
