#include "../src/ui/inventory/EditInventoryDialog.h"
#include "../src/ui/inventory/RemoveInventoryDialog.h"
#include "../src/ui/inventory/RemoteInventoryMutationDialog.h"
#include "../src/ui/inventory/EditInventorySaveState.h"
#include "../src/ui/ToolsMenuLayout.h"
#include "../src/network/BrickSuiteWebSocketClient.h"
#include "../src/services/application/RemoteMutationApplicationServices.h"
#include "../src/services/application/RemoteInventoryMutationApplicationService.h"
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
    app.setApplicationName("InventoryZeroWorkflow-" + QUuid::createUuid().toString(QUuid::WithoutBraces));
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

    if (!sql("INSERT INTO inventory_record(id,workspace_id,part_id,color_id,storage_location_id,manufacturer_id,condition,ownership_type,quantity,created_utc,modified_utc) VALUES(100,100,100,100,100,1,'Used','Owned',5,CURRENT_TIMESTAMP,CURRENT_TIMESTAMP)")) return 1;
    WorkspaceContext workspace; workspace.setCurrentWorkspaceId(100);
    auto scalar = [&](const QString& statement) { sql(statement); query.next(); return query.value(0).toInt(); };
    auto quantity = [&] { return scalar("SELECT quantity FROM inventory_record WHERE id=100"); };
    bool ok = true;
    {
        EditInventoryDialog edit(100, workspace);
        edit.findChild<QSpinBox*>()->setValue(7);
        edit.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click();
        ok &= check(edit.result()==QDialog::Accepted && !edit.removedInventory() && quantity()==7,
                    "positive edit follows unchanged persistence path");
    }
    auto zeroEdit = [&](bool confirm, bool allocated) {
        EditInventoryDialog edit(100,workspace); edit.show();
        auto* spin = edit.findChild<QSpinBox*>();
        spin->findChild<QLineEdit*>()->setText("-1");
        ok &= check(!spin->hasAcceptableInput(),
                    "negative input is invalid, not a removal request");
        spin->setValue(0);
        bool opened=false, prompted=false;
        QTimer timer;
        QObject::connect(&timer,&QTimer::timeout,[&] {
            auto* removal=dynamic_cast<RemoveInventoryDialog*>(QApplication::activeModalWidget());
            if (!removal) return;
            timer.stop(); opened=true;
            auto* button=removal->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok);
            if (allocated) {
                ok &= check(!button->isEnabled(),"zero edit preserves fully allocated removal safeguard");
                removal->reject(); return;
            }
            QTimer::singleShot(0,[&] {
                if (auto* box=qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) {
                    prompted=true; box->button(confirm?QMessageBox::Yes:QMessageBox::No)->click();
                }
            });
            button->click();
            if (!confirm) removal->reject();
        });
        timer.start(1);
        edit.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click();
        timer.stop();
        ok &= check(opened,"zero edit opens the actual Remove Entry dialog");
        if (!allocated) ok &= check(prompted,"existing history-preserving confirmation shown");
        ok &= check(edit.removedInventory()==(confirm&&!allocated),"removal completion flag matches outcome");
        if (!confirm || allocated) ok &= check(edit.isVisible(),"cancel keeps Edit open");
        edit.close();
    };
    zeroEdit(false,false);
    ok &= check(quantity()==7 && scalar("SELECT COUNT(*) FROM inventory_movement WHERE movement_type='EntryRemoved'")==0,
                "declined removal leaves quantity and removal history unchanged");
    zeroEdit(true,false);
    ok &= check(quantity()==0 && scalar("SELECT COUNT(*) FROM inventory_record WHERE id=100")==1,
                "confirmed removal retains the historical inventory record with zero quantity");
    ok &= check(scalar("SELECT quantity_change FROM inventory_movement WHERE movement_type='EntryRemoved' AND inventory_record_id=100")==-7,
                "zero edit records the existing EntryRemoved movement");
    sql("UPDATE inventory_record SET quantity=3 WHERE id=100");
    sql("INSERT INTO build(id,workspace_id,build_type,name,inventory_mode,status,is_active,created_utc,modified_utc) VALUES(100,100,'MOC','Fixture','Stock','Planned',1,CURRENT_TIMESTAMP,CURRENT_TIMESTAMP)");
    sql("INSERT INTO build_allocation(build_id,inventory_record_id,part_id,color_id,storage_location_id,quantity_allocated,created_utc,modified_utc) VALUES(100,100,100,100,100,3,CURRENT_TIMESTAMP,CURRENT_TIMESTAMP)");
    zeroEdit(true,true);
    ok &= check(quantity()==3,"allocated inventory unchanged");
    ok &= check(EditInventorySaveState::canSave(true,true,0,0)
        && !EditInventorySaveState::canSave(true,false,100,-1),"zero removal does not require a loaded color list; negative remains invalid");

    BrickSuiteWebSocketClient client;
    RemoteMutationApplicationServices mutations(client);
    RemoteInventoryMutationApplicationService inventory(mutations);
    RemoteReadDto::InventoryDetail detail; detail.inventoryRecordId=100; detail.workspaceId=100;
    detail.partNumber="soak-a"; detail.rebrickableColorId=4; detail.quantity=8; detail.allocatedQuantity=2;
    detail.storageId=100; detail.manufacturerDisplay="LEGO"; detail.condition="Used"; detail.ownershipType="Owned";
    RemoteInventoryMutationDialog remote("inventory.edit",100,inventory,{}, {"LEGO"}, detail);
    remote.show(); remote.findChild<QSpinBox*>("quantitySpin")->setValue(0);
    bool remoteOpened=false;
    QTimer::singleShot(0,[&] {
        auto* removal=dynamic_cast<RemoteInventoryMutationDialog*>(QApplication::activeModalWidget());
        if (removal && removal!=&remote) {
            remoteOpened=removal->windowTitle()=="Remove Inventory Entry";
            ok &= check(removal->findChild<QSpinBox*>("quantitySpin")->maximum()==6,"Remote removal keeps Host allocation limit");
            removal->reject();
        }
    });
    remote.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();
    events();
    ok &= check(remoteOpened && remote.isVisible() && quantity()==3,"Remote zero opens Remote removal; cancellation never changes local inventory");
    remote.close();

    QMenu menu;
    auto* import=menu.addAction("Import Rebrickable Data Files...");
    auto* part=menu.addAction("Part Reference...");
    auto* viewer=menu.addMenu("3D Model Viewer");
    auto* calibration=menu.addAction("LEGO Fit Calibration...");
    auto* database=menu.addAction("Database Status && Integrity...");
    auto* lists=menu.addAction("Lists && Reference Data...");
    QList<QAction*> primary{import,part,viewer->menuAction(),calibration,database,lists};
    int callbacks=0;
    for(auto* action:primary) QObject::connect(action,&QAction::triggered,[&]{++callbacks;});
    part->setShortcut(QKeySequence("Ctrl+Alt+P"));
    ToolsMenuLayout::apply(&menu,import,part,viewer->menuAction(),calibration,database,lists);
    const QList<QAction*> expected{import,nullptr,part,nullptr,viewer->menuAction(),calibration,nullptr,database,lists};
    const auto actions=menu.actions();
    ok &= check(actions.size()==expected.size(),"Tools menu has no duplicate actions");
    for(int i=0;i<actions.size()&&i<expected.size();++i)
        ok &= check(expected[i]?actions[i]==expected[i]:actions[i]->isSeparator(),"Tools action and separator order");
    for(auto* action:primary) action->trigger();
    ok &= check(callbacks==6 && part->shortcut()==QKeySequence("Ctrl+Alt+P") && viewer->menuAction()->menu()==viewer,
                "reordering preserves original callbacks, shortcut and submenu");
    return ok?0:1;
}
