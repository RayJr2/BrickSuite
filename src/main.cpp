#include <QApplication>

#include "app/Application.h"
#include "settings/ThemeManager.h"

int main(int argc, char *argv[])
{
    QApplication qtApplication(argc, argv);

    QApplication::setApplicationName("BrickSuite");
    QApplication::setApplicationVersion("1.0.0");
    QApplication::setOrganizationName("RFStateSide");

    ThemeManager::applySavedTheme(qtApplication);

    Application application;

    if (!application.initialize())
        return EXIT_FAILURE;

    return qtApplication.exec();
}