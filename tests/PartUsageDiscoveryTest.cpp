#include "../src/repositories/PartUsageDiscoveryRepository.h"
#include <QCoreApplication>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTextStream>

namespace { bool require(bool value,const QString& message){if(!value)QTextStream(stderr)<<message<<'\n';return value;} }

int main(int argc,char**argv)
{
    QCoreApplication app(argc,argv);QSqlDatabase db=QSqlDatabase::addDatabase("QSQLITE","usage-test");db.setDatabaseName(":memory:");
    bool ok=require(db.open(),"open");QSqlQuery q(db);
    const QStringList sql={
        "CREATE TABLE theme_catalog(id INTEGER PRIMARY KEY,name TEXT)",
        "CREATE TABLE set_catalog(id INTEGER PRIMARY KEY,set_number TEXT,name TEXT,year INTEGER,theme_id INTEGER,num_parts INTEGER,image_url TEXT)",
        "CREATE TABLE set_inventory_revision(id INTEGER PRIMARY KEY,set_catalog_id INTEGER,provider TEXT,is_active INTEGER,is_preferred INTEGER)",
        "CREATE TABLE set_inventory_part(set_inventory_revision_id INTEGER,part_id INTEGER,color_id INTEGER,quantity INTEGER,is_spare INTEGER)",
        "CREATE TABLE set_catalog_part(set_catalog_id INTEGER,part_id INTEGER,color_id INTEGER,quantity_required INTEGER,is_spare INTEGER)",
        "INSERT INTO theme_catalog VALUES(1,'City')",
        "INSERT INTO set_catalog VALUES(1,'100-1','Preferred',2024,1,10,''),(2,'200-1','Legacy',2023,1,8,''),(3,'300-1','Spare only',2022,1,1,''),(4,'400-1','Second match',2021,1,20,'')",
        "INSERT INTO set_inventory_revision VALUES(10,1,'Rebrickable',1,1)",
        "INSERT INTO set_inventory_part VALUES(10,1,1,2,0),(10,1,2,2,0),(10,2,1,1,0),(10,9,1,99,1)",
        "INSERT INTO set_catalog_part VALUES(1,1,1,99,0),(2,1,1,5,0),(2,2,1,1,0),(3,1,1,9,1),(4,1,1,1,0)"};
    for(const auto&s:sql)ok&=require(q.exec(s),q.lastError().text());
    PartUsageDiscoveryRepository repo(db);
    PartUsageSearch request;request.criteria={{1,"3001","Brick",0,"Any Color",4}};
    auto result=repo.search(request);ok&=require(result.success&&result.sets.size()==2&&result.sets[0].setCatalogId==1,"Any Color/preferred fallback");
    request.criteria={{1,"3001","Brick",1,"Red",3}};result=repo.search(request);ok&=require(result.success&&result.sets.size()==1&&result.sets[0].setCatalogId==2,"Exact Color quantity");
    request.criteria={{1,"3001","Brick",1,"Red",1},{2,"x","Other",1,"Red",1}};request.matchMode=PartUsageMatchMode::All;result=repo.search(request);ok&=require(result.sets.size()==2,"All criteria");
    request.matchMode=PartUsageMatchMode::Any;result=repo.search(request);ok&=require(result.sets.size()==3&&result.sets[0].matchedCriteria==2,"Any ranking");
    request.criteria={{1,"3001","Brick",1,"Red",2},{1,"3001","Brick",1,"Red",3}};request.matchMode=PartUsageMatchMode::All;result=repo.search(request);ok&=require(result.sets.size()==1&&result.sets[0].setCatalogId==2,"Duplicate criteria merged");
    request.criteria={{1,"3001","Brick",0,"Any",3},{1,"3001","Brick",1,"Red",2}};result=repo.search(request);ok&=require(result.sets.size()==1&&result.sets[0].setCatalogId==2,"Overlapping Any/exact capacity");
    request.matchMode=PartUsageMatchMode::Any;result=repo.search(request);ok&=require(result.sets.size()==2&&result.sets[0].setCatalogId==2&&result.sets[0].matchedCriteria==2&&result.sets[1].matchedCriteria==1,"Any ranking does not double-consume overlapping quantities");
    request.criteria={{1,"3001","Brick",0,"Any",1}};request.maximumResults=50;request.text="Preferred";result=repo.search(request);ok&=require(result.sets.size()==1&&result.sets[0].setCatalogId==1,"Search text");
    request.text.clear();request.criteria.clear();
    for(int i=0;i<21;++i)request.criteria.append({i+1,QString::number(i+1),"Part",0,"Any",1});
    result=repo.search(request);ok&=require(!result.success,"Criteria limit");
    for(int i=0;i<55;++i){
        q.prepare("INSERT INTO set_catalog VALUES(:id,:number,'Cap',2024,1,1,'')");
        q.bindValue(":id",100+i);q.bindValue(":number",QString("C%1-1").arg(i,2,10,QChar('0')));
        ok&=require(q.exec(),q.lastError().text());
        q.prepare("INSERT INTO set_catalog_part VALUES(:id,77,1,1,0)");q.bindValue(":id",100+i);
        ok&=require(q.exec(),q.lastError().text());
    }
    request.criteria={{77,"77","Cap Part",0,"Any",1}};request.maximumResults=50;
    result=repo.search(request);ok&=require(result.success&&result.capReached
        &&result.qualifyingCount==55&&result.sets.size()==50,"Honest result cap");
    db.close();db={};QSqlDatabase::removeDatabase("usage-test");if(ok)QTextStream(stdout)<<"PartUsageDiscoveryTest passed\n";return ok?0:1;
}
