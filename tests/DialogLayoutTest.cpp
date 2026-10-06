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


#include "../src/app/WorkspaceContext.h"
#include "../src/database/DatabaseManager.h"
#include "../src/settings/ThemeManager.h"
#include "../src/settings/UserSettings.h"
#include "../src/services/storage/SessionStorageSelectionService.h"
#include "../src/ui/inventory/AddInventoryDialog.h"
#include "../src/ui/inventory/EditInventoryDialog.h"
#include "../src/ui/inventory/MoveInventoryDialog.h"
#include "../src/ui/inventory/RemoveInventoryDialog.h"
#include "../src/ui/storage/StorageLocationDialog.h"
#include "../src/ui/builds/EditBuildDialog.h"
#include "../src/ui/catalog/PartOutSetDialog.h"
#include "../src/ui/parts/FitCalibrationDialog.h"
#include "../src/ui/about/AboutDialog.h"
#include "../src/services/CredentialStore.h"
CredentialStore::ReadResult CredentialStore::read(const QString&) { return {true, false, {}, {}}; }
bool CredentialStore::write(const QString&, const QString&, QString*) { return false; }
bool CredentialStore::remove(const QString&, QString*) { return false; }
QString CredentialStore::backendName() { return QStringLiteral("Isolated audit double"); }
#include "../src/services/application/ApplicationServices.h"
#include "../src/network/BrickSuiteWebSocketClient.h"
#include "../src/services/application/RemoteMutationApplicationServices.h"
#include "../src/services/application/RemoteInventoryMutationApplicationService.h"
#include "../src/ui/inventory/CorrectInventoryDialog.h"
#include "../src/ui/inventory/FoundInventoryDialog.h"
#include "../src/ui/inventory/MarkLostInventoryDialog.h"
#include "../src/ui/inventory/ImportInventoryDialog.h"
#include "../src/ui/catalog/SetDetailsDialog.h"
#include "../src/ui/parts/PartDetailsDialog.h"
#include "../src/ui/collection/CatalogCollectionDialog.h"
#include "../src/ui/settings/SettingsDialog.h"
#include "../src/network/BrickSuiteNetworkManager.h"
#include <QTabWidget>
#include <QStyleHints>
#include <QCheckBox>
#include "../src/ui/catalog/MinifigDetailsDialog.h"
#include "../src/ui/collection/CollectionItemDialog.h"
#include "../src/ui/builds/EditBuildRequirementDialog.h"
#include "../src/ui/parts/AddPartReferenceDialog.h"
#include "../src/repositories/InventoryRecordRepository.h"
#include "../src/repositories/CollectionRepository.h"
#include <QScrollArea>
#include <QScrollBar>
#include <QAbstractSpinBox>
#include <QTextEdit>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QProxyStyle>
#include <QStyleFactory>
#include <QScreen>
#include "../src/ui/parts/ManufacturingMeshDiagnosticDialog.h"
#include "../src/ui/storage/StorageWidget.h"
#include "../src/ui/builds/BuildsWidget.h"
#include "../src/repositories/StorageLocationTypeRepository.h"
#include <QTreeWidget>
#include <QTableWidget>
#include <QProcess>
#include <QProcessEnvironment>
#include <QLabel>
#include <QGroupBox>
#include <QCompleter>
#include <QAbstractItemView>
#include <QEventLoop>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFormLayout>
#include <QLineEdit>
#include <QComboBox>
#include <QSettings>
#include <QSqlQuery>
#include <QSqlError>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTimer>
#include <QUuid>
#include <QDebug>
class CompactFormStyle : public QProxyStyle {
public:
 CompactFormStyle():QProxyStyle(QStyleFactory::create("Fusion")){}
 int styleHint(StyleHint hint,const QStyleOption*option=nullptr,const QWidget*widget=nullptr,QStyleHintReturn*data=nullptr)const override {
  return hint==SH_FormLayoutFieldGrowthPolicy?QFormLayout::FieldsStayAtSizeHint:QProxyStyle::styleHint(hint,option,widget,data);
 }
};
void settleEvents(){QEventLoop loop;QTimer::singleShot(30,&loop,&QEventLoop::quit);loop.exec();}
int main(int argc,char**argv){
 QApplication app(argc,argv); app.setQuitOnLastWindowClosed(false);
 if(!app.arguments().contains("--capture-dir"))app.setStyle(new CompactFormStyle);
 if(!app.arguments().contains("--layout-child")&&!app.arguments().contains("--capture-dir")){
  QTemporaryDir screenConfig;
  QFile file(screenConfig.filePath("screen.json"));if(!file.open(QIODevice::WriteOnly))return 1;
  file.write(R"({"screens":[{"name":"Layout test","width":1920,"height":1080,"logicalDpi":96,"logicalBaseDpi":96,"dpr":1}]})");file.close();
  for(const char*dpi:{"96","144"}){
   QProcess child;auto environment=QProcessEnvironment::systemEnvironment();environment.insert("QT_FONT_DPI",dpi);child.setProcessEnvironment(environment);
   child.start(app.applicationFilePath(),{"-platform","offscreen:configfile="+file.fileName(),"--layout-child"});
   if(!child.waitForFinished(40000)||child.exitStatus()!=QProcess::NormalExit||child.exitCode()!=0){qWarning().noquote()<<dpi<<child.readAllStandardError();return 1;}
  }
  return 0;
 }
 app.setOrganizationName("BrickSuiteDialogAudit"); app.setApplicationName(QUuid::createUuid().toString());
 QStandardPaths::setTestModeEnabled(true); QTemporaryDir settings;
 QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settings.path());
 if(!DatabaseManager::instance().initialize())return 1;
 QSqlQuery q(DatabaseManager::instance().database());
 QStringList sql={
 "INSERT INTO workspace(id,name,created_utc,modified_utc) VALUES(100,'Fixture',CURRENT_TIMESTAMP,CURRENT_TIMESTAMP)",
 "INSERT INTO storage_location(id,workspace_id,location_type_id,name,is_active,created_utc,modified_utc) VALUES(100,100,1,'Fixture bin',1,CURRENT_TIMESTAMP,CURRENT_TIMESTAMP)",
 "INSERT INTO part(id,part_number,name,is_active,created_utc,modified_utc) VALUES(100,'3001','Brick 2 x 4 — representative description',1,CURRENT_TIMESTAMP,CURRENT_TIMESTAMP)",
 "INSERT INTO color(id,name,rebrickable_id,created_utc,modified_utc) VALUES(100,'Fixture Red',4,CURRENT_TIMESTAMP,CURRENT_TIMESTAMP)",
 "INSERT INTO inventory_record(id,workspace_id,part_id,color_id,storage_location_id,manufacturer_id,condition,ownership_type,quantity,created_utc,modified_utc) VALUES(100,100,100,100,100,1,'Used','Owned',5,CURRENT_TIMESTAMP,CURRENT_TIMESTAMP)",
 "INSERT INTO build(id,workspace_id,build_type,name,inventory_mode,status,is_active,created_utc,modified_utc) VALUES(100,100,'MOC','Fixture build','Stock','Planned',1,CURRENT_TIMESTAMP,CURRENT_TIMESTAMP)",
 "INSERT INTO set_catalog(id,set_number,name,year,num_parts,created_utc,modified_utc) VALUES(100,'fixture-1','Fixture set',2026,0,CURRENT_TIMESTAMP,CURRENT_TIMESTAMP)"};
 for(const auto&s:sql)if(!q.exec(s)){qWarning()<<q.lastError();return 1;}
 q.exec("INSERT INTO minifig_catalog(id,name,num_parts,is_active,created_utc,modified_utc) VALUES(100,'Fixture figure with a descriptive name',0,1,CURRENT_TIMESTAMP,CURRENT_TIMESTAMP)");
 q.exec("INSERT INTO build_requirement(id,build_id,part_id,color_id,quantity_required,is_spare,created_utc,modified_utc) VALUES(100,100,100,100,2,0,CURRENT_TIMESTAMP,CURRENT_TIMESTAMP)");
 InventoryRecordRepository().markLost(100,1,"Synthetic layout fixture");
 CollectionItem item;item.workspaceId=100;item.type=CollectionItemType::Set;item.setCatalogId=100;item.state=CollectionItemState::Assembled;CollectionRepository().create(item);
 q.exec("INSERT INTO storage_location(id,workspace_id,location_type_id,name,is_active,created_utc,modified_utc) VALUES(101,100,1,'Workshop shelving with descriptive labels',1,CURRENT_TIMESTAMP,CURRENT_TIMESTAMP)");
 q.exec("INSERT INTO storage_location(id,workspace_id,location_type_id,parent_location_id,name,is_active,created_utc,modified_utc) VALUES(102,100,1,101,'Drawer for small red Technic elements',1,CURRENT_TIMESTAMP,CURRENT_TIMESTAMP)");
 WorkspaceContext workspace;workspace.setCurrentWorkspaceId(100);SessionStorageSelectionService storage;
 bool ok=true;
 const auto check=[&](bool value,const QString&message){if(!value)qWarning().noquote()<<"FAIL:"<<message;ok &= value;};
 const int captureIndex=app.arguments().indexOf("--capture-dir");
 const QString captureDir=captureIndex>=0?app.arguments().value(captureIndex+1):QString();
 if(!captureDir.isEmpty())QDir().mkpath(captureDir);
 auto capture=[&](QDialog&d,const QString&name){
 const QSize normal=d.size();
 for(auto theme:{UserSettings::Theme::Dark,UserSettings::Theme::Light}){
 ThemeManager::applyTheme(app,theme);d.resize(normal);d.show();settleEvents();
 const QString suffix=theme==UserSettings::Theme::Dark?"dark":"light";
 QList<QWidget*> fields;
 for(auto*edit:d.findChildren<QLineEdit*>())
     if(edit->isVisible()&&!qobject_cast<QAbstractSpinBox*>(edit->parentWidget()))fields<<edit;
 for(auto*edit:d.findChildren<QTextEdit*>())if(edit->isVisible())fields<<edit;
 for(auto*combo:d.findChildren<QComboBox*>())
     if(combo->isVisible()&&combo->sizePolicy().horizontalPolicy()==QSizePolicy::Expanding)fields<<combo;
QList<QAbstractSpinBox*> compact;QList<int> compactWidths;
 for(auto*spin:d.findChildren<QAbstractSpinBox*>())if(spin->isVisible()){compact<<spin;compactWidths<<spin->width();}
 QList<int> widths;for(auto*field:fields)widths<<field->width();
 const int before=d.width();d.resize(before+200,d.height());settleEvents();
 for(int i=0;i<fields.size();++i)
     check(fields[i]->width()>widths[i],name+": descriptive field grows: "+fields[i]->objectName());
for(int i=0;i<compact.size();++i)check(compact[i]->width()==compactWidths[i],name+": compact counts do not stretch");
 d.resize(normal);settleEvents();
 if(!captureDir.isEmpty())d.grab().save(captureDir+"/"+name+"-"+suffix+".png");
 d.resize(QSize(name=="calibration"?640:480,480).boundedTo(d.screen()->availableGeometry().size()));settleEvents();
 check(d.width()<=d.screen()->availableGeometry().width()&&d.height()<=d.screen()->availableGeometry().height(),name+": bounded to screen");
 for(auto*box:d.findChildren<QDialogButtonBox*>())if(box->isVisible())
     check(d.rect().contains(QRect(box->mapTo(&d,QPoint()),box->size())),name+": action buttons inside dialog");
 if(auto*scroll=d.findChild<QScrollArea*>("fitCalibrationScroll")){
     auto*notes=d.findChild<QLineEdit*>("fitCalibrationObservationNotes");
     scroll->ensureWidgetVisible(notes);settleEvents();
     check(notes->width()>=notes->minimumSizeHint().width()*2,name+": useful observation notes");
     check(scroll->viewport()->rect().intersects(QRect(notes->mapTo(scroll->viewport(),QPoint()),notes->size())),name+": notes reachable by scrolling");
 }
 
 if(auto*scroll=d.findChild<QScrollArea*>("catalogDetailsScroll")){
     check(scroll->horizontalScrollBar()->maximum()==0,name+": no horizontal scrolling needed");
     for(auto*label:scroll->findChildren<QLabel*>())if(label->isVisible()){
         check(label->parentWidget()->rect().contains(label->geometry()),name+": header labels and image contained");
         if(label->wordWrap())check(label->height()>=label->heightForWidth(label->width()),name+": wrapped text fully visible");
     }
     auto*content=scroll->widget();
     scroll->ensureVisible(0,content->height());settleEvents();
     check(scroll->verticalScrollBar()->value()==scroll->verticalScrollBar()->maximum(),name+": bottom content reachable");
     scroll->verticalScrollBar()->setValue(0);settleEvents();
 }
 if(!captureDir.isEmpty())d.grab().save(captureDir+"/"+name+"-"+suffix+"-constrained.png");
 }d.hide();};
 AddInventoryDialog add(workspace,storage); capture(add,"add");
 BrickSuiteWebSocketClient client;RemoteMutationApplicationServices mutations(client);RemoteInventoryMutationApplicationService remote(mutations);
 AddInventoryDialog remoteAdd(workspace,storage,remote,{{100,"Host / Fixture bin"}},{"LEGO"},100,"fixture",{});capture(remoteAdd,"remote-add");

 ApplicationServices services;
 FoundInventoryDialog found(100,100,100,storage);capture(found,"found");
 MinifigDetailsDialog minifig(100,workspace);capture(minifig,"minifig");
 CollectionItemDialog collectionItem(item.id);capture(collectionItem,"collection-item");
 EditBuildRequirementDialog requirement(100);capture(requirement,"requirement");
 AddPartReferenceDialog referenceAdd(services.partReferenceCustomizations(),100);capture(referenceAdd,"reference-add");
 CorrectInventoryDialog correct(100,workspace);capture(correct,"correct");
 MarkLostInventoryDialog lost(100);capture(lost,"mark-lost");
 ImportInventoryDialog import(workspace,storage);capture(import,"import");
 SetDetailsDialog details(100,workspace);capture(details,"set-details");
 PartDetailsDialog part(100);capture(part,"part-details");
 CatalogCollectionDialog collection(100,CollectionItemType::Set,100,"fixture-1","Fixture set");capture(collection,"catalog-collection");
 BrickSuiteNetworkManager network;
 SettingsDialog settingsDialog(workspace,services.workspaces(),network);
 auto*tabs=settingsDialog.findChild<QTabWidget*>();
 for(int i=0;i<tabs->count();++i){
 if(!QStringList{"APIs","Database Backup","Server"}.contains(tabs->tabText(i)))continue;
 tabs->setCurrentIndex(i);
 auto*nested=tabs->widget(i)->findChild<QTabWidget*>();
 if(nested){for(int j=0;j<nested->count();++j){nested->setCurrentIndex(j);capture(settingsDialog,"settings-"+QString::number(i)+"-"+QString::number(j));}}
 else capture(settingsDialog,"settings-"+QString::number(i));
 }
 EditInventoryDialog edit(100,workspace);capture(edit,"edit");
 MoveInventoryDialog move(100,workspace,storage);capture(move,"move");
 RemoveInventoryDialog remove(100);capture(remove,"remove");
 QList<StorageLocationDialog::Choice> types;
 for(const auto&type:StorageLocationTypeRepository().getActive())types.append({type.id(),type.name()});
 StorageLocationDialog::Values values;values.name="Fixture bin";
 StorageLocationDialog location(StorageLocationDialog::Mode::Add,values,types,{{100,"Fixture parent / Nested container"}});capture(location,"storage-add");
 auto*typeCombo=location.findChild<QComboBox*>("storageTypeCombo");check(typeCombo->findText("Box")>=0,"Storage Box available");
 for(int i=0;i<typeCombo->count();++i){typeCombo->setCurrentIndex(i);check(location.values().storageTypeId==typeCombo->currentData().toLongLong(),"Storage type identity unchanged");}
 StorageLocationDialog editLocation(StorageLocationDialog::Mode::Edit,values,types,{{100,"Fixture parent / Nested container"}});capture(editLocation,"storage-edit");
 EditBuildDialog build(100);capture(build,"build-edit");
 PartOutSetDialog partOut(100,100,false);capture(partOut,"partout");
 auto*outType=partOut.findChild<QComboBox*>("partOutStorageType");
 check(outType->currentText()=="Bin"&&outType->findText("Box")>=0,"Part Out defaults to Bin and offers Box");
 auto*outMode=partOut.findChild<QComboBox*>("partOutDestinationMode");outMode->setCurrentIndex(1);capture(partOut,"partout-existing");
 FitCalibrationDialog calibration(nullptr,settings.path()+"/calibration");capture(calibration,"calibration");
 auto inspectModal=[&](const QString&name){QTimer::singleShot(0,[&,name]{auto*d=qobject_cast<QDialog*>(QApplication::activeModalWidget());check(d!=nullptr,name+": opened");if(d){d->resize(700,360);capture(*d,name);d->reject();}});};
 StorageWidget storageWidget(workspace);storageWidget.show();settleEvents();
 inspectModal("local-storage-add");QMetaObject::invokeMethod(&storageWidget,"addLocation");
 auto*tree=storageWidget.findChild<QTreeWidget*>();tree->setCurrentItem(tree->topLevelItem(0));
 inspectModal("local-storage-edit");QMetaObject::invokeMethod(&storageWidget,"editLocation");storageWidget.hide();
 BuildsWidget builds(workspace,storage,services.builds());builds.show();settleEvents();
 for(auto*t:builds.findChildren<QTableWidget*>())if(t->columnCount()==8&&t->rowCount()>0)t->setCurrentCell(0,0);
 settleEvents();inspectModal("preferred-storage");QMetaObject::invokeMethod(&builds,"allocateAvailable");builds.hide();
 ManufacturingMeshDiagnosticDialog diagnostic({}, {}, 1.0,Qt::gray,{});capture(diagnostic,"diagnostic");
 AboutDialog about;capture(about,"about");about.resize(600,400);capture(about,"about-constrained");
 DatabaseManager::instance().close();
 QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)).removeRecursively();
 return ok?0:1;
}
