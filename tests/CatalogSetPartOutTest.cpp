/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 *
 * This file is part of BrickSuite.
 *
 * BrickSuite is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, version 3 of the License.
 *
 * BrickSuite is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with BrickSuite. If not, see <https://www.gnu.org/licenses/>.
 */

#include "../src/services/inventory/CatalogSetPartOutService.h"
#include "../src/services/sets/SetCompositionReplacementService.h"
#include "../src/services/application/HostOperationalGate.h"
#include "../src/services/CredentialStore.h"
#include "../src/repositories/InventoryBuildabilityRepository.h"
#include "../src/repositories/StorageLocationTypeRepository.h"
#include "../src/ui/catalog/PartOutSetDialog.h"
#include "../src/ui/help/HelpManager.h"
#include "../src/ui/catalog/SetDetailsDialog.h"
#include "../src/ui/catalog/SetsCatalogWidget.h"
#include "../src/app/WorkspaceContext.h"
#include "../src/database/DatabaseManager.h"
#include "../src/settings/UserSettings.h"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QElapsedTimer>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QPixmap>
#include <QSettings>
#include <QSpinBox>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardItemModel>
#include <QStandardPaths>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>
#include <QUuid>
#include <functional>
#include <cstdio>

// Never access real credentials, provider APIs, or the user's database.
CredentialStore::ReadResult CredentialStore::read(const QString&) { return {true,false,{},{}}; }
bool CredentialStore::write(const QString&,const QString&,QString*) { return false; }
bool CredentialStore::remove(const QString&,QString*) { return false; }
QString CredentialStore::backendName() { return "Isolated test double"; }

namespace {
int failures=0;
void check(bool value,const char* label)
{ if (!value) { ++failures; std::fprintf(stderr,"FAIL: %s\n",label); } }
bool waitFor(const std::function<bool()>& ready)
{
    QElapsedTimer timer; timer.start();
    while (!ready() && timer.elapsed()<10000) { QApplication::processEvents(); QThread::msleep(1); }
    return ready();
}
QString uuid() { return QUuid::createUuid().toString(QUuid::WithoutBraces); }
struct Cleanup {
    QString path;
    ~Cleanup() { DatabaseManager::instance().close(); QDir(path).removeRecursively(); }
};
}

