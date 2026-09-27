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
#include <cstdio>
namespace {
bool check(bool value,const char* message){if(!value)fprintf(stderr,"FAIL: %s\n",message);return value;}
QPushButton* button(QWidget& w,const QString& text){for(auto* b:w.findChildren<QPushButton*>())if(b->text()==text)return b;return nullptr;}
QComboBox* modes(QWidget& w){for(auto* c:w.findChildren<QComboBox*>())if(c->findText("Custom Calibration Package")>=0)return c;return nullptr;}
QComboBox* workspaces(QWidget& w,const QString& identity){for(auto* c:w.findChildren<QComboBox*>())if(c->findData(identity)>=0)return c;return nullptr;}
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
    QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,temp.path());
    app.setOrganizationName("CalibrationUiTests");app.setApplicationName("WorkspaceLifecycle");
    bool ok=true;FitCalibrationDialog dialog(nullptr,temp.path()+"/managed");dialog.show();
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
    ok&=check(!b.isEmpty()&&workspaces(dialog,b)->currentData().toString()==b,"B immediately current");
    ok&=generationModes(dialog);
    auto* selection=workspaces(dialog,a.identity);selection->setCurrentIndex(selection->findData(a.identity));button(dialog,"Resume Workspace")->click();
    ok&=generationModes(dialog);
    button(dialog,"Close")->click();
    FitCalibrationDialog reopened(nullptr,temp.path()+"/managed");reopened.show();selection=workspaces(reopened,b);selection->setCurrentIndex(selection->findData(b));button(reopened,"Resume Workspace")->click();
    ok&=generationModes(reopened);
    ok&=check(library.sessions().isEmpty(),"chooser tests create no geometry or physical evidence");
    return ok?0:1;
}

