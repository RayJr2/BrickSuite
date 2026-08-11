#include <QApplication>

#include "app/Application.h"

int main(int argc, char *argv[])
{
    QApplication qtApplication(argc, argv);

    QApplication::setApplicationName("BrickSuite");
    QApplication::setApplicationVersion("1.0.0");
    QApplication::setOrganizationName("RF StateSide");

    Application application;

    if (!application.initialize())
        return EXIT_FAILURE;

    return qtApplication.exec();
}