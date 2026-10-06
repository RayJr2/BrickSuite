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

#include <QTimer>
#include <QWidget>

inline void requestInitialWindowActivation(QWidget* window)
{
#ifdef Q_OS_MACOS
    // The splash can consume launch activation before the main window exists.
    // Cocoa's raise() activates the app; activateWindow() gives keyboard focus.
    // Queue once after the splash finishes, never on show or refresh events.
    QTimer::singleShot(0, window, [window] {
        if (window->isVisible() && window->windowHandle()) {
            window->raise();
            window->activateWindow();
        }
    });
#else
    Q_UNUSED(window);
#endif
}
