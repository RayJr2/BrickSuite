#include "../src/ui/help/HelpDialog.h"
#include "../src/ui/help/HelpManager.h"
#include "../src/ui/common/TooltipPolicy.h"
#include "../src/settings/ThemeManager.h"
#include <QApplication>
#include <QAction>
#include <QFile>
#include <QHelpEvent>
#include <QKeyEvent>
#include <QLineEdit>
#include <QProcess>
#include <QSettings>
#include <QTemporaryDir>
#include <QTextBrowser>
#include <QTextCursor>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <cmath>
#include <cstdio>

namespace {
bool check(bool value,const char* message){if(!value)fprintf(stderr,"FAIL: %s\n",message);return value;}
class TooltipProbe : public QWidget {
public:
    int delivered=0;
    bool event(QEvent* event) override {
        if(event->type()==QEvent::ToolTip){++delivered;return true;}
        return QWidget::event(event);
    }
};
double luminance(const QColor& c){
    const auto linear=[](double x){return x<=.04045?x/12.92:std::pow((x+.055)/1.055,2.4);};
    return .2126*linear(c.redF())+.7152*linear(c.greenF())+.0722*linear(c.blueF());
}
}
int main(int argc,char** argv){
    QApplication app(argc,argv);app.setQuitOnLastWindowClosed(false);QTemporaryDir temp;
    const auto args=app.arguments();const int child=args.indexOf("--preference-child");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,child>=0?args[child+1]:temp.path());
    app.setOrganizationName("PrintingHelpTests");app.setApplicationName("IsolatedUx");
    auto& settings=UserSettings::instance();
    if(child>=0)return check(!settings.explanatoryTooltipsEnabled(),"tooltip preference survives process restart")?0:1;
    bool ok=check(settings.explanatoryTooltipsEnabled(),"tooltips enabled by default");
    TooltipPolicy policy(app);TooltipProbe probe;probe.setToolTip("Useful explanation");
    QHelpEvent hover(QEvent::ToolTip,QPoint(1,1),QPoint(1,1));
    QApplication::sendEvent(&probe,&hover);ok&=check(probe.delivered==1,"enabled tooltip event reaches control");
    settings.setExplanatoryTooltipsEnabled(false);QSettings().sync();
    QApplication::sendEvent(&probe,&hover);ok&=check(probe.delivered==1&&!probe.toolTip().isEmpty(),"disabled tooltip suppressed without deleting text or disabling control");
    QProcess restart;restart.start(app.applicationFilePath(),{"-platform","offscreen","--preference-child",temp.path()});
    const bool finished=restart.waitForFinished(10000);if(!finished){restart.kill();restart.waitForFinished();}
    ok&=check(finished&&restart.exitStatus()==QProcess::NormalExit&&restart.exitCode()==0,"persisted preference restored by new process");
    settings.setExplanatoryTooltipsEnabled(true);QApplication::sendEvent(&probe,&hover);
    ok&=check(probe.delivered==2,"re-enabled preference applies to existing controls");

    HelpDialog help;help.show();
    auto* tree=help.findChild<QTreeWidget*>();auto* search=help.findChild<QLineEdit*>();auto* browser=help.findChild<QTextBrowser*>();
    auto groups=tree->findItems("3D Printing",Qt::MatchExactly);
    ok&=check(groups.size()==1&&groups.front()->childCount()==7,"first-class printing group contains seven focused topics");
    for(const auto& info:HelpManager::topics())ok&=check(QFile::exists(info.resourcePath),"registered Help topic exists in compiled resources");
    for(const QString& term:{"printing","calibration","fit profile","LDraw","external repair","prepared mesh","Auto Fit","package","orientation"}){
        search->setText(term);int matches=0;for(int i=0;i<tree->topLevelItemCount();++i)matches+=tree->topLevelItem(i)->data(0,Qt::UserRole).isValid();
        ok&=check(matches>0,"common printing search term finds indexed topic");
        if(term=="printing")ok&=check(matches>=4,"printing search finds multiple focused topics");
    }
    help.showTopic(HelpTopic::Printing);
    const auto overview=browser->toPlainText();
    ok&=check(overview.contains("installed LDraw parts library")&&overview.contains(QString::fromUtf8("Edit → Settings → 3D Models"))&&
        overview.contains("Source-dependent printing and calibration require the necessary LDraw files to be available.")&&
        overview.indexOf("installed LDraw parts library")<overview.indexOf("BrickSuite prepares")&&
        browser->toHtml().contains("href=\"ldraw_models.html\""),"overview leads with installed LDraw prerequisite, settings path, and setup link");
    search->setText("printing");
    for(auto theme:{UserSettings::Theme::Dark,UserSettings::Theme::Light}){
        ThemeManager::applyTheme(app,theme);QApplication::processEvents();
        // Theme switches must refresh an existing search, not just a new one.
        if(browser->source().path().endsWith("printing.html")){
            ok&=check(!browser->extraSelections().isEmpty()&&browser->extraSelections().front().format.background().color()==browser->palette().color(QPalette::Highlight),"active search follows theme palette");
        }
        help.showTopic(HelpTopic::Printing);search->setText("printing");QApplication::processEvents();
        const auto highlights=browser->extraSelections();ok&=check(!highlights.isEmpty(),"search highlights remain visible");
        for(const auto& selection:highlights){
            const auto a=luminance(selection.format.foreground().color()),b=luminance(selection.format.background().color());
            ok&=check((std::max(a,b)+.05)/(std::min(a,b)+.05)>=4.5,"Dark and Light search foreground/background contrast >=4.5");
        }
        // Switching pages from search results retains the search and highlights.
        QTreeWidgetItem* printing=nullptr;
        for(int i=0;i<tree->topLevelItemCount();++i)if(tree->topLevelItem(i)->data(0,Qt::UserRole).toInt()==int(HelpTopic::FitCalibration))printing=tree->topLevelItem(i);
        ok&=check(printing,"calibration is a printing search result");
        if(printing)tree->itemActivated(printing,0);
        ok&=check(browser->source().path().endsWith("fit_calibration.html")&&!browser->extraSelections().isEmpty(),"search result navigation highlights new page");
        search->clear();ok&=check(browser->extraSelections().isEmpty(),"clearing search clears highlights");
        help.showTopic(HelpTopic::Printing);search->setText("printing");
    }
    // The existing application F1 action resolves focused controls and their
    // parent dialogs. Exercise that same context lookup with a real key event.
    QWidget window;QLineEdit field(&window);HelpManager::setContextTopic(&window,HelpTopic::FitCalibration);
    QAction f1(&window);f1.setShortcut(QKeySequence::HelpContents);f1.setShortcutContext(Qt::ApplicationShortcut);window.addAction(&f1);
    bool routed=false;
    QObject::connect(&f1,&QAction::triggered,[&]{auto context=HelpManager::context(QApplication::focusWidget());
        routed=context&&context->topic==HelpTopic::FitCalibration;if(context)help.showTopic(context->topic);});
    window.show();window.activateWindow();field.setFocus();QApplication::processEvents();
    QKeyEvent key(QEvent::KeyPress,Qt::Key_F1,Qt::NoModifier);QApplication::sendEvent(&field,&key);QApplication::processEvents();
    ok&=check(routed&&browser->source().path().endsWith("fit_calibration.html"),"F1 routes directly to dedicated calibration topic");
    QWidget unrelated;ok&=check(!HelpManager::context(&unrelated),"unrelated unassigned windows retain normal fallback");
    HelpManager::setContextTopic(&window,HelpTopic::DatabaseStatus,"integrity");
    const auto context=HelpManager::context(&field);ok&=check(context&&context->topic==HelpTopic::DatabaseStatus&&context->anchor=="integrity","unrelated existing context and anchors preserved");
    return ok?0:1;
}
