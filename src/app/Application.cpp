// Application.cpp

#include "Application.h"
#include "WorkspaceContext.h"

#include "../database/DatabaseManager.h"
#include "../services/ReferenceDataSeeder.h"
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
    m_mainWindow->show();

    return true;
}