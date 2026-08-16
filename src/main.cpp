#include <QApplication>
#include <QDebug>

#include "app/Application.h"
#include "settings/ThemeManager.h"
#include "services/Logger.h"

int main(int argc, char *argv[])
{
    QApplication qtApplication(argc, argv);

    QApplication::setApplicationName("BrickSuite");
    QApplication::setApplicationVersion("1.0.0");
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