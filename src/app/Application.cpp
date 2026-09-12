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
#include "../network/HostDataEpoch.h"
#include "../services/database/DatabaseRestoreTransaction.h"
#include "../services/database/DatabaseRestoreCoordinator.h"
#include "../services/ReferenceDataSeeder.h"
#include "../services/database/AutomaticBackupService.h"
#include "../services/storage/SessionStorageSelectionService.h"
#include "../services/application/ApplicationServices.h"
#include "../settings/UserSettings.h"
#include "../ui/MainWindow.h"
#include "../network/BrickSuiteNetworkManager.h"

#include <QDebug>
#include <QElapsedTimer>
#include <QFile>
#include <QMessageBox>

Application::Application() = default;

Application::~Application() = default;

bool Application::initialize(const StartupProgress& progress)
{
    QElapsedTimer initializationTimer;
    QElapsedTimer phaseTimer;
    initializationTimer.start();
    phaseTimer.start();
    const auto report = [&progress](int stage, const QString& message) {
        if (progress) progress(stage, message);
    };
    report(1, QStringLiteral("Initializing database..."));
    const QString epochDirectory = HostDataEpoch::storageDirectory();
    if (QFile::exists(DatabaseRestoreTransaction::journalPath(epochDirectory))) {
        const auto recovery = DatabaseRestoreTransaction::recoverAtStartup(
            DatabaseManager::instance().databasePath(), epochDirectory, epochDirectory);
        if (!recovery.success) {
            QMessageBox::critical(nullptr, "BrickSuite Database Recovery",
                                  QStringLiteral("BrickSuite cannot safely open the database because an interrupted Restore could not be resolved.\n\n%1")
                                      .arg(recovery.error));
            return false;
        }
    }
    const HostDataEpoch::LoadResult epoch = HostDataEpoch::loadOrBootstrap(epochDirectory);
    if (!epoch.success) {
        QMessageBox::critical(nullptr, "BrickSuite",
                              QStringLiteral("Unable to establish the Host database identity.\n\n%1")
                                  .arg(epoch.error));
        return false;
    }
    if (!DatabaseManager::instance().initialize())
    {
        QMessageBox::critical(
            nullptr,
            "BrickSuite",
            "Unable to initialize the BrickSuite database.");

        return false;
    }
    qDebug() << "Startup phase database initialization completed in"
            << phaseTimer.elapsed() << "ms.";

    QSqlDatabase database = DatabaseManager::instance().database();

    report(2, QStringLiteral("Initializing reference data..."));
    phaseTimer.restart();
    ReferenceDataSeeder seeder(database);

    if (!seeder.seedIfRequired()) {
        QMessageBox::critical(nullptr,
                              "BrickSuite",
                              "Unable to initialize BrickSuite reference data.");

        return false;
    }
    qDebug() << "Startup phase reference data initialization completed in"
            << phaseTimer.elapsed() << "ms.";

    report(3, QStringLiteral("Preparing main window and catalog services..."));
    phaseTimer.restart();
    m_workspaceContext = std::make_unique<WorkspaceContext>();
    m_sessionStorageSelectionService = std::make_unique<SessionStorageSelectionService>();
    m_networkManager = std::make_unique<BrickSuiteNetworkManager>();
    const SharedDataSource sharedDataSource = UserSettings::instance().sharedDataSource();
    m_applicationServices = sharedDataSource == SharedDataSource::ThisComputer
        ? std::make_unique<ApplicationServices>()
        : createUnavailableHostApplicationServices();
    if (sharedDataSource == SharedDataSource::BrickSuiteHost)
        m_applicationServices->setRemoteReads(m_networkManager->remoteReads());
    qInfo().noquote() << "Shared data source:"
                      << (sharedDataSource == SharedDataSource::ThisComputer
                              ? "This Computer" : "BrickSuite Host");
    if (!m_applicationServices->sharedStatus().isAvailable())
        qInfo().noquote() << "BrickSuite Host shared services will become available after authentication.";
    qDebug() << "Startup phase application service creation completed in"
            << phaseTimer.elapsed() << "ms.";

    phaseTimer.restart();
    m_mainWindow = std::make_unique<MainWindow>(*m_workspaceContext,
                                                *m_sessionStorageSelectionService,
                                                *m_applicationServices,
                                                *m_networkManager);
    qDebug() << "Startup phase MainWindow construction completed in"
            << phaseTimer.elapsed() << "ms.";
    report(4, QStringLiteral("Starting background services..."));
    phaseTimer.restart();
    m_automaticBackupService = std::make_unique<AutomaticBackupService>();
    m_mainWindow->setAutomaticBackupService(m_automaticBackupService.get());
    m_databaseRestoreCoordinator = std::make_unique<DatabaseRestoreCoordinator>(
        *m_networkManager, *m_automaticBackupService);
    m_mainWindow->setDatabaseRestoreCoordinator(m_databaseRestoreCoordinator.get());
    m_mainWindow->show();

    m_automaticBackupService->start();
    m_networkManager->startConfiguredMode();
    qDebug() << "Startup phase background services and first show completed in"
            << phaseTimer.elapsed() << "ms.";

    report(5, QStringLiteral("Ready"));
    qDebug() << "Application initialization phases completed in"
            << initializationTimer.elapsed() << "ms.";
    return true;
}

QWidget* Application::mainWindow() const
{
    return m_mainWindow.get();
}
