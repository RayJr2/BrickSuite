// Application.h

#pragma once

#include <memory>

class MainWindow;
class WorkspaceContext;

class Application
{
public:
    Application();
    ~Application();

    bool initialize();

private:
    std::unique_ptr<WorkspaceContext> m_workspaceContext;
    std::unique_ptr<MainWindow> m_mainWindow;
};