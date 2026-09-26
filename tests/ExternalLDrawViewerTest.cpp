#include "../src/ui/parts/LDrawModelViewerWindow.h"
#include "../src/ui/parts/PrintPreparationCoordinator.h"
#include "../src/settings/UserSettings.h"
#include "../src/database/DatabaseManager.h"
#include <QStandardPaths>
#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QLabel>
#include <QPushButton>
#include <QSettings>
#include <QTemporaryDir>
#include <QThread>
#include <QThreadPool>
#include <cstdio>

namespace {
bool check(bool value,const char* message){if(!value)fprintf(stderr,"FAIL: %s\n",message);return value;}
bool until(const std::function<bool()>& predicate){QElapsedTimer timer;timer.start();while(!predicate()&&timer.elapsed()<10000){QCoreApplication::processEvents();QThread::msleep(1);}return predicate();}
bool text(LDrawModelViewerWindow& viewer,const QString& value){for(auto* label:viewer.findChildren<QLabel*>())if(label->text().contains(value))return true;return false;}
QPushButton* button(LDrawModelViewerWindow& viewer,const QString& value){for(auto* item:viewer.findChildren<QPushButton*>())if(item->text().contains(value))return item;return nullptr;}
bool write(const QString& path,const QByteArray& data){QFile file(path);return file.open(QIODevice::WriteOnly)&&file.write(data)==data.size();}
}
int main(int argc,char** argv)
{
    QApplication app(argc,argv);app.setQuitOnLastWindowClosed(false);
    QTemporaryDir temporary;
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,temporary.path());
    app.setOrganizationName("ExternalViewerTests");app.setApplicationName("ExternalViewerTests");
    QStandardPaths::setTestModeEnabled(true);
    if(!DatabaseManager::instance().initialize())return 1;
    QDir root(temporary.path());root.mkpath("parts");root.mkpath("p");
    const QByteArray triangle="0 !LDRAW_ORG Part\n0 BFC CERTIFY CCW\n3 16 0 0 0 20 0 0 0 20 0\n";
    if(!write(root.filePath("parts/3001.dat"),triangle)||!write(root.filePath("alternate.dat"),triangle)
       ||!write(root.filePath("missing.ldr"),"1 16 0 0 0 1 0 0 0 1 0 0 0 1 absent.dat\n"))return 1;
    UserSettings::instance().setLDrawLibraryPath(root.path());
    UserSettings::instance().setMeshRepairEnabled(true);
    PrintPreparationCoordinator coordinator;
    bool ok=true;
    {
        LDrawModelViewerWindow viewer(&coordinator);
        LDrawModelViewerRequest external;external.externalFilePath=root.filePath("alternate.dat");
        viewer.showPart(external);
        ok&=check(until([&]{return button(viewer,"Prepare for Printing")->isEnabled();}),"external file loads asynchronously");
        ok&=check(text(viewer,"External LDraw File: alternate.dat"),"external source label");
        button(viewer,"Prepare for Printing")->click();
        ok&=check(until([&]{return !coordinator.busy();}),"failed preparation finishes");
        ok&=check(!text(viewer,"Ready for Printing"),"open triangle cannot become prepared");
        external.externalFilePath=root.filePath("missing.ldr");viewer.showPart(external);
        ok&=check(until([&]{return text(viewer,"absent.dat");}),"missing reference is visible");
        ok&=check(!button(viewer,"Export")->isEnabled()&&!button(viewer,"Prepare for Printing")->isEnabled(),"failed load clears export and preparation");
        external.externalFilePath=root.filePath("alternate.dat");viewer.showPart(external);
        LDrawModelViewerRequest catalog;catalog.partNumber="3001";catalog.partName="Catalog test";catalog.candidates={"3001"};
        viewer.showPart(catalog);
        ok&=check(until([&]{return button(viewer,"Prepare for Printing")->isEnabled();}),"catalog supersedes pending external load");
        ok&=check(!text(viewer,"External LDraw File:")&&text(viewer,"Installed LDraw library"),"catalog session has no external state");
        viewer.close();
    }
    {
        LDrawModelViewerWindow reopened(&coordinator);
        PrintGeometry::PrintPreparationResult stale;stale.state=PrintGeometry::PrintPreparationState::Ready;
        stale.preparedMesh=std::make_shared<PrintGeometry::PreparedMesh>();
        LDrawModelViewerRequest external;external.externalFilePath=root.filePath("alternate.dat");reopened.showPart(external);
        coordinator.completed(1,stale);
        ok&=check(!text(reopened,"Ready for Printing"),"reopened viewer rejects old completion with reused generation");
        ok&=check(until([&]{return button(reopened,"Prepare for Printing")->isEnabled();}),"reopened viewer loads external source");
    }
    QThreadPool::globalInstance()->waitForDone();
    return ok?0:1;
}
