#include "../src/database/DatabaseManager.h"
#include "../src/network/BrickSuiteWebSocketClient.h"
#include "../src/services/application/RemoteMutationApplicationServices.h"
#include "../src/services/application/RemoteInventoryMutationApplicationService.h"
#include "../src/ui/inventory/RemoteInventoryMutationDialog.h"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QSqlQuery>
#include <QStandardPaths>
#include <cstdio>
namespace { bool check(bool v,const char*m){if(!v)std::fprintf(stderr,"%s\n",m);return v;} }
int main(int argc,char**argv)
{
    QApplication app(argc,argv);QStandardPaths::setTestModeEnabled(true);
    bool ok=check(DatabaseManager::instance().initialize(),"database initialization failed");
    QSqlQuery q(DatabaseManager::instance().database());
    q.exec("INSERT OR IGNORE INTO color(name,rgb,is_transparent,rebrickable_id,created_utc,modified_utc) VALUES('Red','C91A09',0,4,CURRENT_TIMESTAMP,CURRENT_TIMESTAMP)");
    BrickSuiteWebSocketClient client;RemoteMutationApplicationServices mutations(client);
    RemoteInventoryMutationApplicationService inventory(mutations);
    RemoteReadDto::InventoryDetail d;d.inventoryRecordId=7;d.workspaceId=1;d.partNumber="3001";
    d.partNameFallback="Brick 2 x 4";d.rebrickableColorId=4;d.colorNameFallback="Red";
    d.quantity=6;d.allocatedQuantity=2;d.storageId=10;d.storagePath="Room / Bin";
    d.manufacturerDisplay="LEGO";d.condition="Used";d.ownershipType="Owned";d.modifiedUtc=QDateTime::currentDateTimeUtc();
    const QHash<int,QString> paths{{10,"Room / Bin"},{11,"Room / Other"}};
    RemoteInventoryMutationDialog edit("inventory.edit",1,inventory,paths,d);
    edit.show();app.processEvents();
    ok&=check(edit.windowTitle()=="Edit Inventory","Edit title mismatch");
    ok&=check(edit.findChild<QComboBox*>("colorCombo")!=nullptr,"Edit named Color combo missing");
    ok&=check(edit.findChild<QCheckBox*>("showAllColorsCheck")->isVisible(),"Edit Show All Colors missing");
    ok&=check(edit.findChild<QLineEdit*>("notesEdit")==nullptr,"Edit unexpectedly exposes Notes");
    ok&=check(edit.findChild<QSpinBox*>("quantitySpin")->value()==6,"Edit quantity does not match Host state");
    ok&=check(edit.findChild<QSpinBox*>("quantitySpin")->minimum()==0
              && edit.findChild<QSpinBox*>("quantitySpin")->maximum()==RemoteInventoryMutationDto::MaximumQuantity,
              "Edit quantity range does not match local Edit semantics");
    for(auto* field:edit.findChildren<QLineEdit*>())
        ok&=check(field->text()!=QStringLiteral("4"),"Edit exposes raw Rebrickable Color ID");
    RemoteInventoryMutationDialog move("inventory.move",1,inventory,paths,d);
    move.show();app.processEvents();
    auto* destination=move.findChild<QComboBox*>("destinationCombo");
    ok&=check(destination&&destination->findData(10)<0&&destination->findData(11)>=0,"Move destination identity/exclusion failed");
    ok&=check(move.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->text()=="Move","Move action wording mismatch");
    ok&=check(move.findChild<QCheckBox*>("showAllColorsCheck")==nullptr,"Move retains Show All Colors");
    ok&=check(move.findChild<QLineEdit*>("notesEdit")==nullptr,"Move unexpectedly exposes Notes");
    RemoteInventoryMutationDialog correct("inventory.correct",1,inventory,paths,d);
    correct.show();app.processEvents();
    ok&=check(correct.findChild<QLineEdit*>("replacementPartEdit")->isVisible(),"Correct Part field missing");
    ok&=check(correct.findChild<QLineEdit*>("notesEdit")->isVisible(),"Correct Notes field missing");
    ok&=check(correct.findChild<QCheckBox*>("showAllColorsCheck")==nullptr,"Correct retains Show All Colors");
    RemoteInventoryMutationDialog remove("inventory.remove",1,inventory,paths,d);
    remove.show();app.processEvents();
    bool allocationShown=false;for(auto* label:remove.findChildren<QLabel*>())allocationShown|=label->text().contains("Allocated to Builds: 2");
    ok&=check(allocationShown,"Remove allocation context missing");
    ok&=check(remove.findChild<QLineEdit*>("notesEdit")->isVisible(),"Remove Notes field missing");
    RemoteInventoryMutationDialog markLost("inventory.markLost",1,inventory,paths,d);
    markLost.show();app.processEvents();
    ok&=check(markLost.findChild<QLineEdit*>("notesEdit")->isVisible(),"Mark Lost Notes field missing");
    ok&=check(markLost.findChild<QComboBox*>("destinationCombo")==nullptr,"Mark Lost retains Destination");
    RemoteReadDto::LostInventoryRow lost;lost.partNumber="3001";lost.partNameFallback="Brick 2 x 4";
    lost.rebrickableColorId=4;lost.colorNameFallback="Red";lost.outstandingQuantity=3;lost.lastStorageId=10;
    lost.lastStoragePath="Room / Bin";lost.condition="Used";lost.ownershipType="Owned";
    RemoteInventoryMutationDialog found(1,inventory,paths,lost);
    found.show();app.processEvents();
    ok&=check(found.windowTitle()=="Found / Return Inventory","Found title mismatch");
    ok&=check(found.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->text()=="Return to Inventory","Found action wording mismatch");
    ok&=check(found.findChild<QComboBox*>("destinationCombo")->isVisible(),"Found destination missing");
    ok&=check(found.findChild<QLineEdit*>("notesEdit")->isVisible(),"Found Notes field missing");
    const QList<QDialog*> dialogs{&edit,&move,&correct,&remove,&markLost,&found};
    for(auto* dialog:dialogs){
        for(auto* child:dialog->findChildren<QWidget*>(QString(),Qt::FindDirectChildrenOnly))
            ok&=check(!child->isVisible()||dialog->layout()->indexOf(child)>=0,"Visible direct child is not managed by the active layout");
    }
    DatabaseManager::instance().close();
    return ok?0:1;
}
