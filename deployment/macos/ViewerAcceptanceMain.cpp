// Disposable GUI acceptance entry point; never included in release builds.
#include "src/ui/parts/PartViewerSurfaceFormat.h"
#include "src/ui/parts/LDrawModelViewerWindow.h"
#include "src/ui/parts/PrintPreparationCoordinator.h"
#include "src/settings/UserSettings.h"
#include "src/database/DatabaseSchema.h"
#include "src/ui/parts/LDrawViewportWidget.h"
#include <QApplication>
#include <QDebug>
#include <QElapsedTimer>
#include <QLabel>
#include <QOpenGLContext>
#include <QPushButton>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTimer>
#include <QEventLoop>
#include <functional>

namespace {
bool waitFor(const std::function<bool()>& predicate, int timeout = 15000)
{
    QElapsedTimer timer;
    timer.start();
    while (!predicate() && timer.elapsed() < timeout) {
        QEventLoop loop;
        QTimer::singleShot(50, &loop, &QEventLoop::quit);
        loop.exec();
    }
    return predicate();
}
}

int main(int argc, char** argv)
{
    configurePartViewerSurfaceFormat();
    QApplication app(argc, argv);
    if (argc < 2) return 2;
    const auto arguments = app.arguments();
    const bool smoke = arguments.contains(QStringLiteral("--smoke"));
    const int external = arguments.indexOf(QStringLiteral("--external"));
    const int part = arguments.indexOf(QStringLiteral("--part"));
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
    if (UserSettings::instance().ldrawLibraryPath() != QString::fromLocal8Bit(argv[1])) return 1;
    PrintPreparationCoordinator coordinator;
    LDrawModelViewerWindow viewer(&coordinator);
    LDrawModelViewerRequest request;
    request.partNumber = part >= 0 && part + 1 < arguments.size() ? arguments[part + 1] : QStringLiteral("3001");
    request.partName = request.partNumber == QStringLiteral("3001") ? QStringLiteral("Brick 2 x 4") : request.partNumber;
    request.candidates = {request.partNumber};
    if (external >= 0 && external + 1 < arguments.size()) request.externalFilePath = arguments[external + 1];
    viewer.showPart(request);
    viewer.show();
    if (smoke) {
        auto* viewport = viewer.findChild<LDrawViewportWidget*>();
        QPushButton* prepare = nullptr;
        for (auto* button : viewer.findChildren<QPushButton*>())
            if (button->text() == QStringLiteral("Prepare for Printing")) prepare = button;
        if (!viewport || !prepare || !waitFor([&] { return prepare->isEnabled(); })) return 1;
        viewport->setShowAxes(false);
        if (!waitFor([&] { return viewport->renderingAvailable(); })) return 1;
        const QImage frame = viewport->grabFramebuffer();
        int visible = 0;
        for (int y = 0; y < frame.height(); ++y)
            for (int x = 0; x < frame.width(); ++x)
                visible += frame.pixelColor(x, y).lightness() > 60;
        const bool rendered = visible > 1000 && viewport->context() && viewport->context()->shareContext();
        qInfo() << "Native source viewport:" << rendered << "visible pixels:" << visible
                << "part:" << request.partNumber << "external:" << !request.externalFilePath.isEmpty();
        if (!rendered) return 1;
        viewport->setRenderMode(PartViewerRenderMode::Wireframe);
        const QImage edges = viewport->grabFramebuffer();
        if (edges.isNull() || edges == frame) return 1;
        viewport->setRenderMode(PartViewerRenderMode::SolidEdges);
        viewport->setShowAxes(true);
        const QImage axes = viewport->grabFramebuffer();
        if (axes.isNull() || axes == frame) return 1;
        qInfo() << "Native edges/axes change the rendered frame: PASS";
        if (arguments.contains(QStringLiteral("--prepare"))) {
            prepare->click();
            QString result;
            const bool finished = waitFor([&] {
                for (auto* label : viewer.findChildren<QLabel*>()) {
                    if (label->text().startsWith(QStringLiteral("Prepared Mesh: Ready")) ||
                        label->text() == QStringLiteral("Prepared Mesh: Preparation failed") ||
                        label->text() == QStringLiteral("Prepared Mesh: Safe workload limit exceeded") ||
                        label->text() == QStringLiteral("Prepared Mesh: Built-in preparation unsupported") ||
                        label->text() == QStringLiteral("Prepared Mesh: Construction ambiguous")) {
                        result = label->text(); return true;
                    }
                }
                return false;
            }, 180000);
            qInfo().noquote() << request.partNumber << result;
            const bool expectedFailure = request.partNumber == QStringLiteral("3021");
            if (!finished || (expectedFailure
                    ? result != QStringLiteral("Prepared Mesh: Preparation failed")
                    : !result.startsWith(QStringLiteral("Prepared Mesh: Ready")))) return 1;
        }
        qInfo() << "Native installed-LDraw acceptance: PASS; isolated library setting persisted";
        return 0;
    }
    return app.exec();
}