int main(int argc,char** argv)
{
    QApplication app(argc,argv); app.setQuitOnLastWindowClosed(false);
    app.setOrganizationName("BrickSuiteTests"); app.setApplicationName("CatalogPartOut-"+uuid());
    QStandardPaths::setTestModeEnabled(true);
    QTemporaryDir settings;
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settings.path());
    Cleanup cleanup{QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)};
    if (!DatabaseManager::instance().initialize()) return 1;
    const auto db=DatabaseManager::instance().database();
    const auto sql=[&](const QString& statement) {
        QSqlQuery query(db); const bool ok=query.exec(statement);
        if (!ok) std::fprintf(stderr,"SQL: %s\n%s\n",qPrintable(statement),qPrintable(query.lastError().text()));
        check(ok,"fixture SQL"); return ok;
    };
    const auto scalar=[&](const QString& statement) {
        QSqlQuery query(db); if (!query.exec(statement) || !query.next()) { check(false,"scalar query"); return QVariant(); }
        return query.value(0);
    };
    const auto stock=[&] { return scalar("SELECT COALESCE(SUM(quantity),0) FROM inventory_record").toLongLong(); };
    const auto count=[&](const QString& table) { return scalar("SELECT COUNT(*) FROM "+table).toInt(); };
    int bin=0,tray=0,box=0;
    for (const auto& type:StorageLocationTypeRepository(db).getActive()) {
        if (type.name()=="Bin") bin=type.id();
        if (type.name()=="Tray") tray=type.id();
        if (type.name()=="Box") box=type.id();
    }
    if (!bin || !tray || !box) return 1;
    if (!sql("INSERT INTO workspace(id,name,created_utc,modified_utc) VALUES(100,'Part-out fixture',CURRENT_TIMESTAMP,CURRENT_TIMESTAMP),(101,'Other fixture',CURRENT_TIMESTAMP,CURRENT_TIMESTAMP)")
        || !sql(QString("INSERT INTO storage_location(id,workspace_id,location_type_id,name,is_active,created_utc,modified_utc) VALUES(100,100,%1,'Fixture bin',1,CURRENT_TIMESTAMP,CURRENT_TIMESTAMP),(101,100,%1,'Empty parent',1,CURRENT_TIMESTAMP,CURRENT_TIMESTAMP),(102,101,%1,'Other workspace',1,CURRENT_TIMESTAMP,CURRENT_TIMESTAMP)").arg(bin))
        || !sql("INSERT INTO part(id,part_number,rebrickable_part_id,name,is_active,created_utc,modified_utc,material) VALUES(100,'part-out-a','part-out-a','Fixture A',1,CURRENT_TIMESTAMP,CURRENT_TIMESTAMP,'Plastic'),(101,'part-out-b','part-out-b','Fixture B',1,CURRENT_TIMESTAMP,CURRENT_TIMESTAMP,'Plastic')")
        || !sql("INSERT INTO color(id,name,rebrickable_id,created_utc,modified_utc) VALUES(100,'Fixture Red',4,CURRENT_TIMESTAMP,CURRENT_TIMESTAMP),(101,'Fixture Blue',1,CURRENT_TIMESTAMP,CURRENT_TIMESTAMP)")
        || !sql("INSERT INTO set_catalog(id,set_number,name,year,num_parts,created_utc,modified_utc) VALUES(100,'part-out-1','Synthetic parts box',2026,10,CURRENT_TIMESTAMP,CURRENT_TIMESTAMP)")) return 1;
    check(SetCompositionReplacementService().replace(100,{{"part-out-a",4,8,false,""},{"part-out-a",4,1,true,""},{"part-out-b",1,2,false,""}},"Fixture","Synthetic test").success,"fallback composition");
    CatalogSetPartOutService service(db);
    CatalogSetPartOutService::Request r;
    r.workspaceId=100; r.setCatalogId=100; r.createStorage=false; r.storageId=100; r.operationId=uuid();
    auto plan=service.preview(r);
    check(plan.success && plan.rows.size()==3 && plan.requiredPieces==10 && plan.sparePieces==1 && plan.totalPieces==11,"fallback exact rows, defaults and spare totals");
    check(stock()==0 && count("collection_item")==0,"preview creates no stock or Collection");
    r.copies=2; check(service.preview(r).totalPieces==22,"copy multiplier includes spares");
    r.includeSpares=false; check(service.preview(r).totalPieces==20,"spares excluded before inventory aggregation");
    r.copies=1; r.includeSpares=true;
    plan=service.preview(r); const auto first=service.execute(r,plan.fingerprint);
    if (!first.success) std::fprintf(stderr,"Execute: %s\n",qPrintable(first.message));
    check(first.success && stock()==11 && count("inventory_record")==2,"required and spare identities consolidate");
    check(count("collection_item")==0 && count("build")==0,"no Collection or Build created");
    sql("INSERT INTO build(workspace_id,build_type,name,status,created_utc,modified_utc) VALUES(100,'MOC','Existing Build','Planned','2026-01-01','2026-01-01')");
    check(scalar("SELECT quantity FROM inventory_record WHERE part_id=100 AND color_id=100").toInt()==9,"exact Part/Color stock");
    check(scalar("SELECT COUNT(*) FROM inventory_record WHERE condition='Used' AND ownership_type='Owned' AND manufacturer_id=(SELECT id FROM manufacturer WHERE name='LEGO')").toInt()==2,"Used Owned LEGO provenance");
    check(scalar("SELECT COUNT(*) FROM inventory_movement WHERE reference_type='SetCatalog' AND reference_id='100' AND movement_type IN ('InitialAdd','QuantityIncrease') AND notes LIKE '%copies: 1; include spares: yes; operation:%' AND notes LIKE '%Catalog Parts List fallback%'").toInt()==3,"normal movement provenance for all input rows");
    const int movements=count("inventory_movement");
    check(service.execute(r,plan.fingerprint).replayed && stock()==11 && count("inventory_movement")==movements,"same UUID durable replay adds nothing");
    r.copies=2; check(!service.execute(r,plan.fingerprint).success,"UUID cannot be reused for a different request");
    r.copies=1; r.operationId=uuid(); plan=service.preview(r);
    check(service.execute(r,plan.fingerprint).success && stock()==22,"intentional new UUID adds another physical copy");
    r.condition="New"; r.operationId=uuid(); plan=service.preview(r);
    check(service.execute(r,plan.fingerprint).success && count("inventory_record")==4,"New stock kept distinct from Used");
    r.condition="Used";
    InventoryBuildabilitySearch search; search.workspaceId=100; search.exactSetCatalogId=100;
    search.minimumSetParts=0; search.minimumPercent=0; search.includeCollection=false;
    const auto buildability=InventoryBuildabilityRepository(db).search(search);
    check(buildability.success && buildability.sets.size()==1 && buildability.sets.first().looseSatisfiedQuantity==10
        && buildability.sets.first().missingQuantity==0,"What Can I Build authoritative loose-stock calculation sees part-out");

    // Fingerprints, overflow, and malformed raw rows must fail before mutation.
    r.operationId=uuid(); plan=service.preview(r);
    sql("UPDATE set_catalog_part SET quantity_required=3 WHERE part_id=101");
    check(!service.execute(r,plan.fingerprint).success && stock()==33,"stale composition blocks execution");
    sql("UPDATE set_catalog_part SET quantity_required=2 WHERE part_id=101");
    sql("UPDATE set_catalog_part SET quantity_required=2147483647 WHERE part_id=100 AND is_spare=0");
    r.copies=2; check(!service.preview(r).success,"per-row multiply overflow");
    r.copies=1; check(!service.preview(r).success,"required plus spare aggregate overflow");
    sql("UPDATE set_catalog_part SET quantity_required=8 WHERE part_id=100 AND is_spare=0");
    sql("UPDATE inventory_record SET quantity=2147483647 WHERE part_id=100 AND condition='Used'");
    check(!service.preview(r).success,"existing plus added overflow");
    sql("UPDATE inventory_record SET quantity=18 WHERE part_id=100 AND condition='Used'");
    sql("UPDATE part SET is_active=0 WHERE id=100");
    check(!service.preview(r).success,"inactive exact Part blocked rather than replaced");
    sql("UPDATE part SET is_active=1 WHERE id=100");
    sql("PRAGMA foreign_keys=OFF");
    sql("UPDATE set_catalog_part SET part_id=9999 WHERE part_id=101");
    check(!service.preview(r).success,"missing Part hidden by effective-reader join blocked");
    sql("UPDATE set_catalog_part SET part_id=101 WHERE part_id=9999");
    sql("UPDATE set_catalog_part SET color_id=9999 WHERE part_id=101");
    check(!service.preview(r).success,"missing Color hidden by effective-reader join blocked");
    sql("UPDATE set_catalog_part SET color_id=101 WHERE part_id=101"); sql("PRAGMA foreign_keys=ON");
    sql("PRAGMA ignore_check_constraints=ON"); sql("UPDATE set_catalog_part SET quantity_required=0 WHERE part_id=101");
    check(!service.preview(r).success,"zero quantity blocked");
    sql("UPDATE set_catalog_part SET quantity_required=-1 WHERE part_id=101"); check(!service.preview(r).success,"negative quantity blocked");
    sql("UPDATE set_catalog_part SET quantity_required=2 WHERE part_id=101"); sql("PRAGMA ignore_check_constraints=OFF");

    sql("INSERT INTO set_inventory_revision(id,provider,external_inventory_id,set_catalog_id,version,is_active,is_preferred,created_utc,modified_utc) VALUES(100,'Rebrickable','fixture-100',100,2,1,1,CURRENT_TIMESTAMP,CURRENT_TIMESTAMP)");
    check(!service.preview(r).success,"empty preferred revision never falls back");
    sql("INSERT INTO set_inventory_part(set_inventory_revision_id,part_id,color_id,quantity,is_spare,created_utc,modified_utc) VALUES(100,101,101,3,0,CURRENT_TIMESTAMP,CURRENT_TIMESTAMP)");
    plan=service.preview(r);
    check(plan.success && plan.rows.size()==1 && plan.totalPieces==3 && plan.rows.first().partId==101 && plan.source.contains("version 2"),"preferred revision overrides fallback without identity substitution");
    r.operationId=uuid(); const auto preferredBefore=stock();
    check(service.execute(r,plan.fingerprint).success && stock()==preferredBefore+3,"execution uses only preferred revision rows");
    sql("INSERT INTO set_inventory_contained_set(set_inventory_revision_id,contained_set_catalog_id,quantity,created_utc,modified_utc) VALUES(100,100,1,CURRENT_TIMESTAMP,CURRENT_TIMESTAMP)");
    check(!service.preview(r).success && service.preview(r).message.contains("nested"),"nested Set coverage blocked");
    sql("DELETE FROM set_inventory_contained_set");
    sql("INSERT INTO minifig_catalog(id,name,created_utc,modified_utc) VALUES(100,'Fixture',CURRENT_TIMESTAMP,CURRENT_TIMESTAMP)");
    sql("INSERT INTO set_inventory_minifig(set_inventory_revision_id,minifig_catalog_id,quantity,created_utc,modified_utc) VALUES(100,100,1,CURRENT_TIMESTAMP,CURRENT_TIMESTAMP)");
    check(!service.preview(r).success,"nested Minifig coverage blocked");
    sql("DELETE FROM set_inventory_minifig"); sql("UPDATE set_inventory_revision SET is_active=0,is_preferred=0 WHERE id=100");

    // Destination and Collection safeguards.
    r.storageId=102; check(!service.preview(r).success,"wrong-workspace destination blocked"); r.storageId=100;
    sql("UPDATE storage_location SET is_active=0 WHERE id=100"); check(!service.preview(r).success,"inactive destination blocked");
    sql("UPDATE storage_location SET is_active=1 WHERE id=100");
    sql("INSERT INTO collection_item(workspace_id,item_type,set_catalog_id,state,condition,completeness,is_active,storage_location_id,created_utc,modified_utc) VALUES(100,'Set',100,'Assembled','Used','Complete',1,101,CURRENT_TIMESTAMP,CURRENT_TIMESTAMP)");
    check(!service.preview(r).warnings.isEmpty(),"intact Collection copy warns without blocking");
    const QString collectionBefore=scalar("SELECT state||modified_utc FROM collection_item").toString();
    r.createStorage=true; r.storageId=0; r.storageTypeId=bin; r.storageName="Synthetic parts box"; r.parentStorageId=100;
    check(!service.preview(r).success,"Inventory-occupied parent blocked");
    r.parentStorageId=101; check(!service.preview(r).success,"Collection-occupied parent blocked");
    sql("UPDATE collection_item SET storage_location_id=NULL");
    plan=service.preview(r); r.operationId=uuid();
    const auto created=service.execute(r,plan.fingerprint);
    check(created.success && created.storageCreated && scalar(QString("SELECT location_type_id FROM storage_location WHERE id=%1").arg(created.storageId)).toInt()==bin,"new Bin under valid parent created atomically");
    check(scalar("SELECT state||modified_utc FROM collection_item").toString()==collectionBefore && count("collection_item")==1,"existing Collection state/history preserved");
    check(!service.preview(r).warnings.isEmpty(),"duplicate sibling name warning");
    r.storageTypeId=tray; r.operationId=uuid(); plan=service.preview(r);
    const auto alternate=service.execute(r,plan.fingerprint);
    check(alternate.success && alternate.storageId!=created.storageId && scalar(QString("SELECT location_type_id FROM storage_location WHERE id=%1").arg(alternate.storageId)).toInt()==tray,"alternate active type and duplicate-name location are independent");
    auto existing=r; existing.createStorage=false; existing.storageId=101;
    check(!service.preview(existing).success,"non-leaf inventory destination blocked");
    existing.storageId=100;
    sql("UPDATE storage_location SET allows_inventory=0 WHERE id=100");
    check(!service.preview(existing).success,"destination must permit Inventory");
    sql("UPDATE storage_location SET allows_inventory=1 WHERE id=100");
    r.parentStorageId=102; check(!service.preview(r).success,"wrong-workspace parent blocked");
    r.parentStorageId=101; r.storageTypeId=9999; check(!service.preview(r).success,"missing Storage type blocked");
    r.storageTypeId=tray; r.storageName=""; check(!service.preview(r).success,"empty Storage name blocked");
    r.storageName="Two copies without spares"; r.copies=2; r.includeSpares=false; r.operationId=uuid();
    const auto multipleBefore=stock(); plan=service.preview(r);
    check(service.execute(r,plan.fingerprint).success && stock()==multipleBefore+20,"multiple physical copies with spares excluded execute correctly");
    r.copies=1; r.includeSpares=true;

    r.storageTypeId=box; r.storageName="8727-1 - Synthetic Box Fixture"; r.operationId=uuid();
    plan=service.preview(r);
    const auto boxed=service.execute(r,plan.fingerprint);
    check(boxed.success && boxed.storageCreated && scalar(QString("SELECT location_type_id FROM storage_location WHERE id=%1").arg(boxed.storageId)).toInt()==box,
        "new Box storage commits with part-out");
    check(scalar(QString("SELECT SUM(quantity) FROM inventory_record WHERE storage_location_id=%1").arg(boxed.storageId)).toInt()==11,
        "part-out Inventory references the new Box");
    auto reuseBox=r; reuseBox.createStorage=false; reuseBox.storageId=boxed.storageId; reuseBox.operationId=uuid();
    const auto reusePlan=service.preview(reuseBox);
    const int storageBeforeReuse=count("storage_location");
    check(reusePlan.success && service.execute(reuseBox,reusePlan.fingerprint).success
        && count("storage_location")==storageBeforeReuse
        && scalar(QString("SELECT SUM(quantity) FROM inventory_record WHERE storage_location_id=%1").arg(boxed.storageId)).toInt()==22,
        "existing active leaf Box is reusable without special restrictions");

    // Fail after an earlier row and new Storage have been written.
    const auto beforeStock=stock(); const int beforeStorage=count("storage_location"),beforeMoves=count("inventory_movement"),beforeReceipts=count("remote_mutation_receipt");
    for (const QString table:{QString("inventory_record"),QString("inventory_movement")}) {
        r.operationId=uuid(); r.storageName="Must roll back"; plan=service.preview(r);
        sql(QString("CREATE TRIGGER part_out_failure BEFORE INSERT ON %1 WHEN NEW.part_id=101 BEGIN SELECT RAISE(ABORT,'injected late failure'); END").arg(table));
        check(!service.execute(r,plan.fingerprint).success,"injected late failure rejected");
        check(stock()==beforeStock && count("storage_location")==beforeStorage && count("inventory_movement")==beforeMoves && count("remote_mutation_receipt")==beforeReceipts,"Inventory, movements, new Storage and receipt all roll back");
        sql("DROP TRIGGER part_out_failure");
    }
    r.operationId=uuid(); plan=service.preview(r);
    sql("CREATE TRIGGER part_out_receipt_failure BEFORE INSERT ON remote_mutation_receipt BEGIN SELECT RAISE(ABORT,'injected receipt failure'); END");
    check(!service.execute(r,plan.fingerprint).success && stock()==beforeStock && count("storage_location")==beforeStorage
        && count("inventory_movement")==beforeMoves,"receipt failure rolls back the entire operation");
    sql("DROP TRIGGER part_out_receipt_failure");
    UserSettings::instance().setSharedDataSource(SharedDataSource::BrickSuiteHost);
    check(!service.preview(r).success && !service.execute(r,plan.fingerprint).success,"Remote cannot write locally");
    UserSettings::instance().setSharedDataSource(SharedDataSource::ThisComputer);
    HostOperationalGate::setLocalWritesAllowed(false); check(!service.preview(r).success,"Host maintenance blocks writes"); HostOperationalGate::setLocalWritesAllowed(true);
    sql("UPDATE workspace SET is_active=0 WHERE id=100"); check(!service.preview(r).success,"inactive workspace blocked"); sql("UPDATE workspace SET is_active=1 WHERE id=100");

    // Exercise the actual widgets and worker-owned connections without live data.
    WorkspaceContext workspace; workspace.setCurrentWorkspaceId(100);
    {
        SetDetailsDialog details(100,workspace);
        auto* action=details.findChild<QPushButton*>("setPartOutButton"); int selected=0;
        QObject::connect(&details,&SetDetailsDialog::partOutRequested,[&](int id){selected=id;});
        check(action && action->isEnabled(),"Set Details exposes enabled local Set action"); if (action) action->click();
        check(selected==100,"Set Details action retains exact catalog identity");
        SetsCatalogWidget catalog(workspace); bool found=false;
        for (auto* combo:catalog.findChildren<QComboBox*>()) {
            const int index=combo->findData("partOut"); if (index<0) continue;
            found=true; check(qobject_cast<QStandardItemModel*>(combo->model())->item(index)->isEnabled(),"Catalog local action enabled");
        }
        check(found,"Sets Catalog row action present");
    }
    {
        UserSettings::instance().setSharedDataSource(SharedDataSource::BrickSuiteHost);
        SetDetailsDialog details(100,workspace);
        check(!details.findChild<QPushButton*>("setPartOutButton")->isEnabled(),"Remote Set Details action disabled");
        SetsCatalogWidget catalog(workspace); bool found=false;
        for (auto* combo:catalog.findChildren<QComboBox*>()) {
            const int index=combo->findData("partOut"); if (index<0) continue;
            const auto* item=qobject_cast<QStandardItemModel*>(combo->model())->item(index);
            found=true; check(!item->isEnabled() && item->toolTip().contains("Host/local"),"Remote Catalog action disabled with explanation");
        }
        check(found,"Remote Catalog preserves visible action");
        UserSettings::instance().setSharedDataSource(SharedDataSource::ThisComputer);
    }
    {
        PartOutSetDialog dialog(100,100,false); dialog.show();
        const auto helpContext=HelpManager::context(&dialog);
        check(helpContext && helpContext->topic==HelpTopic::SetsCatalog
            && helpContext->anchor=="use-set-for-parts","part-out F1 targets its workflow section");
        auto* buttons=dialog.findChild<QDialogButtonBox*>(); auto* confirm=buttons->button(QDialogButtonBox::Ok);
        check(dialog.windowTitle()=="Part Out Set to Inventory","dialog title");
        check(dialog.findChild<QSpinBox*>("partOutCopies")->value()==1 && dialog.findChild<QCheckBox*>("partOutSpares")->isChecked()
            && dialog.findChild<QComboBox*>("partOutCondition")->currentText()=="Used","copies/spares/condition defaults");
        check(dialog.findChild<QComboBox*>("partOutStorageType")->currentText()=="Bin"
            && dialog.findChild<QComboBox*>("partOutParent")->currentData().toInt()==0
            && dialog.findChild<QLineEdit*>("partOutStorageName")->text()=="part-out-1 - Synthetic parts box","new Storage defaults");
        auto* storageType=dialog.findChild<QComboBox*>("partOutStorageType");
        check(storageType->findData(box)>=0,"Box is available from authoritative Storage types");
        storageType->setCurrentIndex(storageType->findData(box));
        check(waitFor([&]{return confirm->isEnabled();}),"worker preview completes");
        check(dialog.findChild<QLabel*>("partOutStatus")->text().contains("intact copies"),"actual worker preview displays Collection warning");
        QApplication::processEvents();
        if (app.arguments().contains("--capture-preview"))
            check(dialog.grab().save("part-out-preview.png"),"capture synthetic preview for visual inspection");
        check(stock()==beforeStock,"actual dialog preview does not mutate");
        auto* name=dialog.findChild<QLineEdit*>("partOutStorageName"); name->clear();
        check(!confirm->isEnabled(),"editing invalidates confirmation immediately");
        check(waitFor([&]{return dialog.findChild<QLabel*>("partOutStatus")->text().contains("Enter a Storage name");}),"invalid destination blocker displayed");
        name->setText("Confirmed fixture destination"); check(waitFor([&]{return confirm->isEnabled();}),"destination editable");
        QTimer cancelTimer; cancelTimer.setInterval(1);
        QObject::connect(&cancelTimer,&QTimer::timeout,[]{ if (auto* box=qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) box->button(QMessageBox::Cancel)->click(); });
        cancelTimer.start(); confirm->click(); cancelTimer.stop();
        check(stock()==beforeStock,"cancel final confirmation leaves database untouched");
        int committed=0; QObject::connect(&dialog,&PartOutSetDialog::inventoryCommitted,[&](int ws,int id,bool made){check(ws==100 && id>0 && made,"postcommit refresh signal"); ++committed;});
        QTimer acceptTimer; acceptTimer.setInterval(1);
        QObject::connect(&acceptTimer,&QTimer::timeout,[]{ if (auto* box=qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) box->button(box->standardButtons().testFlag(QMessageBox::Yes)?QMessageBox::Yes:QMessageBox::Ok)->click(); });
        acceptTimer.start(); confirm->click();
        check(dialog.isSubmitting() && !confirm->isEnabled(),"submission disables duplicate confirmation");
        dialog.reject(); dialog.close();
        check(dialog.isVisible() && dialog.isSubmitting(),"close and reject cannot abandon an active atomic write");
        check(waitFor([&]{return dialog.result()==QDialog::Accepted;}),"worker mutation completes with success summary"); acceptTimer.stop();
        check(committed==1 && stock()==beforeStock+11,"one UI submission adds stock exactly once");
        check(scalar("SELECT location_type_id FROM storage_location WHERE name='Confirmed fixture destination'").toInt()==box,
            "actual Part Out selector persists Box while default remains Bin");
    }
    {
        PartOutSetDialog remote(100,100,true); remote.show();
        check(waitFor([&]{return remote.findChild<QLabel*>("partOutStatus")->text().contains("Host/local");})
            && !remote.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->isEnabled(),"Remote dialog explains blocked execution");
    }
    check(count("build")==1 && scalar("SELECT status||modified_utc FROM build").toString()=="Planned2026-01-01"
        && count("build_allocation")==0,"existing Build and allocation state unchanged");
    std::fprintf(stdout,"Catalog Set part-out: %d failures\n",failures);
    return failures?1:0;
}
