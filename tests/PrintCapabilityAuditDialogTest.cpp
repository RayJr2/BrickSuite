#include "../src/ui/parts/PrintCapabilityAuditDialog.h"
#include <QApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonDocument>
#include <QLabel>
#include <QPushButton>
#include <QSemaphore>
#include <QSettings>
#include <QTemporaryDir>
#include <QThread>
#include <cstdio>

using namespace PrintGeometry;
namespace {
bool check(bool value,const char* message){if(!value)fprintf(stderr,"FAILED: %s\n",message);return value;}
bool until(const std::function<bool()>& predicate){
    QElapsedTimer timer;timer.start();
    while(!predicate()&&timer.elapsed()<5000){QCoreApplication::processEvents();QThread::msleep(1);}
    return predicate();
}
QPushButton* button(PrintCapabilityAuditDialog& dialog,const QString& text){
    for(auto* b:dialog.findChildren<QPushButton*>())if(b->text()==text)return b;
    return nullptr;
}
QJsonObject state(const QString& path){QFile f(path);return f.open(QIODevice::ReadOnly)?
    QJsonDocument::fromJson(f.readAll()).object():QJsonObject();}
}
int main(int argc,char** argv)
{
    QApplication app(argc,argv);app.setQuitOnLastWindowClosed(false);
    QTemporaryDir temp;
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,temp.path());
    app.setOrganizationName("AuditLifecycleTests");app.setApplicationName("AuditLifecycle");
    QSettings().setValue("printAudit/outputRoot",temp.path());
    bool ok=true;
    {
        PrintCapabilityAuditDialog dialog;dialog.show();
        button(dialog,"Close")->click();
        ok&=check(!dialog.isVisible(),"idle Close is immediate");
    }
    {
        PrintCapabilityAuditDialog dialog(nullptr,[](const BatchPrintOptions& base,CancellationState* cancel,
                                                       const BatchPrintableModelService::Progress& progress){
            auto options=base;options.randomSample=false;
            BatchPrintablePart part;part.catalogPresent=true;part.partNumber="999999999";
            return BatchPrintableModelService().run({part},options,cancel,progress);
        });
        dialog.show();button(dialog,"Start")->click();
        ok&=check(until([&]{return button(dialog,"Start")->isEnabled();}),"fast audit completes normally");
        bool complete=false;for(auto* label:dialog.findChildren<QLabel*>())complete|=label->text().startsWith("Complete.");
        ok&=check(complete,"normal completion is reported");
        button(dialog,"Close")->click();ok&=check(!dialog.isVisible(),"Close after normal completion is immediate");
    }
    for(int scenario=0;scenario<3;++scenario){
        QSemaphore entered,release;QString statePath;
        PrintCapabilityAuditDialog dialog(nullptr,[&](const BatchPrintOptions& base,CancellationState* cancel,
                                                      const BatchPrintableModelService::Progress& progress){
            auto options=base;options.randomSample=false;
            options.beforePart=[&](const QString& path,int,const QString&){
                statePath=path;entered.release();release.acquire();
            };
            BatchPrintablePart part;part.catalogPresent=true;part.partNumber="999999999";
            return BatchPrintableModelService().run({part,part},options,cancel,progress);
        });
        dialog.show();button(dialog,"Start")->click();
        const bool active=until([&]{return entered.available()>0;});
        ok&=check(active,"worker reaches safe simulated slow operation");
        if(!active){release.release(2);continue;}
        button(dialog,"Stop")->click();
        ok&=check(state(statePath).value("status")==QStringLiteral("stopping")&&
            state(statePath).value("currentPartNumber")==QStringLiteral("999999999"),
            "Stop immediately persists active identity and stopping status");
        ok&=check(button(dialog,"Stopping...")&&!button(dialog,"Stopping...")->isEnabled(),
            "Stop becomes disabled Stopping state");
        if(scenario==1)button(dialog,"Close")->click();
        if(scenario==2)dialog.close();
        if(scenario>0){
            bool closing=false;for(auto* label:dialog.findChildren<QLabel*>())closing|=label->text().contains("closing");
            ok&=check(dialog.isVisible()&&closing,"Close/X waits visibly at safe boundary");
        }
        release.release();
        ok&=check(until([&]{return state(statePath).value("status")==QStringLiteral("stopped");}),
            "worker finishes with clean stopped state");
        if(scenario==0){
            ok&=check(until([&]{return button(dialog,"Start")->isEnabled();}),"finished notification delivered");
            button(dialog,"Close")->click();
        }
        ok&=check(until([&]{return !dialog.isVisible();}),"Close after Stop or queued Close/X closes safely");
        ok&=check(state(statePath).value("completedCount").toInt()==0&&
            state(statePath).value("currentSampleSequence").toInt()==1,"Stop starts no next Part");
    }
    {
        PrintCapabilityAuditDialog reopened;reopened.show();bool found=false;
        for(auto* label:reopened.findChildren<QLabel*>())found|=label->text().contains("stopped");
        ok&=check(found,"reopening identifies stopped run");reopened.close();
    }
    return ok?0:1;
}
