#include "../src/ui/catalog/SetDetailsDialog.h"
#include "../src/ui/inventory/AddInventoryDialog.h"
#include "../src/app/WorkspaceContext.h"
#include "../src/database/DatabaseManager.h"
#include "../src/services/CredentialStore.h"
#include "../src/services/sets/SetCompositionReplacementService.h"
#include "../src/services/storage/SessionStorageSelectionService.h"
#include "../src/settings/UserSettings.h"
#include "../src/settings/ThemeManager.h"
#include "../src/services/sets/SetDetailsProviderService.h"
#include "../src/ui/common/ThemeRichTextLabel.h"
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QHeaderView>
#include <QLineEdit>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QShortcut>
#include <QSpinBox>
#include <QSplitter>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <QUuid>
#include <cstdio>
#include <functional>

// Never read or modify the developer's OS credentials or use a live provider.
CredentialStore::ReadResult CredentialStore::read(const QString&) { return {true, false, {}, {}}; }
bool CredentialStore::write(const QString&, const QString&, QString*) { return false; }
bool CredentialStore::remove(const QString&, QString*) { return false; }
QString CredentialStore::backendName() { return QStringLiteral("Isolated test double"); }

namespace {
bool check(bool value, const char* message)
{
    if (!value) std::fprintf(stderr, "FAIL: %s\n", message);
    return value;
}
void events()
{
    QApplication::processEvents();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
}
struct Cleanup {
    QString path;
    ~Cleanup() { DatabaseManager::instance().close(); QDir(path).removeRecursively(); }
};
}

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    app.setOrganizationName("BrickSuiteTests");
    app.setApplicationName("SetDetailsWorkflow-" + QUuid::createUuid().toString(QUuid::WithoutBraces));
    QStandardPaths::setTestModeEnabled(true);
    QTemporaryDir settingsDirectory;
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDirectory.path());
    Cleanup cleanup{QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)};
    if (!check(DatabaseManager::instance().initialize(), "isolated database initializes")) return 1;
    QSqlQuery query(DatabaseManager::instance().database());
    const auto sql = [&](const QString& statement) {
        const bool success = query.exec(statement);
        if (!success) std::fprintf(stderr, "%s\n", qPrintable(query.lastError().text()));
        return success;
    };
    if (!sql("INSERT INTO workspace(id,name,created_utc,modified_utc) VALUES(100,'Fixture',CURRENT_TIMESTAMP,CURRENT_TIMESTAMP)")
        || !sql("INSERT INTO storage_location(id,workspace_id,location_type_id,name,is_active,created_utc,modified_utc) VALUES(100,100,1,'Fixture bin',1,CURRENT_TIMESTAMP,CURRENT_TIMESTAMP)")
        || !sql("INSERT INTO part(id,part_number,name,is_active,created_utc,modified_utc,material) VALUES(100,'soak-a','Fixture A',1,CURRENT_TIMESTAMP,CURRENT_TIMESTAMP,'Plastic'),(101,'soak-b','Fixture B',1,CURRENT_TIMESTAMP,CURRENT_TIMESTAMP,'Plastic')")
        || !sql("INSERT INTO color(id,name,rebrickable_id,created_utc,modified_utc) VALUES(100,'Fixture Red',4,CURRENT_TIMESTAMP,CURRENT_TIMESTAMP),(101,'Fixture Blue',1,CURRENT_TIMESTAMP,CURRENT_TIMESTAMP)")
        || !sql("INSERT INTO set_catalog(id,set_number,name,year,num_parts,created_utc,modified_utc) VALUES(100,'42118-1','Monster Jam Grave Digger',2021,212,CURRENT_TIMESTAMP,CURRENT_TIMESTAMP)")) return 1;
    if (!sql("UPDATE part SET rebrickable_part_id=part_number WHERE id IN (100,101)")) return 1;
    const auto composition = SetCompositionReplacementService().replace(100,
        {{"soak-a",4,9,false,"1"},{"soak-b",1,7,false,"2"}}, "Fixture", "Synthetic test");
    if (!check(composition.success, "synthetic composition created")) return 1;
    WorkspaceContext workspace;
    workspace.setCurrentWorkspaceId(100);
    SessionStorageSelectionService storage;
    storage.rememberDestination(100, 100);
    QWidget owner; owner.show();
    QPointer<SetDetailsDialog> details = new SetDetailsDialog(100, workspace, &owner);
    details->setAttribute(Qt::WA_DeleteOnClose);
    details->show(); events();
    auto* table = details->findChild<QTableWidget*>("setCompositionTable");
    auto* action = details->findChild<QAction*>("setPartAddInventoryAction");
    auto* splitter = details->findChild<QSplitter*>("setDetailsSections");
    bool ok = check(table && action && splitter, "actual Set Details exposes table, action and splitter");
    if (!ok) return 1;
    // Exercise the real provider-result presentation without credentials or HTTP.
    SetDetailsProviderService::Result enrichment;
    enrichment.setNumber = "42118-1";
    enrichment.source = SetDetailsProviderService::Source::Brickset;
    enrichment.hasEnrichment = true;
    enrichment.brickset.bricksetSetId = 30768;
    enrichment.brickset.theme = "Technic";
    enrichment.brickset.subtheme = "Monster Jam";
    enrichment.brickset.availability = "Retail";
    enrichment.brickset.rating = 3.8;
    enrichment.brickset.ratingCount = 138;
    enrichment.brickset.instructionsCount = 3;
    enrichment.brickset.bricksetUrl = "https://brickset.com/sets/42118-1";
    details->findChild<SetDetailsProviderService*>()->detailsReady(enrichment);
    events();
    auto* providerScroll = qobject_cast<QScrollArea*>(splitter->widget(0));
    auto* providerForm = qobject_cast<QFormLayout*>(providerScroll->widget()->layout());
    auto* status = qobject_cast<QLabel*>(providerForm->itemAt(1,QFormLayout::FieldRole)->widget());
    ok &= check(providerForm->fieldGrowthPolicy() == QFormLayout::ExpandingFieldsGrow
        && providerForm->rowWrapPolicy() == QFormLayout::DontWrapRows
        && providerForm->formAlignment() == (Qt::AlignLeft | Qt::AlignTop),
        "provider form expands values without style-dependent centering or row wrapping");
    ok &= check(providerScroll->widgetResizable()
        && providerScroll->widget()->sizePolicy().expandingDirections().testFlag(Qt::Horizontal),
        "provider scroll contents expand horizontally");
    for (int row = 0; row < providerForm->rowCount(); ++row)
        ok &= check(providerForm->itemAt(row,QFormLayout::FieldRole)->widget()->sizePolicy().horizontalPolicy()
            == QSizePolicy::Expanding, "every provider value receives expandable width");
    for (const QSize size : {QSize(1000,750), QSize(1200,800), QSize(1600,950)}) {
        details->resize(size); events();
        std::fprintf(stdout,"Provider layout %dx%d: pane=%d status=%d text=%d growth=%d wrap=%d alignment=%d\n",
            details->width(),details->height(),providerScroll->viewport()->width(),status->width(),
            status->fontMetrics().horizontalAdvance(status->text()),int(providerForm->fieldGrowthPolicy()),
            int(providerForm->rowWrapPolicy()),int(providerForm->formAlignment()));
        ok &= check(status->width() >= status->fontMetrics().horizontalAdvance(status->text()),
            "ordinary loaded status fits one line at normal dialog widths");
        ok &= check(providerScroll->horizontalScrollBar()->maximum() == 0,
            "provider pane needs no horizontal scrolling");
    }
    details->resize(1000,750); events();
    ok &= check(splitter->sizes().at(1) > splitter->sizes().at(0),
        "default splitter gives Catalog Parts the majority of the available height");
    const QString normalStatus = status->text();
    const QString longStatus = QStringLiteral("Provider temporarily unavailable while refreshing supplementary details. ")
        .repeated(5);
    status->setText(longStatus);
    auto* subtheme = qobject_cast<QLabel*>(providerForm->itemAt(4,QFormLayout::FieldRole)->widget());
    auto* availability = qobject_cast<QLabel*>(providerForm->itemAt(6,QFormLayout::FieldRole)->widget());
    subtheme->setText(QStringLiteral("Monster Jam special exhibition and championship collection ").repeated(3));
    availability->setText(QStringLiteral("Available through selected retailers and participating regional stores ").repeated(3));
    // A child height-for-width change posts a second LayoutRequest to the
    // scroll area; drain that propagation before inspecting final geometry.
    events(); events();
    std::fprintf(stdout,"Long provider values: status %d/%d subtheme %d/%d availability %d/%d hscroll %d\n",
        status->height(),status->heightForWidth(status->width()),subtheme->height(),subtheme->heightForWidth(subtheme->width()),
        availability->height(),availability->heightForWidth(availability->width()),providerScroll->horizontalScrollBar()->maximum());
    ok &= check(status->text() == longStatus && status->height() >= status->heightForWidth(status->width())
        && subtheme->height() >= subtheme->heightForWidth(subtheme->width())
        && availability->height() >= availability->heightForWidth(availability->width())
        && providerScroll->horizontalScrollBar()->maximum() == 0,
        "genuinely long values retain all text with enough wrapped height and no horizontal scrollbar");
    details->findChild<SetDetailsProviderService*>()->detailsReady(enrichment);
    auto* link = static_cast<ThemeRichTextLabel*>(providerForm->itemAt(10,QFormLayout::FieldRole)->widget());
    QString darkLink;
    for (auto theme : {UserSettings::Theme::Dark, UserSettings::Theme::Light, UserSettings::Theme::Dark}) {
        ThemeManager::applyTheme(app, theme); events();
        ok &= check(link->openExternalLinks() && link->text().contains(enrichment.brickset.bricksetUrl)
            && link->text().contains("<style>a { color:"), "live theme changes preserve styled external provider link");
        ok &= check(status->width() >= status->fontMetrics().horizontalAdvance(normalStatus),
            "normal status fits after live theme changes");
        if (theme == UserSettings::Theme::Dark) {
            ok &= check(darkLink.isEmpty() || darkLink == link->text(), "Dark-Light-Dark restores link markup exactly");
            darkLink = link->text();
        } else ok &= check(darkLink != link->text(), "Light theme updates provider link color");
    }
    ok &= check(!details->isModal() && !action->isEnabled(), "Set Details is modeless; no selection cannot add");
    ok &= check(splitter->orientation() == Qt::Vertical && splitter->count() == 2
        && qobject_cast<QScrollArea*>(splitter->widget(0)) && splitter->widget(1)->isAncestorOf(table),
        "scrollable provider and catalog sections share the vertical splitter");
    const int lowerBefore = splitter->sizes().at(1);
    splitter->setSizes({0, 1000}); events();
    ok &= check(splitter->sizes().at(1) > lowerBefore, "parts list gains space when provider collapses");
    details->resize(details->width(), details->height() + 100); events();
    details->showMaximized(); events(); details->showNormal(); events();
    ok &= check(table->horizontalHeader()->isVisible(), "table header survives resizing and maximize/restore");
    splitter->setSizes({0,1000});
    const QByteArray savedState = splitter->saveState();

    QPointer<AddInventoryDialog> add = new AddInventoryDialog(workspace, storage, &owner);
    add->setAttribute(Qt::WA_DeleteOnClose);
    int transferredPart = 0, transferredColor = 0;
    QObject::connect(details, &SetDetailsDialog::addInventoryRequested, &owner, [&](int part, int color) {
        transferredPart = part; transferredColor = color;
        if (add) add->setPartFromSetCatalog(part, color);
    });
    if (app.arguments().contains("--layout-preview")) {
        // Optional native inspection of the same isolated fixture and real save path.
        QObject::connect(details, &QObject::destroyed, &app, &QApplication::quit);
        const auto shortcut = [&](const char* key, const std::function<void()>& action) {
            QObject::connect(new QShortcut(QKeySequence(key),details), &QShortcut::activated, details, action);
        };
        shortcut("Ctrl+L", [&] { ThemeManager::applyTheme(app,UserSettings::Theme::Light); });
        shortcut("Ctrl+D", [&] { ThemeManager::applyTheme(app,UserSettings::Theme::Dark); });
        shortcut("Ctrl+1", [&] { details->resize(1000,750); });
        shortcut("Ctrl+2", [&] { details->resize(1200,800); });
        shortcut("Ctrl+3", [&] { details->resize(1600,950); });
        details->resize(1000,750);
        splitter->setSizes({120,360});
        return app.exec();
    }
    table->selectRow(0); action->trigger(); events();
    auto* quantity = add->findChild<QSpinBox*>("addInventoryQuantity");
    auto* color = add->findChild<QComboBox*>("addInventoryColor");
    auto* destination = add->findChild<QComboBox*>("addInventoryStorage");
    auto* partSearch = add->findChild<QLineEdit*>("addInventoryPartSearch");
    auto* keepOpen = add->findChild<QCheckBox*>("addInventoryKeepOpen");
    auto* remember = add->findChild<QCheckBox*>("addInventoryRememberPart");
    ok &= check(transferredPart == 100 && transferredColor == 100 && quantity->value() == 1
        && color->currentData().toInt() == 100, "stable Part and Color prefill; quantity is one, not nine");
    ok &= check(details->isVisible() && add->isVisible() && !add->isModal()
        && !QApplication::activeModalWidget() && owner.isEnabled(), "both actual dialogs are concurrently interactive");
    ok &= check(destination->currentData().toInt() == 100, "existing remembered leaf destination is retained");
    quantity->setValue(3);
    bool prompted = false;
    QMessageBox::StandardButton answer = QMessageBox::Cancel;
    QTimer confirmation;
    confirmation.setInterval(1);
    QObject::connect(&confirmation, &QTimer::timeout, [&] {
        if (auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) {
            prompted = true; box->button(answer)->click();
        }
    });
    confirmation.start();
    ok &= check(!add->setPartFromSetCatalog(101,101), "cancelling replacement declines new prefill");
    ok &= check(prompted && quantity->value() == 3 && color->currentData().toInt() == 100
        && partSearch->text().contains("soak-a"), "unsaved Part, Color and quantity survive cancellation");
    answer = QMessageBox::Yes;
    ok &= check(add->setPartFromSetCatalog(101,101) && quantity->value() == 1
        && color->currentData().toInt() == 101, "confirmed replacement uses exact second row identities");
    confirmation.stop();
    keepOpen->setChecked(true); remember->setChecked(true); quantity->setValue(2);
    int added = 0;
    QObject::connect(add, &AddInventoryDialog::inventoryAdded, [&] { ++added; });
    const int selectedRow = table->currentRow();
    const int scroll = table->verticalScrollBar()->value();
    auto* buttons = add->findChild<QDialogButtonBox*>();
    // Existing Add button role differs from the selected-Part constructor.
    QPushButton* save = nullptr;
    for (auto* button : buttons->buttons())
        if (buttons->buttonRole(button) == QDialogButtonBox::AcceptRole) save = qobject_cast<QPushButton*>(button);
    if (!check(save && save->isEnabled(), "normal Add submission is available")) return 1;
    save->click(); events();
    ok &= check(added == 1 && add->isVisible() && details->isVisible()
        && partSearch->text().contains("soak-b"), "real save honors Keep Open and Remember Part");
    ok &= check(table->currentRow() == selectedRow && table->verticalScrollBar()->value() == scroll,
        "inventory save leaves Set selection and scroll untouched");
    ok &= check(sql("SELECT quantity FROM inventory_record WHERE part_id=101 AND color_id=101 AND workspace_id=100 AND storage_location_id=100")
        && query.next() && query.value(0).toInt() == 2, "normal inventory persistence uses chosen identities and quantity");
    ok &= check(sql("SELECT SUM(quantity_required) FROM set_catalog_part WHERE set_catalog_id=100")
        && query.next() && query.value(0).toInt() == 16, "saving Inventory does not mutate catalog composition");
    remember->setChecked(false); save->click(); events();
    ok &= check(added == 2 && partSearch->text().isEmpty(), "Keep Open clears Part when Remember Part is off");
    add->setPartFromReference("soak-a");
    ok &= check(partSearch->text().contains("soak-a"), "existing Part Reference prefill still works");
    details->close(); events();
    ok &= check(!details && add && add->isVisible(), "closing Set Details deletes it without invalidating Add Inventory");
    ok &= check(QSettings().value("SetDetails/sectionsSplitterState").toByteArray() == savedState,
        "splitter state saved on close");
    details = new SetDetailsDialog(100,workspace,&owner);details->setAttribute(Qt::WA_DeleteOnClose);details->show();events();
    splitter = details->findChild<QSplitter*>("setDetailsSections");
    ok &= check(splitter->sizes().at(0) == 0 && splitter->sizes().at(1) > 0, "splitter position restored on reopen");
    details->resize(1000,750); events();
    splitter->setSizes({80,240}); events();
    const QByteArray expandedState = splitter->saveState();
    const auto expandedSizes = splitter->sizes();
    details->close(); events();
    ok &= check(QSettings().value("SetDetails/sectionsSplitterState").toByteArray() == expandedState,
        "non-collapsed splitter state is saved without changing the settings key");
    details = new SetDetailsDialog(100,workspace,&owner);details->setAttribute(Qt::WA_DeleteOnClose);
    details->resize(1000,750);details->show();events();
    splitter = details->findChild<QSplitter*>("setDetailsSections");
    ok &= check(splitter->sizes() == expandedSizes, "non-collapsed splitter position restored at the same window size");
    add->close(); events();
    ok &= check(!add && details && details->isVisible(), "closing Add Inventory leaves Set Details usable");
    {
        AddInventoryDialog catalogLaunch(100,workspace,storage,&owner);
        ok &= check(catalogLaunch.findChild<QSpinBox*>("addInventoryQuantity")->value() == 1,
            "existing Parts Catalog constructor retains normal quantity default");
    }
    UserSettings::instance().setSharedDataSource(SharedDataSource::BrickSuiteHost);
    table = details->findChild<QTableWidget*>("setCompositionTable");table->selectRow(0);
    action = details->findChild<QAction*>("setPartAddInventoryAction");
    ok &= check(!action->isEnabled() && action->toolTip().contains("local catalog identities"),
        "Remote row action is explicitly unavailable rather than writing locally");
    details->close(); events();
    return ok ? 0 : 1;
}
