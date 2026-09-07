#include "../src/database/DatabaseManager.h"
#include "../src/models/InventorySearchCriteria.h"
#include "../src/models/PartSearchCriteria.h"
#include "../src/repositories/InventoryRecordRepository.h"
#include "../src/repositories/PartRepository.h"
#include <QCoreApplication>
#include <QDir>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QStringList>
#include <QUuid>
#include <cstdio>

namespace {
bool require(bool value,const QString& message){if(!value)std::fprintf(stderr,"%s\n",message.toUtf8().constData());return value;}
QVariant scalar(QSqlDatabase db,const QString& sql){QSqlQuery q(db);return q.exec(sql)&&q.next()?q.value(0):QVariant();}
bool reportPlan(QSqlDatabase db,const QString& label,const QString& sql)
{
    QSqlQuery query(db);
    if (!query.exec("EXPLAIN QUERY PLAN " + sql))
        return require(false, label + ": " + query.lastError().text());
    QStringList details;
    while (query.next()) details.append(query.value(3).toString());
    std::fprintf(stdout,"QUERY PLAN %s: %s\n",label.toUtf8().constData(),details.join(" | ").toUtf8().constData());
    return require(!details.isEmpty(),label + " produced no plan rows");
}
class Cleanup{QString path;public:explicit Cleanup(QString p):path(std::move(p)){}~Cleanup(){DatabaseManager::instance().close();QDir(path).removeRecursively();}};
}

