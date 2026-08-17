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

#include "UserSettings.h"

#include <QString>

class QApplication;

class ThemeManager
{
public:
    static void applyTheme(QApplication& application, UserSettings::Theme theme);

    static void applySavedTheme(QApplication& application);

private:
    static void applySystemTheme(QApplication& application);

    static void applyLightTheme(QApplication& application);

    static void applyDarkTheme(QApplication& application);

    static QString globalStyleSheet(UserSettings::Theme theme);
};