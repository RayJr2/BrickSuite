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

#include <QApplication>
#include <QDebug>

#include "app/Application.h"
#include "core/AppVersion.h"
#include "services/Logger.h"
#include "settings/ThemeManager.h"

int main(int argc, char *argv[])
{
    QApplication qtApplication(argc, argv);

    QApplication::setApplicationName("BrickSuite");
    QApplication::setApplicationVersion(AppVersion::version());
    QApplication::setOrganizationName("RFStateSide");

    //
    // Install the application-wide Qt message handler after the Qt
    // application identity is set so AppLocalDataLocation resolves to the
    // BrickSuite data folder. Existing qDebug/qInfo/qWarning/qCritical calls
    // are captured automatically from this point forward.
    //
    Logger::init();

    qInfo() << "BrickSuite version:" << QApplication::applicationVersion();

    ThemeManager::applySavedTheme(qtApplication);

    Application application;

    if (!application.initialize()) {
        qCritical() << "BrickSuite application initialization failed.";
        Logger::shutdown();
        return EXIT_FAILURE;
    }

    const int result = qtApplication.exec();

    Logger::shutdown();

    return result;
}