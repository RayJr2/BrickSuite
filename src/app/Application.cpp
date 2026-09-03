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

// Application.cpp

#include "Application.h"
#include "WorkspaceContext.h"

#include "../database/DatabaseManager.h"
#include "../services/ReferenceDataSeeder.h"
#include "../services/database/AutomaticBackupService.h"
#include "../ui/MainWindow.h"

#include <QDebug>
#include <QMessageBox>

Application::Application() = default;

Application::~Application() = default;

bool Application::initialize()
{
    if (!DatabaseManager::instance().initialize())
    {
        QMessageBox::critical(
            nullptr,
            "BrickSuite",
            "Unable to initialize the BrickSuite database.");

        return false;
    }

    QSqlDatabase database = DatabaseManager::instance().database();

    ReferenceDataSeeder seeder(database);

    if (!seeder.seedIfRequired()) {
        QMessageBox::critical(nullptr,
                              "BrickSuite",
                              "Unable to initialize BrickSuite reference data.");

        return false;
    }

    m_workspaceContext = std::make_unique<WorkspaceContext>();

    m_mainWindow = std::make_unique<MainWindow>(*m_workspaceContext);
    m_automaticBackupService = std::make_unique<AutomaticBackupService>();
    m_mainWindow->setAutomaticBackupService(m_automaticBackupService.get());
    m_mainWindow->show();

    m_automaticBackupService->start();

    return true;
}

QWidget* Application::mainWindow() const
{
    return m_mainWindow.get();
}
