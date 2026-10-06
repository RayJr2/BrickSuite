#include "../src/ui/help/HelpDialog.h"
#include "../src/ui/about/AboutDialog.h"
#include "../src/ui/common/ThemeRichTextLabel.h"
#include "../src/ui/common/SupportLinks.h"
#include <QDesktopServices>
#include <QAbstractTextDocumentLayout>
#include <QUrl>
#include "../src/ui/help/HelpManager.h"
#include "../src/ui/common/TooltipPolicy.h"
#include "../src/settings/ThemeManager.h"
#include <QApplication>
#include <QAction>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QPushButton>
#include <QRegularExpression>
#include <QHelpEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QEventLoop>
#include <QTimer>
#include <QLineEdit>
#include <QProcess>
#include <QSettings>
#include <QTemporaryDir>
#include <QTextBrowser>
#include <QTextCursor>
#include <QTextBlock>
#include <QTextFragment>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <cmath>
#include <cstdio>

class LinkReceiver : public QObject {
    Q_OBJECT
public:
    QList<QUrl> urls;
public slots:
    void receive(const QUrl& url){urls.append(url);}
};

namespace {
void settleEvents(){QEventLoop loop;QTimer::singleShot(50,&loop,&QEventLoop::quit);loop.exec();}
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
    TooltipPolicy policy(app);TooltipProbe probe;TooltipPolicy::explain(&probe,"Useful explanation");
    QHelpEvent hover(QEvent::ToolTip,QPoint(1,1),QPoint(1,1));
    QApplication::sendEvent(&probe,&hover);ok&=check(probe.delivered==1,"enabled tooltip event reaches control");
    settings.setExplanatoryTooltipsEnabled(false);QSettings().sync();
    QApplication::sendEvent(&probe,&hover);ok&=check(probe.delivered==1&&!probe.toolTip().isEmpty(),"disabled tooltip suppressed without deleting text or disabling control");
    TooltipPolicy::preferenceChanged();
    TooltipProbe data;data.setToolTip("Full diagnostic text");
    QApplication::sendEvent(&data,&hover);
    ok&=check(data.delivered==1,"data and diagnostic tooltips remain available");
    TooltipProbe createdLater;TooltipPolicy::explain(&createdLater,"New dialog explanation");
    QApplication::sendEvent(&createdLater,&hover);
    ok&=check(createdLater.delivered==0,"newly created controls honor disabled preference");
    probe.setToolTip("Dynamic error replacing generic help");
    QApplication::sendEvent(&probe,&hover);
    ok&=check(probe.delivered==2,"dynamic diagnostic replacing help is not hidden");
    TooltipPolicy::explain(&probe,"Useful explanation");
    QLineEdit editable;TooltipPolicy::explain(&editable,"Entry explanation");
    QKeyEvent typing(QEvent::KeyPress,Qt::Key_A,Qt::NoModifier,"a");
    QApplication::sendEvent(&editable,&typing);
    ok&=check(editable.text()=="a","policy leaves keyboard entry usable");
    QPushButton cancel("Cancel");
    ok&=check(cancel.toolTip().isEmpty()&&!cancel.property("brickSuiteExplanatoryTooltip").isValid(),"obvious controls receive no automatic help");
    QProcess restart;restart.start(app.applicationFilePath(),{"-platform","offscreen","--preference-child",temp.path()});
    const bool finished=restart.waitForFinished(10000);if(!finished){restart.kill();restart.waitForFinished();}
    ok&=check(finished&&restart.exitStatus()==QProcess::NormalExit&&restart.exitCode()==0,"persisted preference restored by new process");
    settings.setExplanatoryTooltipsEnabled(true);QApplication::sendEvent(&probe,&hover);
    ok&=check(probe.delivered==3,"re-enabled preference applies to existing controls");

