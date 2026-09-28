// Disposable GUI acceptance entry point; never included in release builds.
#include "src/ui/parts/PartViewerSurfaceFormat.h"
#include "src/ui/parts/LDrawModelViewerWindow.h"
#include "src/ui/parts/PrintPreparationCoordinator.h"
#include "src/settings/UserSettings.h"
#include "src/database/DatabaseSchema.h"
#include <QApplication>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>

int main(int argc, char** argv)
{
    configurePartViewerSurfaceFormat();
    QApplication app(argc, argv);
    if (argc != 2) return 2;
    app.setOrganizationName(QStringLiteral("BrickSuiteViewerAcceptance"));
    app.setApplicationName(QStringLiteral("BrickSuiteViewerAcceptance"));
    QTemporaryDir settings;
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings.path());
    QStandardPaths::setTestModeEnabled(true);
    auto database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"));
    database.setDatabaseName(QStringLiteral(":memory:"));
    if (!database.open() || !DatabaseSchema::initialize(database)) return 1;
    UserSettings::instance().setLDrawLibraryPath(QString::fromLocal8Bit(argv[1]));
    PrintPreparationCoordinator coordinator;
    LDrawModelViewerWindow viewer(&coordinator);
    LDrawModelViewerRequest request;
    request.partNumber = QStringLiteral("3001");
    request.partName = QStringLiteral("Brick 2 x 4");
    request.candidates = {QStringLiteral("3001")};
    viewer.showPart(request);
    viewer.show();
    return app.exec();
}
