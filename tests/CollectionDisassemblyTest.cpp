#include "../src/database/DatabaseManager.h"
#include "../src/repositories/CollectionRepository.h"
#include "../src/services/collection/CollectionDisassemblyService.h"
#include "../src/services/collection/CollectionItemService.h"

#include <QCoreApplication>
#include <QDir>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <cstdio>

namespace {
bool require(bool value,const QString& message){if(!value){qCritical().noquote()<<message;std::fprintf(stderr,"FAILED: %s\n",message.toUtf8().constData());}return value;}
int scalar(QSqlDatabase db,const QString&sql){QSqlQuery q(db);return q.exec(sql)&&q.next()?q.value(0).toInt():-1;}
bool exec(QSqlDatabase db,const QString&sql){QSqlQuery q(db);if(q.exec(sql))return true;qCritical()<<q.lastError().text()<<sql;return false;}
class Cleanup{public:explicit Cleanup(QString p):path(std::move(p)){}~Cleanup(){DatabaseManager::instance().close();QDir(path).removeRecursively();}QString path;};
}

int main(int argc,char**argv)
{
    QCoreApplication app(argc,argv);app.setOrganizationName("RFStateSideTests");app.setApplicationName("CollectionDisassembly");QStandardPaths::setTestModeEnabled(true);
    const QString data=QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);QDir(data).removeRecursively();Cleanup cleanup(data);
    if(!require(DatabaseManager::instance().initialize(),"database initialization"))return 1;
    QSqlDatabase db=DatabaseManager::instance().database();const QString now="2026-01-01T00:00:00.000Z";
    if(!require(exec(db,"INSERT INTO workspace(name,description,created_utc,modified_utc) VALUES('Test','', '"+now+"','"+now+"')"),"workspace"))return 1;
    const int workspace=scalar(db,"SELECT id FROM workspace WHERE name='Test'");
    const int storageType=scalar(db,"SELECT id FROM storage_location_type ORDER BY id LIMIT 1");
    if(!require(exec(db,QString("INSERT INTO storage_location(workspace_id,parent_location_id,location_type_id,name,description,sort_order,is_active,allows_inventory,allows_collection,created_utc,modified_utc) VALUES(%1,NULL,%2,'Inventory Bin','',0,1,1,0,'%3','%3')").arg(workspace).arg(storageType).arg(now)),"inventory destination"))return 1;
    if(!require(exec(db,QString("INSERT INTO storage_location(workspace_id,parent_location_id,location_type_id,name,description,sort_order,is_active,allows_inventory,allows_collection,created_utc,modified_utc) VALUES(%1,NULL,%2,'Second Bin','',1,1,1,0,'%3','%3')").arg(workspace).arg(storageType).arg(now)),"second inventory destination"))return 1;
    if(!require(exec(db,QString("INSERT INTO storage_location(workspace_id,parent_location_id,location_type_id,name,description,sort_order,is_active,allows_inventory,allows_collection,created_utc,modified_utc) VALUES(%1,NULL,%2,'Invalidated Bin','',2,1,1,0,'%3','%3')").arg(workspace).arg(storageType).arg(now)),"invalidated inventory destination"))return 1;
    const int destination=scalar(db,"SELECT id FROM storage_location WHERE name='Inventory Bin'");
    const int secondDestination=scalar(db,"SELECT id FROM storage_location WHERE name='Second Bin'");
    const int invalidatedDestination=scalar(db,"SELECT id FROM storage_location WHERE name='Invalidated Bin'");
    if(!require(exec(db,"INSERT INTO part(part_number,name,rebrickable_part_id,is_active,created_utc,modified_utc,material) VALUES('p1','One','p1',1,'"+now+"','"+now+"','Plastic'),('p2','Two','p2',1,'"+now+"','"+now+"','Plastic')"),"parts")
       ||!require(exec(db,"INSERT INTO color(name,rebrickable_id,created_utc,modified_utc) VALUES('Red',1,'"+now+"','"+now+"'),('Blue',2,'"+now+"','"+now+"')"),"colors"))return 1;
    const int p1=scalar(db,"SELECT id FROM part WHERE part_number='p1'"),p2=scalar(db,"SELECT id FROM part WHERE part_number='p2'");
    const int red=scalar(db,"SELECT id FROM color WHERE rebrickable_id=1"),blue=scalar(db,"SELECT id FROM color WHERE rebrickable_id=2");
    if(!require(exec(db,"INSERT INTO set_catalog(set_number,name,year,theme_id,num_parts,image_url,created_utc,modified_utc) VALUES('100-1','Set',2026,1,3,'','"+now+"','"+now+"')"),"set")
       ||!require(exec(db,"INSERT INTO minifig_catalog(name,num_parts,image_url,is_active,created_utc,modified_utc) VALUES('Fig',4,'',1,'"+now+"','"+now+"')"),"minifig"))return 1;
    const int setId=scalar(db,"SELECT id FROM set_catalog WHERE set_number='100-1'"),figId=scalar(db,"SELECT id FROM minifig_catalog WHERE name='Fig'");
    if(!require(exec(db,QString("INSERT INTO set_catalog_part(set_catalog_id,part_id,color_id,quantity_required,is_spare,provider,source,created_utc,modified_utc) VALUES(%1,%2,%3,2,0,'Rebrickable','test','%4','%4'),(%1,%5,%6,1,1,'Rebrickable','test','%4','%4')").arg(setId).arg(p1).arg(red).arg(now).arg(p2).arg(blue)),"set composition")
       ||!require(exec(db,QString("INSERT INTO minifig_catalog_part(minifig_catalog_id,part_id,color_id,quantity_required,is_spare,provider,source,created_utc,modified_utc) VALUES(%1,%2,%3,3,0,'Rebrickable','test','%4','%4'),(%1,%5,%6,1,1,'Rebrickable','test','%4','%4')").arg(figId).arg(p1).arg(blue).arg(now).arg(p2).arg(red)),"minifig composition"))return 1;
    if(!require(exec(db,QString("INSERT INTO collection_item(workspace_id,item_type,set_catalog_id,state,condition,completeness,nickname,notes,allow_parts_source,is_active,created_utc,modified_utc) VALUES(%1,'Set',%2,'Assembled','New','Complete','Copy','Keep',1,1,'%3','%3')").arg(workspace).arg(setId).arg(now)),"set collection")
       ||!require(exec(db,QString("INSERT INTO collection_item(workspace_id,item_type,minifig_catalog_id,state,condition,completeness,allow_parts_source,is_active,created_utc,modified_utc) VALUES(%1,'Minifig',%2,'Assembled','Used','Complete',0,1,'%3','%3')").arg(workspace).arg(figId).arg(now)),"fig collection"))return 1;
    const int setItem=scalar(db,"SELECT id FROM collection_item WHERE item_type='Set'"),figItem=scalar(db,"SELECT id FROM collection_item WHERE item_type='Minifig'");
    const int lego=scalar(db,"SELECT id FROM manufacturer WHERE code='LEGO'");
    if(!require(exec(db,QString("INSERT INTO inventory_record(workspace_id,part_id,color_id,storage_location_id,manufacturer_id,condition,ownership_type,quantity,created_utc,modified_utc) VALUES(%1,%2,%3,%4,%5,'New','Owned',1,'%6','%6')").arg(workspace).arg(p1).arg(red).arg(destination).arg(lego).arg(now)),"existing inventory"))return 1;
    CollectionItemService itemService;
    const int recordsBeforeBypass=scalar(db,"SELECT COUNT(*) FROM inventory_record");
    const int movementsBeforeBypass=scalar(db,"SELECT COUNT(*) FROM inventory_movement");
    const auto bypass=itemService.updateDetails(setItem,CollectionItemState::Unassembled,0,
        "Changed","Changed",false,CollectionItemCondition::Used,
        CollectionItemCompleteness::Incomplete);
    if(!require(!bypass.success
        &&bypass.error==CollectionItemService::Error::PhysicalTransitionRequired
        &&scalar(db,QString("SELECT COUNT(*) FROM collection_item WHERE id=%1 AND state='Assembled' AND condition='New' AND completeness='Complete' AND nickname='Copy' AND notes='Keep' AND allow_parts_source=1").arg(setItem))==1
        &&scalar(db,"SELECT COUNT(*) FROM inventory_record")==recordsBeforeBypass
        &&scalar(db,"SELECT COUNT(*) FROM inventory_movement")==movementsBeforeBypass,
        "generic metadata edit cannot bypass physical disassembly or mutate Inventory"))return 1;
    CollectionDisassemblyService service;
    auto setPlan=service.preview(setItem);
    if(!require(setPlan.success&&setPlan.rows.size()==1&&setPlan.totalPieces==2&&setPlan.excludedSparePieces==1,"Set preview excludes spares"))return 1;
    const QList<CollectionDisassemblyService::DestinationAssignment> setAssignments{
        {p1,red,1,destination},{p1,red,1,secondDestination}};
    auto setResult=service.disassemble(setItem,setPlan.item.modifiedUtc,setAssignments);
    if(!require(setResult.success&&setResult.totalPieces==2&&setResult.distinctRows==2,"Set disassembly supports split destinations")
       ||!require(scalar(db,QString("SELECT quantity FROM inventory_record WHERE workspace_id=%1 AND part_id=%2 AND color_id=%3 AND storage_location_id=%4 AND condition='New' AND ownership_type='Owned' AND manufacturer_id=%5").arg(workspace).arg(p1).arg(red).arg(destination).arg(lego))==2,"existing Inventory merged within first destination")
       ||!require(scalar(db,QString("SELECT quantity FROM inventory_record WHERE workspace_id=%1 AND part_id=%2 AND color_id=%3 AND storage_location_id=%4 AND condition='New' AND ownership_type='Owned' AND manufacturer_id=%5").arg(workspace).arg(p1).arg(red).arg(secondDestination).arg(lego))==1,"identical Part/Color preserved in second destination")
       ||!require(scalar(db,QString("SELECT COUNT(*) FROM inventory_record WHERE part_id=%1").arg(p2))==0,"spare excluded")
       ||!require(scalar(db,QString("SELECT COUNT(*) FROM inventory_movement WHERE movement_type='CollectionDisassembly' AND reference_type='Collection' AND reference_id='%1' AND quantity_change=1").arg(setItem))==2,"history written per destination")
       ||!require(scalar(db,QString("SELECT COUNT(*) FROM collection_item WHERE id=%1 AND state='Unassembled' AND condition='New' AND completeness='Complete' AND allow_parts_source=1 AND nickname='Copy' AND notes='Keep'").arg(setItem))==1,"Collection metadata preserved"))return 1;
    const int inventoryAfter=scalar(db,"SELECT SUM(quantity) FROM inventory_record"),movementAfter=scalar(db,"SELECT COUNT(*) FROM inventory_movement");
    if(!require(!service.disassemble(setItem,setPlan.item.modifiedUtc,setAssignments).success&&scalar(db,"SELECT SUM(quantity) FROM inventory_record")==inventoryAfter&&scalar(db,"SELECT COUNT(*) FROM inventory_movement")==movementAfter,"second disassembly rejected without duplicates"))return 1;
    auto figPlan=service.preview(figItem);auto figResult=service.disassemble(figItem,figPlan.item.modifiedUtc,{{p1,blue,3,destination}});
    if(!require(figPlan.success&&figPlan.totalPieces==3&&figPlan.excludedSparePieces==1&&figResult.success,"Minifig disassembly succeeds")
       ||!require(scalar(db,QString("SELECT quantity FROM inventory_record WHERE part_id=%1 AND color_id=%2 AND condition='Used' AND ownership_type='Owned'").arg(p1).arg(blue))==3,"Minifig defaults and quantity")
       ||!require(scalar(db,QString("SELECT COUNT(*) FROM collection_item WHERE id=%1 AND state='Unassembled' AND completeness='Complete' AND condition='Used'").arg(figItem))==1,"Minifig state transition"))return 1;
    auto insertState=[&](const QString&state,const QString&complete,int source=0,const QString&type="Set"){
        QSqlQuery q(db);q.prepare("INSERT INTO collection_item(workspace_id,item_type,set_catalog_id,state,condition,completeness,source_build_id,allow_parts_source,is_active,created_utc,modified_utc) VALUES(?,?,?,?,?,?,?,0,1,?,?)");q.addBindValue(workspace);q.addBindValue(type);q.addBindValue(type=="Set"?QVariant(setId):QVariant());q.addBindValue(state);q.addBindValue("Used");q.addBindValue(complete);q.addBindValue(source>0?QVariant(source):QVariant());q.addBindValue(now);q.addBindValue(now);return q.exec()?q.lastInsertId().toInt():0;};
    for(const auto&pair:QList<QPair<QString,QString>>{{"Sealed","Complete"},{"PartiallyAssembled","Complete"},{"Unassembled","Complete"},{"Assembled","Incomplete"},{"Assembled","Unknown"}})
        if(!require(!service.preview(insertState(pair.first,pair.second)).success,"ineligible state/completeness rejected"))return 1;
    const int archived=insertState("Assembled","Complete");exec(db,QString("UPDATE collection_item SET is_active=0 WHERE id=%1").arg(archived));
    if(!require(!service.preview(archived).success,"archived item rejected"))return 1;
    if(!require(exec(db,QString("INSERT INTO build(workspace_id,build_type,name,set_number,set_catalog_id,inventory_mode,manufacturer_id,status,is_active,notes,created_utc,modified_utc) VALUES(%1,'Set','Linked','100-1',%2,'CompleteSet',%3,'Complete',1,'','%4','%4')").arg(workspace).arg(setId).arg(lego).arg(now)),"linked Build"))return 1;
    const int linkedBuild=scalar(db,"SELECT id FROM build WHERE name='Linked'");
    const int linked=insertState("Assembled","Complete",linkedBuild);
    if(!require(linked>0&&!service.preview(linked).success,"Build-linked item rejected"))return 1;
    if(!require(exec(db,QString("INSERT INTO build(workspace_id,build_type,name,set_number,inventory_mode,manufacturer_id,status,is_active,notes,created_utc,modified_utc) VALUES(%1,'MOC','MOC','MOC-1','Stock',%2,'Complete',1,'','%3','%3')").arg(workspace).arg(lego).arg(now)),"MOC Build"))return 1;
    const int mocBuild=scalar(db,"SELECT id FROM build WHERE name='MOC'");
    if(!require(exec(db,QString("INSERT INTO collection_item(workspace_id,item_type,state,condition,completeness,source_build_id,allow_parts_source,is_active,created_utc,modified_utc) VALUES(%1,'MOC','Assembled','Used','Complete',%2,0,1,'%3','%3')").arg(workspace).arg(mocBuild).arg(now)),"MOC Collection"))return 1;
    const int mocItem=scalar(db,"SELECT id FROM collection_item WHERE item_type='MOC'");
    if(!require(!service.preview(mocItem).success,"MOC rejected in M29.1"))return 1;
    if(!require(!service.disassemble(insertState("Assembled","Complete"),QDateTime::fromString(now,Qt::ISODateWithMs),{{p1,red,2,0}}).success,"invalid Storage rejected"))return 1;
    const int incompletePlan=insertState("Assembled","Complete");
    if(!require(!service.disassemble(incompletePlan,QDateTime::fromString(now,Qt::ISODateWithMs),{{p1,red,1,destination}}).success,"partial required quantity rejected"))return 1;
    const int invalidatedItem=insertState("Assembled","Complete");
    const auto invalidatedPlan=service.preview(invalidatedItem);
    exec(db,QString("UPDATE storage_location SET is_active=0 WHERE id=%1").arg(invalidatedDestination));
    const int beforeInvalidatedRecords=scalar(db,"SELECT COUNT(*) FROM inventory_record");
    const int beforeInvalidatedMovements=scalar(db,"SELECT COUNT(*) FROM inventory_movement");
    if(!require(!service.disassemble(invalidatedItem,invalidatedPlan.item.modifiedUtc,
        {{p1,red,1,destination},{p1,red,1,invalidatedDestination}}).success
        &&scalar(db,"SELECT COUNT(*) FROM inventory_record")==beforeInvalidatedRecords
        &&scalar(db,"SELECT COUNT(*) FROM inventory_movement")==beforeInvalidatedMovements
        &&scalar(db,QString("SELECT COUNT(*) FROM collection_item WHERE id=%1 AND state='Assembled'").arg(invalidatedItem))==1,
        "invalidated per-row destination rolls back the complete operation"))return 1;
    const int stale=insertState("Assembled","Complete");auto stalePlan=service.preview(stale);
    exec(db,QString("UPDATE collection_item SET modified_utc='2026-02-01T00:00:00.000Z' WHERE id=%1").arg(stale));
    const int beforeStale=scalar(db,"SELECT SUM(quantity) FROM inventory_record");
    if(!require(!service.disassemble(stale,stalePlan.item.modifiedUtc,{{p1,red,2,destination}}).success&&scalar(db,"SELECT SUM(quantity) FROM inventory_record")==beforeStale,"stale plan rejected atomically"))return 1;
    if(!require(exec(db,"INSERT INTO set_catalog(set_number,name,year,theme_id,num_parts,image_url,created_utc,modified_utc) VALUES('200-1','Rollback Set',2026,1,3,'','"+now+"','"+now+"')"),"rollback Set") )return 1;
    const int rollbackSet=scalar(db,"SELECT id FROM set_catalog WHERE set_number='200-1'");
    if(!require(exec(db,QString("INSERT INTO set_catalog_part(set_catalog_id,part_id,color_id,quantity_required,is_spare,provider,source,created_utc,modified_utc) VALUES(%1,%2,%3,1,0,'Rebrickable','test','%4','%4'),(%1,%5,%6,2,0,'Rebrickable','test','%4','%4')").arg(rollbackSet).arg(p1).arg(red).arg(now).arg(p2).arg(blue)),"rollback composition")
       ||!require(exec(db,QString("INSERT INTO collection_item(workspace_id,item_type,set_catalog_id,state,condition,completeness,allow_parts_source,is_active,created_utc,modified_utc) VALUES(%1,'Set',%2,'Assembled','Used','Complete',0,1,'%3','%3')").arg(workspace).arg(rollbackSet).arg(now)),"rollback Collection"))return 1;
    const int rollbackItem=scalar(db,QString("SELECT id FROM collection_item WHERE set_catalog_id=%1").arg(rollbackSet));
    const auto rollbackPlan=service.preview(rollbackItem);const int recordsBeforeRollback=scalar(db,"SELECT COUNT(*) FROM inventory_record"),movementsBeforeRollback=scalar(db,"SELECT COUNT(*) FROM inventory_movement");
    if(!require(exec(db,QString("CREATE TRIGGER force_collection_disassembly_failure BEFORE INSERT ON inventory_movement WHEN NEW.part_id=%1 BEGIN SELECT RAISE(ABORT,'forced late failure'); END").arg(p2)),"rollback trigger"))return 1;
    if(!require(!service.disassemble(rollbackItem,rollbackPlan.item.modifiedUtc,{{p1,red,1,destination},{p2,blue,2,secondDestination}}).success
        &&scalar(db,"SELECT COUNT(*) FROM inventory_record")==recordsBeforeRollback
        &&scalar(db,"SELECT COUNT(*) FROM inventory_movement")==movementsBeforeRollback
        &&scalar(db,QString("SELECT COUNT(*) FROM collection_item WHERE id=%1 AND state='Assembled'").arg(rollbackItem))==1,
        "late failure rolls back Inventory, history, and Collection state"))return 1;
    exec(db,"DROP TRIGGER force_collection_disassembly_failure");
    qInfo()<<"CollectionDisassemblyTest passed";return 0;
}