int main(int argc,char** argv)
{
    QCoreApplication app(argc,argv);QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setOrganizationName("RFStateSideTests");
    QCoreApplication::setApplicationName("PagingProjection_"+QUuid::createUuid().toString(QUuid::WithoutBraces));
    const QString path=QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);Cleanup cleanup(path);
    if(!require(DatabaseManager::instance().initialize(),"initialize isolated database"))return 1;
    QSqlDatabase db=DatabaseManager::instance().database();QSqlQuery q(db);const QString now="2026-09-08T12:00:00.000Z";
    if(!require(q.exec("INSERT INTO workspace(name,description,created_utc,modified_utc,is_active) VALUES('Paging','', '"+now+"','"+now+"',1)"),"workspace"))return 1;
    const int workspace=q.lastInsertId().toInt();
    const int type=scalar(db,"SELECT id FROM storage_location_type WHERE is_active=1 ORDER BY id LIMIT 1").toInt();
    const int manufacturer=scalar(db,"SELECT id FROM manufacturer WHERE is_active=1 ORDER BY id LIMIT 1").toInt();
    if(!require(q.exec(QString("INSERT INTO storage_location(workspace_id,location_type_id,name,is_active,created_utc,modified_utc) VALUES(%1,%2,'Bin',1,'%3','%3')").arg(workspace).arg(type).arg(now)),"location"))return 1;
    const int location=q.lastInsertId().toInt();
    if(!require(q.exec("INSERT INTO part_category(name,rebrickable_id,created_utc,modified_utc) VALUES('Paging Category',99001,'"+now+"','"+now+"')"),"category"))return 1;
    const int category=q.lastInsertId().toInt();
    if(!require(q.exec("INSERT INTO color(name,rgb,is_transparent,rebrickable_id,created_utc,modified_utc) VALUES('Paging Red','AA0000',0,321,'"+now+"','"+now+"')"),"color"))return 1;
    const int color=q.lastInsertId().toInt();
    for(int i=1;i<=5;++i){
        const QString number=QString("paging-%1").arg(i,2,10,QChar('0'));
        q.prepare("INSERT INTO part(part_number,name,part_category_id,is_active,created_utc,modified_utc,material) VALUES(?,?,?,1,?,?,'Plastic')");
        q.addBindValue(number);q.addBindValue("Paging Part "+QString::number(i));q.addBindValue(category);q.addBindValue(now);q.addBindValue(now);
        if(!require(q.exec(),"part "+number))return 1;const int part=q.lastInsertId().toInt();
        q.prepare("INSERT INTO inventory_record(workspace_id,part_id,color_id,storage_location_id,manufacturer_id,condition,ownership_type,quantity,created_utc,modified_utc) VALUES(?,?,?,?,?,'Used','Owned',1,?,?)");
        q.addBindValue(workspace);q.addBindValue(part);q.addBindValue(color);q.addBindValue(location);q.addBindValue(manufacturer);q.addBindValue(now);q.addBindValue(now);
        if(!require(q.exec(),"inventory "+number))return 1;
    }

    PartSearchCriteria partsCriteria;partsCriteria.limit=2;partsCriteria.offset=2;
    const auto parts=PartRepository().search(partsCriteria);
    if(!require(parts.size()==2,"Parts page size retained")
       ||!require(parts.at(0).part.partNumber()=="paging-03"&&parts.at(1).part.partNumber()=="paging-04","Parts offset and ordering retained")
       ||!require(parts.at(0).part.id()>0&&parts.at(1).part.id()>0,"Parts page projects stable internal IDs"))return 1;
    partsCriteria.categoryId=category;partsCriteria.searchText="Paging Part";partsCriteria.offset=0;
    if(!require(PartRepository().count(partsCriteria)==5&&PartRepository().search(partsCriteria).size()==2,"Parts filters/count remain correct"))return 1;

    InventorySearchCriteria inventoryCriteria;inventoryCriteria.workspaceId=workspace;inventoryCriteria.limit=2;inventoryCriteria.offset=2;
    const auto inventory=InventoryRecordRepository().search(inventoryCriteria);
    if(!require(inventory.size()==2,"Inventory page size retained")
       ||!require(inventory.at(0).partNumber=="paging-03"&&inventory.at(1).partNumber=="paging-04","Inventory offset and ordering retained")
       ||!require(inventory.at(0).rebrickableColorId==321&&inventory.at(1).rebrickableColorId==321,"Inventory page projects Rebrickable color ID"))return 1;
    inventoryCriteria.categoryId=category;inventoryCriteria.colorId=color;inventoryCriteria.storageLocationId=location;inventoryCriteria.searchText="Paging";inventoryCriteria.offset=0;
    if(!require(InventoryRecordRepository().count(inventoryCriteria)==5&&InventoryRecordRepository().search(inventoryCriteria).size()==2,"Inventory filters/count remain correct"))return 1;

    const QString partBase = "SELECT p.id FROM part p WHERE p.is_active=1";
    const QString partSearch = partBase + " AND (p.part_number LIKE '%paging%' OR p.name LIKE '%paging%' OR EXISTS (SELECT 1 FROM part_alias pa WHERE pa.part_id=p.id AND pa.is_active=1 AND pa.alias_part_number LIKE '%paging%'))";
    if(!reportPlan(db,"Parts unfiltered",partBase+" ORDER BY p.part_number LIMIT 2 OFFSET 2")
       ||!reportPlan(db,"Parts category",partBase+QString(" AND p.part_category_id=%1 ORDER BY p.part_number LIMIT 2").arg(category))
       ||!reportPlan(db,"Parts exact",partSearch+" ORDER BY CASE WHEN p.part_number='paging-03' COLLATE NOCASE THEN 0 ELSE 4 END,p.part_number LIMIT 2")
       ||!reportPlan(db,"Parts prefix",partSearch+" ORDER BY CASE WHEN p.part_number LIKE 'paging-%' THEN 2 ELSE 4 END,p.part_number LIMIT 2")
       ||!reportPlan(db,"Parts contains",partSearch+" ORDER BY p.part_number LIMIT 2")
       ||!reportPlan(db,"Parts alias",partBase+" AND EXISTS (SELECT 1 FROM part_alias pa WHERE pa.part_id=p.id AND pa.is_active=1 AND pa.alias_part_number LIKE '%external%') ORDER BY p.part_number LIMIT 2"))return 1;
    const QString inventoryBase=QString("SELECT ir.id FROM inventory_record ir INNER JOIN part p ON p.id=ir.part_id WHERE ir.workspace_id=%1 AND ir.quantity>0").arg(workspace);
    if(!reportPlan(db,"Inventory unfiltered",inventoryBase+" ORDER BY p.part_number LIMIT 2 OFFSET 2")
       ||!reportPlan(db,"Inventory category",inventoryBase+QString(" AND p.part_category_id=%1 ORDER BY p.part_number LIMIT 2").arg(category))
       ||!reportPlan(db,"Inventory color",inventoryBase+QString(" AND ir.color_id=%1 ORDER BY p.part_number LIMIT 2").arg(color))
       ||!reportPlan(db,"Inventory storage",inventoryBase+QString(" AND ir.storage_location_id=%1 ORDER BY p.part_number LIMIT 2").arg(location))
       ||!reportPlan(db,"Inventory text",inventoryBase+" AND (p.part_number LIKE '%paging%' OR p.name LIKE '%paging%') ORDER BY p.part_number LIMIT 2"))return 1;
    return 0;
}
