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
#include "../services/storage/SessionStorageSelectionService.h"
#include "../ui/MainWindow.h"

#include <QDebug>
#include <QMessageBox>

Application::Application() = default;

Application::~Application() = default;

bool Application::initialize(const StartupProgress& progress)
{
    const auto report = [&progress](int stage, const QString& message) {
        if (progress) progress(stage, message);
    };
    report(1, QStringLiteral("Initializing database..."));
    if (!DatabaseManager::instance().initialize())
    {
        QMessageBox::critical(
            nullptr,
            "BrickSuite",
            "Unable to initialize the BrickSuite database.");

        return false;
    }

    QSqlDatabase database = DatabaseManager::instance().database();

    report(2, QStringLiteral("Initializing reference data..."));
    ReferenceDataSeeder seeder(database);

    if (!seeder.seedIfRequired()) {
        QMessageBox::critical(nullptr,
                              "BrickSuite",
                              "Unable to initialize BrickSuite reference data.");

        return false;
    }

    report(3, QStringLiteral("Preparing main window and catalog services..."));
    m_workspaceContext = std::make_unique<WorkspaceContext>();
    m_sessionStorageSelectionService = std::make_unique<SessionStorageSelectionService>();

    m_mainWindow = std::make_unique<MainWindow>(*m_workspaceContext,
                                                *m_sessionStorageSelectionService);
    report(4, QStringLiteral("Starting background services..."));
    m_automaticBackupService = std::make_unique<AutomaticBackupService>();
    m_mainWindow->setAutomaticBackupService(m_automaticBackupService.get());
    m_mainWindow->show();

    m_automaticBackupService->start();

    report(5, QStringLiteral("Ready"));
    return true;
}

QWidget* Application::mainWindow() const
{
    return m_mainWindow.get();
}
