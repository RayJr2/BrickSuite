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