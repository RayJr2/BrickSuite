#include "../src/ui/parts/LDrawViewportWidget.h"
#include "../src/ui/parts/PartViewerSurfaceFormat.h"

#include <QApplication>
#include <QDebug>
#include <QEventLoop>
#include <QOpenGLContext>
#include <QTimer>
#include <QVBoxLayout>
#include <cstring>

namespace {
void settle()
{
    QEventLoop loop;
    QTimer::singleShot(150, &loop, &QEventLoop::quit);
    loop.exec();
}

bool require(bool value, const char* message)
{
    if (!value) qCritical() << message;
    return value;
}

int brightPixels(const QImage& frame)
{
    int count = 0;
    for (int y = 0; y < frame.height(); ++y)
        for (int x = 0; x < frame.width(); ++x)
            if (frame.pixelColor(x, y).lightness() > 60) ++count;
    return count;
}
}

// Requires a native desktop OpenGL context; intentionally not an offscreen CTest.
// --legacy-default reproduces the macOS compositor-sharing regression.
int main(int argc, char** argv)
{
    bool legacy = false;
    for (int i = 1; i < argc; ++i)
        legacy |= std::strcmp(argv[i], "--legacy-default") == 0;
    if (!legacy) configurePartViewerSurfaceFormat();
    QApplication app(argc, argv);
    QWidget window;
    window.setWindowTitle(QStringLiteral("BrickSuite native viewport regression"));
    auto* layout = new QVBoxLayout(&window);
    auto* viewport = new LDrawViewportWidget(&window);
    layout->addWidget(viewport);
    bool ok = true;
    QObject::connect(viewport, &LDrawViewportWidget::renderingError,
                     [&ok](const QString& message) { qCritical() << message; ok = false; });
    window.resize(640, 480);
    window.show();
    settle();
    ok &= require(viewport->renderingAvailable(), "Renderer initialization failed");
    ok &= require(viewport->context() && viewport->context()->shareContext(),
                  "Viewer cannot share its framebuffer texture with the window compositor");
    if (!viewport->renderingAvailable()) return 1;

    viewport->setShowAxes(false);
    settle();
    const QImage empty = viewport->grabFramebuffer();
    ok &= require(!empty.isNull() && brightPixels(empty) == 0, "Empty viewport baseline failed");

    LDrawGeometry::PartMesh mesh;
    mesh.hasBounds = true;
    mesh.minimumBounds = {-20, -20, 0};
    mesh.maximumBounds = {20, 20, 0};
    mesh.triangles.push_back({{-20, -20, 0}, {20, -20, 0}, {0, 20, 0},
                              {0, 0, 1}, QStringLiteral("16"), false});
    viewport->setMesh(mesh, true);
    viewport->setRenderMode(PartViewerRenderMode::Solid);
    settle();
    const QImage solid = viewport->grabFramebuffer();
    ok &= require(brightPixels(solid) > 1000, "Loaded geometry did not render");
    viewport->setRenderMode(PartViewerRenderMode::Wireframe);
    settle();
    const QImage wire = viewport->grabFramebuffer();
    ok &= require(brightPixels(wire) > 100 && brightPixels(wire) < brightPixels(solid),
                  "Wireframe did not replace the solid faces");
    viewport->clearMesh();
    viewport->setShowAxes(true);
    settle();
    ok &= require(brightPixels(viewport->grabFramebuffer()) > 50, "Axes did not render");
    window.resize(800, 550);
    settle();
    ok &= require(brightPixels(viewport->grabFramebuffer()) > 50, "Axes lost after resize");
    qInfo() << "Native viewport regression:" << (ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}
