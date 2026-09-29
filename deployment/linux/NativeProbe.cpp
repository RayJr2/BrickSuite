// Disposable native acceptance harness linked to the actual Release objects.
#include "src/app/Application.h"
#include "src/settings/UserSettings.h"
#include "src/services/CredentialStore.h"
#include "src/network/BrickSuiteHostIdentity.h"
#include "src/ui/parts/PartViewerSurfaceFormat.h"
#include "src/ui/parts/LDrawModelViewerWindow.h"
#include "src/ui/parts/PrintPreparationCoordinator.h"
#include "src/ui/common/TooltipPolicy.h"
#include <QtWidgets>
#include <QKeyEvent>
#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QTcpServer>
#include <QSslSocket>

static void pause(int ms) { QEventLoop loop; QTimer::singleShot(ms,&loop,&QEventLoop::quit); loop.exec(); }
static bool check(bool ok,const QString& name) {qInfo().noquote()<<(ok?"PASS":"FAIL")<<name;return ok;}
static bool wait(const std::function<bool()>& ready,int timeout=60000) {
    QElapsedTimer timer;timer.start();while(!ready()&&timer.elapsed()<timeout)pause(50);return ready();
}
static QAction* action(QWidget* w,const QString& name) {for(auto* a:w->findChildren<QAction*>())if(a->text().remove('&')==name)return a;return nullptr;}
static QPushButton* button(QWidget* w,const QString& name) {for(auto* b:w->findChildren<QPushButton*>())if(b->text()==name)return b;return nullptr;}
int main(int argc,char** argv)
{
    configurePartViewerSurfaceFormat();QApplication q(argc,argv);q.setQuitOnLastWindowClosed(false);
    q.setOrganizationName("BrickSuitePackageAcceptance");q.setApplicationName("NativeProbe");
    const QString output=qEnvironmentVariable("M39_AUDIT_OUTPUT"),ldraw=qEnvironmentVariable("BRICKSUITE_TEST_LDRAW");
    if(output.isEmpty()||!QFile::exists(ldraw+"/parts/2456.dat"))return 2;
    QDir().mkpath(output);new TooltipPolicy(q);
    auto& settings=UserSettings::instance();settings.setLDrawLibraryPath(ldraw);
    QString error;const auto token=QUuid::createUuid().toString()+QUuid::createUuid().toString();
    if(!CredentialStore::write("BrickSuiteHostAccessToken",token,&error))return 3;
    QTcpServer reserve;if(!reserve.listen(QHostAddress::LocalHost,0))return 4;
    const int port=reserve.serverPort();reserve.close();
    settings.setBrickSuiteServerBindAddress("127.0.0.1");settings.setBrickSuiteServerPort(port);settings.setBrickSuiteServerEnabled(true);
    Application app;if(!app.initialize())return 5;
    auto* window=app.mainWindow();window->resize(1200,800);window->show();pause(500);
    bool ok=check(window->isVisible(),"production application startup");
    window->grab().save(output+"/startup.png");
    const auto identity=BrickSuiteHostIdentity::loadOrCreate();
    QSslSocket host;
    QObject::connect(&host,&QSslSocket::sslErrors,&host,[&](const QList<QSslError>& errors){if(identity.success&&BrickSuiteHostIdentity::fingerprint(host.peerCertificate())==identity.fingerprint)host.ignoreSslErrors(errors);});
    host.connectToHostEncrypted("127.0.0.1",port);
    ok &= check(wait([&]{return host.isEncrypted();},10000),"production Application Host listener: pinned TLS connection");host.disconnectFromHost();
    window->activateWindow();window->raise();QApplication::setActiveWindow(window);window->setFocus();pause(150);QKeyEvent press(QEvent::KeyPress,Qt::Key_F1,Qt::NoModifier);QApplication::sendEvent(window,&press);
    QKeyEvent release(QEvent::KeyRelease,Qt::Key_F1,Qt::NoModifier);QApplication::sendEvent(window,&release);
    bool help=false;pause(500);
    for(auto* w:QApplication::topLevelWidgets())if(w!=window&&w->isVisible()) {
        if(w->windowTitle().contains("Help",Qt::CaseInsensitive)){help=true;w->grab().save(output+"/help.png");w->close();}
    }
    ok &= check(help,"F1 opens built-in Help");
    {
        QFileDialog dialog(window,"Package file dialog",ldraw);dialog.show();pause(300);
        ok &= check(dialog.isVisible()&&dialog.directory().absolutePath()==QDir(ldraw).absolutePath(),"file dialog opens installed LDraw directory");
        dialog.grab().save(output+"/file-dialog.png");dialog.reject();
    }
    PrintPreparationCoordinator coordinator;
    const bool full=q.arguments().contains("--prepare");
    for(const QString part:full?QStringList{"2456","3037","3021"}:QStringList{"3001"}) {
        LDrawModelViewerWindow viewer(&coordinator,window);LDrawModelViewerRequest request;
        request.partNumber=part;request.partName="Package acceptance "+part;request.candidates={part};
        viewer.showPart(request);viewer.resize(1200,800);viewer.show();
        auto* prepare=button(&viewer,"Prepare for Printing");
        ok &= check(prepare&&wait([&]{return prepare->isEnabled();}),part+" installed LDraw source loads");
        auto* gl=viewer.findChild<QOpenGLWidget*>();
        ok &= check(gl&&gl->isValid()&&!gl->grabFramebuffer().isNull(),part+" 3D Viewer valid OpenGL framebuffer");
        if(gl&&gl->isValid()) {gl->makeCurrent();qInfo()<<"OPENGL"<<reinterpret_cast<const char*>(gl->context()->functions()->glGetString(GL_RENDERER));gl->doneCurrent();}
        viewer.grab().save(output+"/viewer-"+part+".png");
        if(full&&prepare&&prepare->isEnabled()) {
            prepare->click();
            QLabel* status=nullptr;for(auto* label:viewer.findChildren<QLabel*>())if(label->text().startsWith("Prepared Mesh:"))status=label;
            const bool ended=status&&wait([&]{return !status->text().contains("Preparing");},180000);
            const bool ready=ended&&status->text().contains("Ready for Printing");
            qInfo().noquote()<<"PREPARATION"<<part<<(status?status->text():"missing status");
            ok &= check(part=="3021" ? ended&&!ready : ready,part+" expected frozen preparation outcome");
            viewer.grab().save(output+"/prepared-"+part+".png");
        }
        viewer.close();
    }
    if(auto* calibration=action(window,"LEGO Fit Calibration...")) {
        calibration->trigger();pause(400);bool visible=false;
        for(auto* w:QApplication::topLevelWidgets())if(w!=window&&w->isVisible()){visible=true;w->grab().save(output+"/calibration.png");w->close();}
        ok &= check(visible,"calibration workspace opens");
    }else ok &= check(false,"calibration action exists");
    return ok?0:1;
}
