#include "../src/ui/parts/FitCalibrationDialog.h"
#include <QApplication>
#include <QAction>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QTemporaryDir>
#include <QTimer>
#include <QPlainTextEdit>
#include <QTableWidget>
#include <QProcess>
#include <QFile>
#include <cstdio>
namespace {
constexpr auto lastWorkspaceKey="LegoFitCalibration/LastWorkspaceIdentity";
bool check(bool value,const char* message){if(!value)fprintf(stderr,"FAIL: %s\n",message);return value;}
QPushButton* button(QWidget& w,const QString& text){for(auto* b:w.findChildren<QPushButton*>())if(b->text()==text)return b;return nullptr;}
QComboBox* modes(QWidget& w){for(auto* c:w.findChildren<QComboBox*>())if(c->findText("Custom Calibration Package")>=0)return c;return nullptr;}
QComboBox* workspaces(QWidget& w,const QString& identity){for(auto* c:w.findChildren<QComboBox*>())if(c->findData(identity)>=0)return c;return nullptr;}
bool contextVisible(QWidget& dialog,const QString& printer){for(auto* edit:dialog.findChildren<QLineEdit*>())if(edit->text()==printer)return true;return false;}
bool removeWorkspace(FitCalibrationDialog& dialog){
    QAction* action=nullptr;for(auto* a:dialog.findChildren<QAction*>())if(a->text()=="Delete Selected Workspace...")action=a;
    if(!action)return false;
    bool confirmed=false,completed=false;QTimer timer;timer.setInterval(5);
    QObject::connect(&timer,&QTimer::timeout,[&]{if(auto* box=qobject_cast<QMessageBox*>(QApplication::activeModalWidget())){
        if(box->windowTitle()=="Delete Calibration Workspace"){confirmed=true;box->button(QMessageBox::Yes)->click();}
        else {completed=box->windowTitle()=="Calibration Workspace Deleted";box->accept();}
    }});
    timer.start();action->trigger();timer.stop();return confirmed&&completed;
}
bool dirtySwitch(FitCalibrationDialog& dialog,const QString& destination,QMessageBox::StandardButton decision,bool expectFailure=false){
    bool prompted=false,failed=false;QTimer timer;timer.setInterval(5);
    QObject::connect(&timer,&QTimer::timeout,[&]{if(auto* box=qobject_cast<QMessageBox*>(QApplication::activeModalWidget())){
        if(box->windowTitle()=="Unsaved Calibration Changes"){prompted=true;box->button(decision)->click();}
        else {failed=box->windowTitle()=="Calibration";box->reject();}
    }});
    auto* combo=workspaces(dialog,destination);timer.start();combo->setCurrentIndex(combo->findData(destination));timer.stop();
    return prompted&&failed==expectFailure;
}
bool openModal(const std::function<void()>& action,const QString& title,const std::function<void(QDialog*)>& inspect={}){
    bool seen=false;QTimer timer;timer.setInterval(5);
    QObject::connect(&timer,&QTimer::timeout,[&]{if(auto* d=qobject_cast<QDialog*>(QApplication::activeModalWidget())){
        seen=d->windowTitle()==title;if(!seen)fprintf(stderr,"Unexpected modal: %s\n",qPrintable(d->windowTitle()));
        timer.stop();if(seen&&inspect)inspect(d);else d->reject();}});
    timer.start();action();timer.stop();return seen;
}
bool create(FitCalibrationDialog& dialog,const QString& printer){
    QAction* action=nullptr;for(auto* a:dialog.findChildren<QAction*>())if(a->text()=="New Workspace...")action=a;
    return action&&openModal([&]{action->trigger();},"New Calibration Workspace",[&](QDialog* d){
        auto* form=d->findChild<QFormLayout*>();
        qobject_cast<QLineEdit*>(form->itemAt(0,QFormLayout::FieldRole)->widget())->setText(printer);
        qobject_cast<QLineEdit*>(form->itemAt(1,QFormLayout::FieldRole)->widget())->setText("PETG");
        qobject_cast<QLineEdit*>(form->itemAt(3,QFormLayout::FieldRole)->widget())->setText("Synthetic process");
        d->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();
    });
}
bool generationModes(FitCalibrationDialog& dialog){
    bool ok=true;auto* mode=modes(dialog);auto* generate=dialog.findChild<QPushButton*>("fitCalibrationNewAction");
    mode->setCurrentIndex(mode->findText("Custom Calibration Package"));
    ok&=check(generate->isEnabled(),"custom generation enabled");
    ok&=check(openModal([&]{generate->click();},"Custom Calibration Package",[&](QDialog* chooser){
        auto* table=chooser->findChild<QTableWidget*>();
        for(int row=0;row<table->rowCount();++row){const auto family=table->item(row,0)->text(),variant=table->item(row,1)->text();
            const bool core=(family=="Standard Stud"&&variant=="Outside diameter")||
                (family=="Stud Receiving Clutch"&&(variant=="Tube wall cell"||variant=="Post wall cell"))||family=="Technic Axle Hole"||family=="Standard Bar";
            if(core)table->item(row,0)->setCheckState(Qt::Checked);
        }
        const auto text=chooser->findChild<QPlainTextEdit*>()->toPlainText();
        ok&=check(text.contains("5 calibrations selected")&&text.contains("2 printable fixtures"),"custom UI plans five calibrations in two fixtures");chooser->reject();
    }),"custom chooser opens");
    mode->setCurrentIndex(mode->findText("Recommended Calibration Package"));
    ok&=check(openModal([&]{generate->click();},"Recommended Calibration Package"),"recommended plan opens");
    mode->setCurrentIndex(0);auto* family=dialog.findChild<QComboBox*>("fitCalibrationNewFamily");
    family->setCurrentIndex(family->findData(int(FitCalibrationFamily::StudReceivingClutch)));
    ok&=check(openModal([&]{generate->click();},"Calibration"),"single calibration variant chooser opens");
    return ok;
}
}
int main(int argc,char** argv){
    QApplication app(argc,argv);app.setQuitOnLastWindowClosed(false);QTemporaryDir temp;
    QSettings::setDefaultFormat(QSettings::IniFormat);
    const auto args=app.arguments();const int child=args.indexOf("--restore-smoke");
    QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,child>=0?args[child+1]:temp.path());
    app.setOrganizationName("CalibrationUiTests");app.setApplicationName("WorkspaceLifecycle");
    if(child>=0){FitCalibrationDialog restored(nullptr,args[child+2]);
        auto* combo=workspaces(restored,args[child+3]);
        return check(combo&&combo->currentData().toString()==args[child+3]&&contextVisible(restored,"Printer B"),"separate application process restores saved workspace")?0:1;
    }
    bool ok=true;FitCalibrationDialog dialog(nullptr,temp.path()+"/managed");dialog.show();
    ok&=check(!button(dialog,"Resume Workspace"),"Resume button removed");
    ok&=check(create(dialog,"Printer A"),"new workspace created through UI");
    auto* mode=modes(dialog);mode->setCurrentIndex(mode->findText("Custom Calibration Package"));
    auto* generate=dialog.findChild<QPushButton*>("fitCalibrationNewAction");
    ok&=check(generate&&generate->isEnabled(),"package action immediately enabled");
    ok&=check(openModal([&]{generate->click();},"Custom Calibration Package"),"custom chooser opens immediately without restart");
    // An empty workspace has no feature evidence to edit. Exercise the controls
    // a user encounters before choosing a calibration package.
    for(auto* combo:dialog.findChildren<QComboBox*>())if(combo->findText("Select actual printed orientation")>=0&&combo->isEnabled())combo->setCurrentIndex(2);
    ok&=check(openModal([&]{generate->click();},"Custom Calibration Package"),"empty-workspace feature controls cannot silently block generation");
    ok&=generationModes(dialog);
    PrintGeometry::FitCalibrationLibrary library(temp.path()+"/managed");const auto a=library.workspaces().front();
    ok&=check(workspaces(dialog,a.identity)&&workspaces(dialog,a.identity)->currentData().toString()==a.identity,"new workspace A selected");
    bool visible=false;for(auto* edit:dialog.findChildren<QLineEdit*>())visible|=edit->text()=="Printer A";
    ok&=check(visible,"new manufacturing context visible");
    ok&=check(create(dialog,"Printer B"),"switch A to newly created B");
    const auto available=library.workspaces();QString b;for(const auto& workspace:available)if(workspace.identity!=a.identity)b=workspace.identity;
    ok&=check(!b.isEmpty()&&workspaces(dialog,b)->currentData().toString()==b&&QSettings().value(lastWorkspaceKey).toString()==b,"B immediately current and persisted");
    ok&=generationModes(dialog);
    auto* selection=workspaces(dialog,a.identity);selection->setCurrentIndex(selection->findData(a.identity));
    ok&=check(contextVisible(dialog,"Printer A")&&QSettings().value(lastWorkspaceKey).toString()==a.identity,"combo activates A and refreshes context immediately");
    ok&=generationModes(dialog);
    button(dialog,"Close")->click();
    FitCalibrationDialog reopened(nullptr,temp.path()+"/managed");reopened.show();
    ok&=check(contextVisible(reopened,"Printer A")&&workspaces(reopened,a.identity)->currentData().toString()==a.identity,"dialog reopen automatically restores A");
    selection=workspaces(reopened,b);selection->setCurrentIndex(selection->findData(b));
    ok&=check(contextVisible(reopened,"Printer B"),"combo immediately loads B context");
    ok&=generationModes(reopened);
    ok&=check(library.sessions().isEmpty(),"chooser tests create no geometry or physical evidence");
    button(reopened,"Close")->click();QSettings().sync();
    QProcess restart;restart.start(app.applicationFilePath(),{"-platform","offscreen","--restore-smoke",temp.path(),library.storageRoot(),b});
    const bool restarted=restart.waitForFinished(10000);if(!restarted){restart.kill();restart.waitForFinished();}
    ok&=check(restarted&&restart.exitStatus()==QProcess::NormalExit&&restart.exitCode()==0,"QSettings survives application restart");
    QSettings().setValue(lastWorkspaceKey,"missing-workspace");
    {FitCalibrationDialog fallback(nullptr,library.storageRoot());const auto expected=library.workspaces().front().identity;
        ok&=check(workspaces(fallback,expected)->currentData().toString()==expected&&QSettings().value(lastWorkspaceKey).toString()==expected,"stale preference uses deterministic fallback");}
    {QFile record(QDir(library.workspacesDirectory()).filePath(b+".json"));if(!check(record.open(QIODevice::ReadOnly),"read temporary workspace record"))return 1;const auto original=record.readAll();record.close();
        if(!check(record.open(QIODevice::WriteOnly)&&record.write("{}")==2,"inject invalid temporary record"))return 1;record.close();QSettings().setValue(lastWorkspaceKey,b);
        {FitCalibrationDialog fallback(nullptr,library.storageRoot());ok&=check(contextVisible(fallback,"Printer A")&&QSettings().value(lastWorkspaceKey).toString()==a.identity,"invalid saved workspace falls back without creating a replacement");}
        ok&=check(record.open(QIODevice::WriteOnly)&&record.write(original)==original.size(),"restore temporary workspace record");}
    // Create one supported session in temporary storage to exercise real dirty/save handling.
    PrintGeometry::FitCalibrationGenerationService::Request request;request.family="StandardBar";request.workspace=a;
    const auto generated=PrintGeometry::FitCalibrationGenerationService(temp.path()+"/artifacts",library.storageRoot()).generate(request);
    ok&=check(generated.ok(),"temporary editable session generated");if(!generated.ok())return 1;
    QSettings().setValue(lastWorkspaceKey,a.identity);
    {FitCalibrationDialog dirty(nullptr,library.storageRoot());dirty.show();
        QLineEdit* printer=nullptr;for(auto* edit:dirty.findChildren<QLineEdit*>())if(edit->text()=="Printer A")printer=edit;
        printer->setText("Printer A pending edit");
        ok&=check(dirtySwitch(dirty,b,QMessageBox::Cancel),"user can cancel pending-edit workspace switch");
        ok&=check(workspaces(dirty,a.identity)->currentData().toString()==a.identity&&contextVisible(dirty,"Printer A pending edit")&&
            QSettings().value(lastWorkspaceKey).toString()==a.identity,"Cancel preserves edits, selection, and last-used preference");
        const auto sessionPath=QDir(library.sessionsDirectory()).filePath(generated.session.sessionIdentity+".json");
        ok&=check(QFile::rename(sessionPath,sessionPath+".hold")&&QDir().mkdir(sessionPath),"inject failed-save destination");
        ok&=check(dirtySwitch(dirty,b,QMessageBox::Save,true),"failed dirty save reported and switch aborted");
        ok&=check(workspaces(dirty,a.identity)->currentData().toString()==a.identity&&contextVisible(dirty,"Printer A pending edit")&&
            QSettings().value(lastWorkspaceKey).toString()==a.identity,"aborted switch preserves edits, previous combo selection, and preference");
        ok&=check(QDir().rmdir(sessionPath)&&QFile::rename(sessionPath+".hold",sessionPath),"restore temporary save destination");
        printer->setText("Printer A");ok&=check(dirtySwitch(dirty,b,QMessageBox::Save),"pending edits use normal managed save before switching");
        ok&=check(contextVisible(dirty,"Printer B")&&QSettings().value(lastWorkspaceKey).toString()==b,"successful dirty save permits activation");
        button(dirty,"Close")->click();}
    {FitCalibrationDialog deleting(nullptr,library.storageRoot());deleting.show();
        ok&=check(removeWorkspace(deleting),"active B deleted through UI");
        ok&=check(contextVisible(deleting,"Printer A")&&QSettings().value(lastWorkspaceKey).toString()==a.identity,"deletion activates and persists fallback A");
        ok&=check(removeWorkspace(deleting),"final workspace deleted through UI");
        ok&=check(library.workspaces().isEmpty()&&!QSettings().contains(lastWorkspaceKey)&&!contextVisible(deleting,"Printer A"),"final deletion clears current context and preference");
        modes(deleting)->setCurrentIndex(2);ok&=check(!deleting.findChild<QPushButton*>("fitCalibrationNewAction")->isEnabled(),"empty state disables package generation");}
    return ok?0:1;
}

