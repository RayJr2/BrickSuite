#include "../src/ui/parts/LDrawModelViewerWindow.h"
#include "../src/ui/parts/PrintPreparationCoordinator.h"
#include "../src/settings/UserSettings.h"
#include "../src/database/DatabaseManager.h"
#include "../src/services/geometry/LDrawLibraryService.h"
#include "../src/services/geometry/ThreeMfWriter.h"
#include "../src/services/geometry/print/PrintMeshConversion.h"
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
#include <QFileDialog>
#include <QMessageBox>
#include <QTimer>
#include <QComboBox>
#include "../src/services/geometry/fit/FitCalibrationLibrary.h"
#include <cstdio>

using namespace PrintGeometry;
namespace {
FitCalibrationSession verifiedSession() {
    FitCalibrationSession session; session.sessionIdentity = "stable-session";
    session.process.printerIdentity = "Bambu H2D"; session.process.materialIdentity = "PETG";
    session.process.profileName = "0.20 mm Standard"; session.process.hasNozzleDiameter = true;
    session.process.nozzleDiameterMillimetres = .4; session.process.hasLayerHeight = true;
    session.process.layerHeightMillimetres = .2;
    session.process.actualPrintedOrientation = FitPrintedOrientation::FeatureAxisPerpendicularToBuildPlate;
    session.process.orientationNotes = "Physical feature axis perpendicular";
    session.process.dimensionalCompensationNotes = "Defaults; no explicit compensation";
    session.hasFineExperiment = true; auto& experiment = session.fineExperiment;
    experiment.artifactIdentity = "round-technic-female-perpendicular-verification-v2";
    experiment.parentArtifactIdentity = "round-technic-female-perpendicular-v1";
    experiment.featureFamily = "RoundTechnicPassage"; experiment.featureRole = "female";
    experiment.modeledOrientationIdentity = "feature-axis-perpendicular-to-build-plate";
    experiment.process = session.process; experiment.preferredCandidateIndex = 2;
    experiment.state = FitEvidenceState::Verified;
    for (int index = 1; index <= 3; ++index) {
        FitCalibrationCandidate candidate; candidate.index = index;
        candidate.diameterCorrectionMillimetres = .175 + .025 * (index - 1);
        candidate.functionalDiameterMillimetres = 4.8 + candidate.diameterCorrectionMillimetres;
        FitCalibrationObservation observation; observation.repeatNumber = 1;
        observation.result = index == 2 ? FitObservation::Preferred : FitObservation::Acceptable;
        observation.notes = QStringLiteral("physical result %1").arg(index);
        observation.performedUtc = QDateTime::fromString("2026-09-20T12:00:00.000Z", Qt::ISODateWithMs);
        candidate.observations.push_back(observation);
        if (index == 2) { observation.repeatNumber = 2; candidate.observations.push_back(observation); }
        experiment.candidates.push_back(candidate);
    }
    return session;
}
bool check(bool value,const char* message){if(!value)fprintf(stderr,"FAIL: %s\n",message);return value;}
bool until(const std::function<bool()>& predicate){QElapsedTimer timer;timer.start();while(!predicate()&&timer.elapsed()<10000){QCoreApplication::processEvents();QThread::msleep(1);}return predicate();}
bool text(LDrawModelViewerWindow& viewer,const QString& value){for(auto* label:viewer.findChildren<QLabel*>())if(label->text().contains(value))return true;return false;}
QPushButton* button(LDrawModelViewerWindow& viewer,const QString& value){for(auto* item:viewer.findChildren<QPushButton*>())if(item->text().contains(value))return item;return nullptr;}
bool write(const QString& path,const QByteArray& data){QFile file(path);return file.open(QIODevice::WriteOnly)&&file.write(data)==data.size();}
}
int main(int argc,char** argv)
{
    if(const auto worker=PrintGeometry::LocalPrintableOverrideService::runUnionWorker(argc,argv))return *worker;
    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
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
    {
        // A real saved synthetic override exercises asynchronous viewer reload,
        // Source switching, removal, and isolation from later catalog sessions.
        const QVector<QVector3D> vertices{{0,0,0},{10,0,0},{10,10,0},{0,10,0},{0,0,10},{10,0,10},{10,10,10},{0,10,10}};
        const std::vector<PrintGeometry::Face> faces{{0,2,1},{0,3,2},{4,5,6},{4,6,7},{0,1,5},{0,5,4},{1,2,6},{1,6,5},{2,3,7},{2,7,6},{3,0,4},{3,4,7}};
        QByteArray data="0 !LDRAW_ORG Part\n0 BFC CERTIFY CCW\n";
        for(const auto& face:faces){data+="3 16";for(auto i:face){const auto p=vertices[int(i)];data+=' '+QByteArray::number(p.x())+' '+QByteArray::number(p.y())+' '+QByteArray::number(p.z());}data+='\n';}
        if(!write(root.filePath("parts/override-test.dat"),data))return 1;
        const auto loaded=LDrawLibraryService::loadPart(root.path(),"override-test");
        PrintGeometry::LocalPrintableOverrideService service;
        PrintGeometry::LocalPrintableOverrideService::Context context{987654,"override-test",loaded};
        ThreeMfWriter::Options options;options.modelColor=QColor(160,160,160);QString error;
        const auto repairPath=root.filePath("repaired.3mf");
        ok&=check(ThreeMfWriter::write(PrintGeometry::PrintMeshConversion::fromPartMesh(loaded.mesh),repairPath,options,&error),"write viewer repair fixture");
        ok&=check(service.importRepaired(context,repairPath).ok(),"store viewer override fixture");
        LDrawModelViewerRequest catalog;catalog.partId=context.partId;catalog.partNumber=context.partNumber;catalog.candidates={"override-test"};
        LDrawModelViewerWindow viewer(&coordinator);viewer.showPart(catalog);
        ok&=check(until([&]{return text(viewer,"Ready — Strictly Validated Local Override (Nominal)");}),"saved override loads as nominal Ready");
        ok&=check(!button(viewer,"Prepare for Printing")->isEnabled(),"active override does not launch native preparation");
        LDrawModelViewerRequest other;other.partId=1;other.partNumber="3001";other.candidates={"3001"};viewer.showPart(other);
        ok&=check(until([&]{return button(viewer,"Prepare for Printing")->isEnabled();})&&!text(viewer,"Ready — Strictly Validated Local Override"),"override does not leak to another catalog Part");
        viewer.showPart(catalog);
        ok&=check(until([&]{return text(viewer,"Ready — Strictly Validated Local Override (Nominal)");}),"returning reloads bound override");
        button(viewer,"Remove Local Override")->click();
        ok&=check(button(viewer,"Prepare for Printing")->isEnabled()&&!service.load(context).ok(),"remove restores native preparation and removes storage");
        service.remove(context,&error);
        // Retain an internal open source sheet: reverse correspondence is
        // unresolved, but the repaired envelope is objectively reviewable.
        data+="3 16 4 5 4 6 5 4 5 5 6\n";
        ok&=check(write(root.filePath("parts/override-test.dat"),data),"write reviewable source composition");
        context.source=LDrawLibraryService::loadPart(root.path(),"override-test");
        viewer.showPart(catalog);
        ok&=check(until([&]{return button(viewer,"Import Repaired Mesh")->isEnabled();}),"review source loaded");
        bool pickerShown=false;
        QTimer::singleShot(0,[&]{for(auto* widget:QApplication::topLevelWidgets())if(auto* picker=qobject_cast<QFileDialog*>(widget)){pickerShown=true;picker->selectFile(repairPath);QMetaObject::invokeMethod(picker,"accept",Qt::QueuedConnection);}});
        button(viewer,"Import Repaired Mesh")->click();
        ok&=check(pickerShown&&until([&]{return text(viewer,"Reviewable — Source exposure correspondence unresolved");}),"UI exposes review only after validation");
        auto* accept=viewer.findChild<QPushButton*>("acceptNominalOverride");
        bool warningShown=false;
        QTimer::singleShot(0,[&]{for(auto* widget:QApplication::topLevelWidgets())if(auto* warning=qobject_cast<QMessageBox*>(widget)){warningShown=warning->text().contains("after your own review");warning->button(QMessageBox::Cancel)->click();}});
        accept->click();
        ok&=check(warningShown&&!service.load(context).ok(),"Cancel leaves candidate unaccepted");
        QTimer::singleShot(0,[&]{for(auto* widget:QApplication::topLevelWidgets())if(auto* warning=qobject_cast<QMessageBox*>(widget))for(auto* choice:warning->buttons())if(choice->text()=="Accept as Nominal Local Override")choice->click();});
        accept->click();
        ok&=check(until([&]{return text(viewer,"Ready — User-Accepted Local Override (Nominal)");})&&service.load(context).ok(),"explicit UI confirmation persists accepted nominal geometry");
        FitCalibrationLibrary profiles;auto session=verifiedSession();FitProfile profile;
        ok&=check(profiles.saveSession(&session,&error)&&FitCalibrationLibrary::promoteVerifiedSession(session,"Synthetic PETG Verified profile",&profile,&error)&&profiles.saveProfile(&profile,&error),"create isolated Verified profile fixture");
        bool nominalClear=false,profileAvailable=false;
        QTimer::singleShot(0,[&]{for(auto* widget:QApplication::topLevelWidgets())if(auto* dialog=qobject_cast<QDialog*>(widget))if(dialog->windowTitle()=="Export 3D Model"){
            auto* wording=dialog->findChild<QLabel*>("nominalFitProfileMessage");auto* available=dialog->findChild<QLabel*>("availableVerifiedFitProfiles");
            nominalClear=wording&&wording->isVisible()&&wording->text()=="Not applicable — nominal PreparedMesh export";
            profileAvailable=available&&available->isVisible()&&available->text().contains(profile.name);dialog->reject();
        }});
        button(viewer,"Export 3D Model")->click();
        ok&=check(nominalClear&&profileAvailable,"nominal export shows non-applicability and available Verified profile independently");
        auto* fit=viewer.findChild<QPushButton*>("experimentalOverrideFit");
        bool fitWarning=false;
        QTimer::singleShot(0,[&]{for(auto* widget:QApplication::topLevelWidgets())if(auto* warning=qobject_cast<QMessageBox*>(widget)){auto* choices=warning->findChild<QComboBox*>("experimentalFitProfile");fitWarning=warning->text().contains("incorrect region")&&choices&&choices->currentText()==profile.name;warning->button(QMessageBox::Cancel)->click();}});
        fit->click();
        ok&=check(fitWarning&&text(viewer,"Ready — User-Accepted Local Override"),"experimental warning Cancel retains nominal");
        QTimer::singleShot(0,[&]{for(auto* widget:QApplication::topLevelWidgets())if(auto* warning=qobject_cast<QMessageBox*>(widget))for(auto* choice:warning->buttons())if(choice->text()=="Apply Experimental Auto Fit")choice->click();});
        fit->click();
        ok&=check(until([&]{return text(viewer,"Experimental Auto Fit failed safely");})&&text(viewer,"Ready — User-Accepted Local Override"),"experimental failure preserves nominal Ready");
        ok&=check(text(viewer,profile.name)&&text(viewer,"mapped repaired-surface fit features: 0")&&text(viewer,"correction entries considered: 1")&&text(viewer,"corrections applied: 0"),"selected available profile is distinct from zero mappings/corrections");
        viewer.close();
        LDrawModelViewerWindow restart(&coordinator);restart.showPart(catalog);
        ok&=check(until([&]{return text(restart,"Ready — User-Accepted Local Override");}),"accepted provenance persists across viewer restart");
        restart.showPart(other);
        ok&=check(until([&]{return button(restart,"Prepare for Printing")->isEnabled();})&&!restart.findChild<QPushButton*>("acceptNominalOverride")->isVisible()&&!restart.findChild<QPushButton*>("experimentalOverrideFit")->isVisible(),"review and experimental state cannot leak to another Part");
        service.remove(context,&error);
    }
    QThreadPool::globalInstance()->waitForDone();
    return ok?0:1;
}