    // Structural integration coverage: registrations are explicit at creation, not
    // dependent on widget labels or a preference snapshot in individual dialogs.
    const QDir repository(QFileInfo(QString::fromUtf8(__FILE__)).absoluteDir().absoluteFilePath(".."));
    const auto source=[&](const QString& path){QFile file(repository.filePath(path));
        if(!file.open(QIODevice::ReadOnly)){ok&=check(false,"UI integration source readable");return QString();}
        return QString::fromUtf8(file.readAll());};
    struct Registration {const char* file;const char* control;const char* topic;};
    for(const auto& entry:{
        Registration{"inventory/AddInventoryDialog.cpp","m_rememberPartCheck","Inventory"},
        Registration{"builds/BuildsWidget.cpp","m_inventoryModeCombo","Builds"},
        Registration{"procurement/ProcurementPreviewDialog.cpp","rememberCheck","MissingParts"},
        Registration{"settings/SettingsDialog.cpp","m_apiKeyEdit","Settings"},
        Registration{"settings/SettingsDialog.cpp","m_forgetHostButton","BrickSuiteServer"},
        Registration{"parts/PartReferenceDialog.cpp","m_sendButton","PartReference"},
        Registration{"parts/LDrawModelViewerWindow.cpp","m_prepare","LDrawModels"},
        Registration{"parts/FitCalibrationDialog.cpp","m_features","FitCalibration"}}){
        const auto text=source(QString("src/ui/%1").arg(entry.file));
        ok&=check(text.contains(QString("TooltipPolicy::explain(%1,").arg(entry.control)),"representative control registers explanatory help");
        ok&=check(text.contains(QString("HelpTopic::%1").arg(entry.topic)),"representative surface has appropriate F1 topic");
    }
    const auto inventory=source("src/ui/inventory/AddInventoryDialog.cpp");
    ok&=check(!inventory.contains("TooltipPolicy::explain(m_quantitySpin,")&&!inventory.contains("TooltipPolicy::explain(m_partSearchEdit,"),"ordinary quantity/search controls stay uncluttered");
    ok&=check(source("src/ui/parts/PartReferenceDialog.cpp").contains("button->setAccessibleName("),"Part Reference cards have an explicit accessible name");
    ok&=check(source("src/ui/procurement/ProcurementPreviewDialog.cpp").contains("rememberCheck->setAccessibleName("),"unlabeled Remember checkbox has an accessible name");
    settings.setExplanatoryTooltipsEnabled(false);
    for(auto topic:{HelpTopic::Inventory,HelpTopic::Builds,HelpTopic::Settings,HelpTopic::PreparePrinting,HelpTopic::FitCalibration}){
        QWidget parent;QLineEdit child(&parent);HelpManager::setContextTopic(&parent,topic);
        const auto context=HelpManager::context(&child);
        ok&=check(context&&context->topic==topic,"focused child inherits its surface F1 context regardless of tooltip setting");
    }
    QAction action(nullptr);TooltipPolicy::explain(&action,"Action explanation");
    ok&=check(action.statusTip()==action.toolTip(),"menu action status tip agrees with explanatory help");

