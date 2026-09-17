#include "../src/repositories/InventoryBuildabilityRepository.h"
#include <QCoreApplication>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <iostream>

namespace {
bool require(bool value,const char* message){if(!value)std::cerr<<message<<'\n';return value;}
bool sql(QSqlDatabase&db,const QString&s){QSqlQuery q(db);if(q.exec(s))return true;std::cerr<<q.lastError().text().toStdString()<<'\n';return false;}
}

int main(int argc,char**argv)
{
    QCoreApplication app(argc,argv);QTemporaryDir dir;if(!require(dir.isValid(),"temporary directory"))return 1;
    const QString name="buildability-test";auto db=QSqlDatabase::addDatabase("QSQLITE",name);db.setDatabaseName(dir.filePath("test.db"));if(!require(db.open(),"database open"))return 1;
    const QStringList schema={
        "CREATE TABLE part(id INTEGER PRIMARY KEY,part_number TEXT,name TEXT)",
        "CREATE TABLE color(id INTEGER PRIMARY KEY,name TEXT,rebrickable_id INTEGER)",
        "CREATE TABLE theme_catalog(id INTEGER PRIMARY KEY,name TEXT,parent_theme_catalog_id INTEGER,is_active INTEGER)",
        "CREATE TABLE theme_external_identifier(theme_catalog_id INTEGER,provider TEXT,external_id TEXT,is_active INTEGER)",
        "CREATE TABLE set_catalog(id INTEGER PRIMARY KEY,set_number TEXT,name TEXT,year INTEGER,theme_id INTEGER,image_url TEXT,num_parts INTEGER)",
        "CREATE TABLE set_catalog_part(id INTEGER PRIMARY KEY,set_catalog_id INTEGER,part_id INTEGER,color_id INTEGER,quantity_required INTEGER,is_spare INTEGER)",
        "CREATE TABLE set_inventory_revision(id INTEGER PRIMARY KEY,provider TEXT,external_inventory_id TEXT,set_catalog_id INTEGER,version INTEGER,is_active INTEGER,is_preferred INTEGER)",
        "CREATE TABLE set_inventory_part(id INTEGER PRIMARY KEY,set_inventory_revision_id INTEGER,part_id INTEGER,color_id INTEGER,quantity INTEGER,is_spare INTEGER)",
        "CREATE TABLE storage_location(id INTEGER PRIMARY KEY,workspace_id INTEGER,parent_location_id INTEGER,is_active INTEGER,allows_inventory INTEGER)",
        "CREATE TABLE inventory_record(id INTEGER PRIMARY KEY,workspace_id INTEGER,part_id INTEGER,color_id INTEGER,storage_location_id INTEGER,ownership_type TEXT,quantity INTEGER)",
        "CREATE TABLE build(id INTEGER PRIMARY KEY,workspace_id INTEGER)",
        "CREATE TABLE build_allocation(id INTEGER PRIMARY KEY,build_id INTEGER,part_id INTEGER,color_id INTEGER,quantity_allocated INTEGER)",
        "CREATE TABLE collection_item(id INTEGER PRIMARY KEY,workspace_id INTEGER,item_type TEXT,set_catalog_id INTEGER,state TEXT,completeness TEXT,allow_parts_source INTEGER,is_active INTEGER,nickname TEXT)"};
    for(const auto&s:schema)if(!sql(db,s))return 1;
    const QStringList data={
        "INSERT INTO part VALUES(1,'3001','Brick'),(2,'3002','Plate')",
        "INSERT INTO color VALUES(1,'Red',4),(2,'Blue',1)",
        "INSERT INTO theme_catalog VALUES(100,'Space',NULL,1),(101,'Classic Space',100,1),(200,'Technic',NULL,1)",
        "INSERT INTO theme_external_identifier VALUES(100,'Rebrickable','100',1),(101,'Rebrickable','101',1),(200,'Rebrickable','200',1)",
        "INSERT INTO set_catalog VALUES(10,'A-1','Exact Set',2026,100,'',4),(20,'B-1','Collection Set',2025,100,'',2),(30,'C-1','Fallback Set',2024,101,'',3),(40,'D-1','Two Source Set',2024,101,'',5),(50,'E-1','Twenty Four',2015,101,'',24),(60,'F-1','Twenty Five',2020,100,'',25),(65,'F-2','Twenty Six',2021,100,'',26),(66,'F-3','Combined Match',2020,101,'',25),(70,'G-1','Spare Inflation',2010,200,'',27),(80,'H-1','One Piece',0,0,'',1),(90,'I-1','No Composition',2020,100,'',0)",
        "INSERT INTO set_inventory_revision VALUES(1,'Rebrickable','100',10,1,1,1),(2,'Rebrickable','200',20,1,1,1)",
        "INSERT INTO set_inventory_part VALUES(1,1,1,1,3,0),(2,1,2,2,1,0),(3,1,1,2,99,1),(4,2,1,1,2,0)",
        "INSERT INTO set_catalog_part VALUES(1,30,1,1,3,0),(2,30,2,2,50,1),(3,40,1,1,5,0),(4,50,1,1,24,0),(5,60,1,1,25,0),(6,70,1,1,24,0),(7,70,2,2,3,1),(8,80,2,2,1,0),(9,65,1,1,26,0),(10,66,1,1,5,0),(11,66,1,2,20,0)",
        "INSERT INTO storage_location VALUES(1,7,NULL,1,1),(2,7,1,1,1),(3,7,NULL,0,1)",
        "INSERT INTO inventory_record VALUES(1,7,1,1,2,'Owned',2),(2,7,2,2,2,'Owned',1),(3,7,1,2,2,'Owned',20),(4,7,1,1,3,'Owned',20),(5,7,1,1,2,'Borrowed',20)",
        "INSERT INTO build VALUES(1,7)",
        "INSERT INTO build_allocation VALUES(1,1,1,1,1)",
        "INSERT INTO collection_item VALUES(5,7,'Set',20,'Sealed','Complete',1,1,'Source Box'),(8,7,'Set',20,'Assembled','Complete',1,1,'Second Source')",
        "INSERT INTO collection_item VALUES(6,7,'Set',20,'Assembled','Incomplete',1,1,'Incomplete')",
        "INSERT INTO collection_item VALUES(9,7,'Set',20,'Unassembled','Complete',1,0,'Archived')",
        "INSERT INTO collection_item VALUES(10,7,'Set',90,'PartiallyAssembled','Complete',1,1,'No composition')",
        "INSERT INTO collection_item VALUES(7,7,'Set',20,'Assembled','Complete',0,1,'Not opted in')"};
    for(const auto&s:data)if(!sql(db,s))return 1;
    for(int i=0;i<51;++i){if(!sql(db,QStringLiteral("INSERT INTO set_catalog VALUES(%1,'CAP-%2','Cap Candidate',2020,100,'',25)").arg(1000+i).arg(i))||!sql(db,QStringLiteral("INSERT INTO set_catalog_part VALUES(%1,%2,1,1,25,0)").arg(100+i).arg(1000+i)))return 1;}
    InventoryBuildabilitySearch request;request.workspaceId=7;request.minimumPercent=0;request.minimumSetParts=1;request.text="A-1";request.includeCollection=true;request.maximumResults=250;
    auto result=InventoryBuildabilityRepository(db).search(request);
    if(!require(result.success,"search succeeds")||!require(result.eligibleCollectionSources==1,"only assembled complete opted-in sources eligible")||!require(result.dormantCollectionSources==4,"sealed, incomplete, archived, and missing-composition sources are dormant")||!require(result.sets.size()==1,"text filter"))return 1;
    auto find=[&](int id)->InventoryBuildabilitySetResult{for(const auto&x:result.sets)if(x.setCatalogId==id)return x;return{};};
    const auto exact=find(10);if(!require(exact.looseSatisfiedQuantity==2,"allocation is subtracted and exact color used")||!require(exact.advisorySatisfiedQuantity==4,"collection applied after loose")||!require(exact.advisoryPercent()==100,"collection-assisted percent")||!require(exact.sources.size()==1&&exact.sources.first().collectionItemId==8,"stable assembled Collection source identity"))return 1;
    request.text.clear();result=InventoryBuildabilityRepository(db).search(request);
    const auto fallback=find(30);if(!require(fallback.totalQuantity==3,"legacy fallback composition used")||!require(fallback.totalRequirements==1,"spares excluded"))return 1;
    const auto twoSources=find(40);if(!require(twoSources.sources.size()==1,"only assembled Collection copies remain physical sources")||!require(twoSources.sources.first().piecesUsed==2,"composition is not multiplied by ineligible source rows"))return 1;
    request.includeCollection=false;result=InventoryBuildabilityRepository(db).search(request);const auto looseOnly=[&]{for(const auto&x:result.sets)if(x.setCatalogId==10)return x;return InventoryBuildabilitySetResult{};}();
    if(!require(looseOnly.advisorySatisfiedQuantity==looseOnly.looseSatisfiedQuantity,"collection exclusion")||!require(looseOnly.sources.isEmpty(),"no advisory sources when excluded"))return 1;
    request.includeCollection=true;request.minimumPercent=0;request.fullyBuildableOnly=false;request.minimumSetParts=25;result=InventoryBuildabilityRepository(db).search(request);
    if(!require(find(50).setCatalogId==0,"24-piece Set excluded at minimum 25")||!require(find(60).setCatalogId==60,"25-piece Set included")||!require(find(65).setCatalogId==65,"26-piece Set included")||!require(find(70).setCatalogId==0,"spares do not inflate minimum Set size"))return 1;
    request.minimumSetParts=1;result=InventoryBuildabilityRepository(db).search(request);if(!require(find(80).setCatalogId==80,"minimum 1 restores one-piece Sets"))return 1;
    request.yearFrom=2015;request.yearTo=2025;request.themeCatalogId=100;result=InventoryBuildabilityRepository(db).search(request);
    if(!require(find(20).setCatalogId==20&&find(30).setCatalogId==30&&find(50).setCatalogId==50&&find(60).setCatalogId==60&&find(65).setCatalogId==65,"parent Theme includes descendants within year range")||!require(find(10).setCatalogId==0&&find(70).setCatalogId==0&&find(80).setCatalogId==0,"year, unrelated Theme, and unknown year excluded"))return 1;
    request.themeCatalogId=101;result=InventoryBuildabilityRepository(db).search(request);if(!require(find(30).setCatalogId==30&&find(50).setCatalogId==50&&find(60).setCatalogId==0,"leaf Theme exact subtree"))return 1;
    if(!require(find(30).themeCatalogId==101&&find(30).themeName=="Space → Classic Space","Theme filter and qualified result display share catalog identity"))return 1;
    request.minimumPercent=75;request.minimumSetParts=25;request.yearFrom=2015;request.yearTo=2025;request.themeCatalogId=101;request.includeCollection=true;result=InventoryBuildabilityRepository(db).search(request);
    if(!require(result.sets.size()==1&&result.sets.first().setCatalogId==66,"combined size, year, Theme, buildability, and Collection filter")||!require(result.candidateCountBeforeFilters>result.candidateCountAfterCatalogFilters&&result.preciseEvaluationCount==result.candidateCountAfterCatalogFilters,"catalog filters reduce precise evaluation"))return 1;
    request.yearFrom=2026;request.yearTo=2025;result=InventoryBuildabilityRepository(db).search(request);if(!require(!result.success&&result.errorMessage.contains("Year From"),"invalid year range rejected"))return 1;
    request={};request.workspaceId=7;request.text="CAP-";request.minimumPercent=0;request.minimumSetParts=25;request.includeCollection=false;request.maximumResults=50;result=InventoryBuildabilityRepository(db).search(request);
    if(!require(result.success&&result.qualifyingCount==51&&result.sets.size()==50&&result.capReached,"exhaustive qualifying count, ranking-before-truncation, and honest cap"))return 1;
    db.close();db={};QSqlDatabase::removeDatabase(name);return 0;
}