    for(const QColor background:{QColor(Qt::white),QColor(Qt::black),QColor("#2b2b2b")}){
        for(const QColor original:{QColor(Qt::blue),QColor(Qt::darkMagenta),QColor(Qt::cyan)}){
            const auto adjusted=ThemeContrast::readableLink(original,background);
            const double a=luminance(adjusted),b=luminance(background);
            ok&=check((std::max(a,b)+.05)/(std::min(a,b)+.05)>=4.5,"shared link helper contrast contract");
        }
    }
    LinkReceiver receiver;
    QDesktopServices::setUrlHandler("https",&receiver,"receive");
    const QUrl supportUrl(QString::fromLatin1(AppConstants::SupportUrl));
    ok&=check(supportUrl.isValid()&&supportUrl.scheme()=="https"&&supportUrl.host()=="www.paypal.com"
        &&supportUrl.path()=="/ncp/payment/WB8RKBVN6DTYW"&&supportUrl.port()==-1
        &&supportUrl.userInfo().isEmpty()&&!supportUrl.hasQuery()&&!supportUrl.hasFragment(),
        "central support URL has the exact HTTPS destination and no added data");
    QWidget menuOwner;QMenu helpMenu("Help",&menuOwner);
    auto* supportAction=SupportLinks::addHelpAction(&helpMenu,&menuOwner);
    ok&=check(helpMenu.actions().contains(supportAction)&&supportAction->text()==QString::fromUtf8("Support BrickSuite…"),
        "support action is attached to Help with the requested label");
    ok&=check(source("src/ui/MainWindow.cpp").contains("SupportLinks::addHelpAction(helpMenu, this);"),
        "main window installs the shared action in its Help menu");
    supportAction->trigger();
    ok&=check(receiver.urls.count(supportUrl)==1,"Help action uses the centralized external desktop URL");
    bool failureShown=false;
    QTimer dismissFailure;dismissFailure.setInterval(5);
    QObject::connect(&dismissFailure,&QTimer::timeout,[&]{
        if(auto* warning=qobject_cast<QMessageBox*>(QApplication::activeModalWidget())){
            failureShown=warning->icon()==QMessageBox::Warning
                &&warning->text().contains(supportUrl.toString())
                &&warning->text().contains("could not open")
                &&warning->textFormat()==Qt::PlainText
                &&warning->textInteractionFlags().testFlag(Qt::TextSelectableByKeyboard);
            warning->accept();
        }
    });
    dismissFailure.start();
    SupportLinks::openSupportPage(&menuOwner,[](const QUrl&){return false;});
    dismissFailure.stop();
    ok&=check(failureShown,"browser failure returns normally after a selectable-address warning");
    AboutDialog about;about.show();
    auto* supportLabel=about.findChild<QLabel*>("supportBrickSuiteLink");
    bool voluntaryText=false;for(auto* label:about.findChildren<QLabel*>())voluntaryText|=label->text().contains("Voluntary contributions");
    ok&=check(supportLabel&&voluntaryText
        &&supportLabel->text().contains(supportUrl.toString())
        &&supportLabel->accessibleName()=="Support BrickSuite with PayPal"
        &&supportLabel->focusPolicy()==Qt::StrongFocus
        &&supportLabel->textInteractionFlags().testFlag(Qt::LinksAccessibleByKeyboard)
        &&!supportLabel->openExternalLinks(),"About support uses shared handling and accessible keyboard link text");
    ThemeRichTextLabel provider("-");
    provider.setText("<a href=\"https://brickset.com/sets/test\">Open on Brickset</a>");
    const auto labelContrast=[&](QLabel* label){
        QTextDocument document;document.setHtml(label->text());int links=0;
        for(auto block=document.begin();block.isValid();block=block.next())
            for(auto it=block.begin();!it.atEnd();++it){const auto format=it.fragment().charFormat();
                if(!format.isAnchor()||format.anchorHref().isEmpty())continue;
                ++links;const double foreground=luminance(format.foreground().color());
                const double background=luminance(label->palette().color(label->backgroundRole()));
                ok&=check((std::max(foreground,background)+.05)/(std::min(foreground,background)+.05)>=4.5,
                    "parsed rich-label links have readable contrast");
                ok&=check(format.fontUnderline(),"rich-label links retain underline");
            }
        return links;
    };
    for(auto theme:{UserSettings::Theme::Dark,UserSettings::Theme::Light,UserSettings::Theme::Dark}){
        ThemeManager::applyTheme(app,theme);settleEvents();
        ok&=check(labelContrast(&provider)==1,"dynamically populated provider label retains its link");
        int aboutLinks=0;
        for(auto* label:about.findChildren<QLabel*>()){
            if(!label->text().contains("<a "))continue;
            aboutLinks+=labelContrast(label);
            const QString before=label->text();label->setSelection(0,7);
            ThemeManager::applyTheme(app,theme==UserSettings::Theme::Dark?UserSettings::Theme::Light:UserSettings::Theme::Dark);
            settleEvents();ok&=check(label->selectionStart()==0&&label->selectedText().size()==7,"rich-label selection survives live theme refresh");
            ThemeManager::applyTheme(app,theme);settleEvents();
            ok&=check(label->text()==before,"theme round trip preserves markup without accumulated styles");
            if(theme==UserSettings::Theme::Light){
                const auto original=QApplication::palette().color(QPalette::Link);
                const double a=luminance(original),b=luminance(label->palette().color(label->backgroundRole()));
                if((std::max(a,b)+.05)/(std::min(a,b)+.05)>=4.5)
                    ok&=check(label->text().contains(QString("color: %1").arg(original.name())),
                        "Light theme retains its readable native link color");
            }
            label->setSelection(0,0);about.activateWindow();QApplication::setActiveWindow(&about);
            // Locate the rendered anchor without relying on platform font pixels.
            QTextDocument layout;layout.setDocumentMargin(0);layout.setDefaultFont(label->font());
            layout.setHtml(label->text());layout.setTextWidth(label->contentsRect().width());
            if(label==supportLabel)ok&=check(layout.size().height()<=label->contentsRect().height(),
                "support link remains fully visible after a live theme change");
            QPoint anchor(-1,-1);
            for(int y=0;y<int(layout.size().height())&&anchor.x()<0;++y)
                for(int x=0;x<int(layout.size().width());++x)
                    if(!layout.documentLayout()->anchorAt(QPointF(x,y)).isEmpty()){anchor={x+1,y};break;}
            anchor+=label->contentsRect().topLeft()+QPoint(0,int((label->contentsRect().height()-layout.size().height())/2));
            QMouseEvent press(QEvent::MouseButtonPress,anchor,label->mapToGlobal(anchor),Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
            QMouseEvent release(QEvent::MouseButtonRelease,anchor,label->mapToGlobal(anchor),Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
            QApplication::sendEvent(label,&press);QApplication::sendEvent(label,&release);
        }
        ok&=check(aboutLinks==3,"About retains original links and adds the support link");
        QPushButton disabled("Unavailable");disabled.setEnabled(false);disabled.ensurePolished();
        const auto disabledPalette=disabled.palette();
        const double disabledText=luminance(disabledPalette.color(QPalette::Disabled,QPalette::ButtonText));
        const double disabledBase=luminance(disabledPalette.color(QPalette::Disabled,QPalette::Button));
        ok&=check((std::max(disabledText,disabledBase)+.05)/(std::min(disabledText,disabledBase)+.05)>=3.0,
            "disabled button text remains legible while distinct from enabled text");
        const auto palette=QApplication::palette();
        for(auto group:{QPalette::Active,QPalette::Inactive}){
            const double a=luminance(palette.color(group,QPalette::Highlight));
            const double b=luminance(palette.color(group,QPalette::HighlightedText));
            ok&=check((std::max(a,b)+.05)/(std::min(a,b)+.05)>=4.5,"shared text selection roles have readable contrast");
        }
    }
    ok&=check(receiver.urls.count(QUrl("https://www.gnu.org/licenses/lgpl-3.0.html"))==3&&
        receiver.urls.count(QUrl("https://rfstateside.com"))==3,"both About links activate their unchanged destinations in each theme");
    ok&=check(receiver.urls.count(supportUrl)==4,
        "About mouse activation uses the same support URL through Dark-Light-Dark changes");
    if(supportLabel){
        about.activateWindow();QApplication::setActiveWindow(&about);
        supportLabel->clearFocus();supportLabel->setFocus(Qt::TabFocusReason);settleEvents();
        // Tab selects the anchor within the focused rich-text label; Return opens it.
        QKeyEvent selectLink(QEvent::KeyPress,Qt::Key_Tab,Qt::NoModifier);
        QApplication::sendEvent(supportLabel,&selectLink);
        QKeyEvent activate(QEvent::KeyPress,Qt::Key_Return,Qt::NoModifier);
        QApplication::sendEvent(supportLabel,&activate);
        ok&=check(receiver.urls.count(supportUrl)==5,"About support link activates from the keyboard");
    }
    QDesktopServices::unsetUrlHandler("https");about.hide();

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
    // Validate the published HTML contract against the same compiled resources
    // used by the application: links, fragments and accessible image labels.
    const QRegularExpression references(R"re((href|src)="([^"]+)")re");
    const QRegularExpression imageTags(R"re(<img\b[^>]*>)re");
    const QRegularExpression altText(R"re(alt="[^"]+")re");
    for (const auto& info : HelpManager::topics()) {
        QFile page(info.resourcePath);
        ok &= check(page.open(QIODevice::ReadOnly), "Help page can be read");
        const QString html = QString::fromUtf8(page.readAll());
        auto links = references.globalMatch(html);
        while (links.hasNext()) {
            const auto match = links.next();
            const QUrl target(match.captured(2));
            if (!target.scheme().isEmpty()) continue;
            const QString path = target.path().isEmpty() ? info.resourcePath
                : QDir(QFileInfo(info.resourcePath).path()).filePath(target.path());
            QFile linked(path);
            ok &= check(linked.open(QIODevice::ReadOnly), "local Help target is in QRC");
            if (!target.fragment().isEmpty()) {
                const QString body = QString::fromUtf8(linked.readAll());
                ok &= check(body.contains("id=\"" + target.fragment() + "\"")
                    || body.contains("name=\"" + target.fragment() + "\""),
                    "local Help fragment exists");
            }
        }
        auto images = imageTags.globalMatch(html);
        while (images.hasNext())
            ok &= check(altText.match(images.next().captured()).hasMatch(),
                        "Help image has nonempty alt text");
    }
    for (const auto& destination : {
             HelpContext{HelpTopic::SetsCatalog, "use-set-for-parts"},
             HelpContext{HelpTopic::Builds, "what-can-i-build"},
             HelpContext{HelpTopic::PreparePrinting, "fit-outcomes"}}) {
        help.showTopic(destination.topic, destination.anchor);
        ok &= check(browser->source().fragment() == destination.anchor,
                    "workflow navigation preserves the requested fragment");
    }
    help.showTopic(HelpTopic::Home);
    ok &= check(!browser->toPlainText().contains("v0.3.0")
        && browser->toPlainText().contains("Support BrickSuite"),
        "Help Home describes the current application and optional support");
    help.showTopic(HelpTopic::PrintTroubleshooting);
    ok &= check(browser->toPlainText().contains("Part Reference audit mode")
        && browser->toPlainText().contains("not universal printability"),
        "reference corpus coverage remains a bounded developer diagnostic");
    help.showTopic(HelpTopic::Printing);
    const auto overview=browser->toPlainText();
    ok&=check(overview.contains("installed LDraw parts library")&&overview.contains(QString::fromUtf8("Edit → Settings → 3D Models"))&&
        overview.contains("Source-dependent printing and calibration require the necessary LDraw files to be available.")&&
        overview.indexOf("installed LDraw parts library")<overview.indexOf("BrickSuite prepares")&&
        browser->toHtml().contains("href=\"ldraw_models.html\""),"overview leads with installed LDraw prerequisite, settings path, and setup link");
    help.showTopic(HelpTopic::PrintTroubleshooting);
    const auto auditHelp=browser->toPlainText();
    ok&=check(auditHelp.contains("Automatic / No explicit selection")&&auditHelp.contains("partial fit")&&
        auditHelp.contains("exports/success/")&&auditHelp.contains("exports/diagnostic/"),
        "audit Help explains explicit intent, partial fit and diagnostic separation");
    help.showTopic(HelpTopic::Printing);
    search->setText("printing");
    for(auto theme:{UserSettings::Theme::Dark,UserSettings::Theme::Light,UserSettings::Theme::Dark}){
        ThemeManager::applyTheme(app,theme);settleEvents();
        for (const auto topic : {HelpTopic::Home, HelpTopic::Storage, HelpTopic::SetsCatalog,
                 HelpTopic::Inventory, HelpTopic::MyCollection, HelpTopic::Builds,
                 HelpTopic::MissingParts, HelpTopic::Settings, HelpTopic::BrickSuiteServer,
                 HelpTopic::Printing, HelpTopic::PreparePrinting, HelpTopic::FitCalibration,
                 HelpTopic::LocalPrintableOverride}) {
            help.showTopic(topic);
            ok &= check(!browser->toPlainText().trimmed().isEmpty()
                && browser->source().path() == HelpManager::resourcePath(topic).mid(1),
                "representative Help topic loads in both themes");
        }
        help.showTopic(HelpTopic::Printing);search->setText("printing");
        auto liveLink=browser->document()->find("LDraw 3D Models");
        browser->setTextCursor(liveLink);
        const auto liveSource=browser->source();
        ThemeManager::applyTheme(app,theme);settleEvents();
        const double linkText=luminance(liveLink.charFormat().foreground().color());
        const double articleBase=luminance(browser->palette().color(QPalette::Base));
        ok&=check((std::max(linkText,articleBase)+.05)/(std::min(linkText,articleBase)+.05)>=4.5&&
            browser->source()==liveSource&&browser->textCursor().selectedText()==liveLink.selectedText(),
            "Dark-Light-Dark refresh recolors existing links without losing selection or reloading");
        const auto tooltipPalette=QToolTip::palette();
        const double tipText=luminance(tooltipPalette.color(QPalette::ToolTipText));
        const double tipBase=luminance(tooltipPalette.color(QPalette::ToolTipBase));
        ok&=check((std::max(tipText,tipBase)+.05)/(std::min(tipText,tipBase)+.05)>=4.5,"theme tooltip palette has readable contrast");
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
        for(auto group:{QPalette::Active,QPalette::Inactive}){
            const auto palette=browser->palette();
            const double foreground=luminance(palette.color(group,QPalette::HighlightedText));
            const double background=luminance(palette.color(group,QPalette::Highlight));
            ok&=check((std::max(foreground,background)+.05)/(std::min(foreground,background)+.05)>=4.5,
                "active and inactive article selection contrast >=4.5");
        }
        // The same open document must retain links, selection and navigation
        // when its theme changes, including selected/search-highlighted links.
        for(auto topic:{HelpTopic::Home,HelpTopic::Printing}){
            help.showTopic(topic);
            auto selected=browser->document()->find(topic==HelpTopic::Home?"Getting Started":"LDraw 3D Models");
            browser->setTextCursor(selected);
            const auto source=browser->source();
            const bool backward=browser->isBackwardAvailable();
            ThemeManager::applyTheme(app,theme);settleEvents();
            ok&=check(browser->source()==source&&browser->isBackwardAvailable()==backward&&
                browser->textCursor().selectedText()==selected.selectedText(),"theme refresh preserves article selection and navigation");
            int links=0;
            for(auto block=browser->document()->begin();block.isValid();block=block.next()){
                for(auto it=block.begin();!it.atEnd();++it){
                    const auto fragment=it.fragment();const auto format=fragment.charFormat();
                    if(!format.isAnchor()||format.anchorHref().isEmpty())continue;
                    ++links;
                    const double foreground=luminance(format.foreground().color());
                    const double background=luminance(browser->palette().color(QPalette::Base));
                    ok&=check((std::max(foreground,background)+.05)/(std::min(foreground,background)+.05)>=4.5,
                        "Home and Printing links have readable theme contrast");
                    ok&=check(format.fontUnderline()&&format.foreground().color()!=browser->palette().color(QPalette::Text),
                        "links retain underline and distinct color");
                }
            }
            ok&=check(links>0,"article retains hyperlink targets");
            browser->setFocus();QApplication::processEvents();
            QTextCursor start(browser->document());browser->setTextCursor(start);
            // QTextBrowser normally enables mouse selection and keyboard link
            // navigation, not keyboard text selection. Check it only if enabled.
            if(browser->textInteractionFlags().testFlag(Qt::TextSelectableByKeyboard)){
                QKeyEvent selectWord(QEvent::KeyPress,Qt::Key_Right,Qt::ControlModifier|Qt::ShiftModifier);
                QApplication::sendEvent(browser,&selectWord);
                ok&=check(browser->textCursor().hasSelection(),"keyboard can select article text when supported");
            }
            browser->setTextCursor(start);
            const QPoint from=browser->cursorRect(start).center();
            start.movePosition(QTextCursor::NextWord);
            const QPoint to=browser->cursorRect(start).center();
            auto* viewport=browser->viewport();
            QMouseEvent press(QEvent::MouseButtonPress,from,viewport->mapToGlobal(from),Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
            QMouseEvent move(QEvent::MouseMove,to,viewport->mapToGlobal(to),Qt::NoButton,Qt::LeftButton,Qt::NoModifier);
            QMouseEvent release(QEvent::MouseButtonRelease,to,viewport->mapToGlobal(to),Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
            QApplication::sendEvent(viewport,&press);QApplication::sendEvent(viewport,&move);QApplication::sendEvent(viewport,&release);
            ok&=check(browser->textCursor().hasSelection(),"mouse drag can select article text");
            tree->setFocus();QApplication::processEvents();
            ok&=check(browser->textCursor().hasSelection(),"article selection survives focus moving to contents");
        }
        help.showTopic(HelpTopic::Printing);search->setText("printing");
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
    // Give the synthetic shortcut a deterministic focus owner even when a
    // desktop compositor declines this test window's activation request.
    window.show();window.activateWindow();settleEvents();QApplication::setActiveWindow(&window);field.setFocus();QApplication::processEvents();
    QKeyEvent key(QEvent::KeyPress,Qt::Key_F1,Qt::NoModifier);QApplication::sendEvent(&field,&key);QApplication::processEvents();
    ok&=check(routed&&browser->source().path().endsWith("fit_calibration.html"),"F1 routes directly to dedicated calibration topic");
    QWidget unrelated;ok&=check(!HelpManager::context(&unrelated),"unrelated unassigned windows retain normal fallback");
    HelpManager::setContextTopic(&window,HelpTopic::DatabaseStatus,"integrity");
    const auto context=HelpManager::context(&field);ok&=check(context&&context->topic==HelpTopic::DatabaseStatus&&context->anchor=="integrity","unrelated existing context and anchors preserved");
    return ok?0:1;
}

#include "PrintingHelpUxTest.moc"
