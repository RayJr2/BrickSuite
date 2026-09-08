#include "../src/database/DatabaseSchema.h"
#include "../src/import/RebrickableInventoryCompositionImporter.h"
#include "../src/repositories/SetInventoryRevisionRepository.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QThread>
#include <cstdio>
#include <zlib.h>

namespace {
bool require(bool value, const QString& message)
{ if (!value) std::fprintf(stderr, "%s\n", qPrintable(message)); return value; }
bool writeFile(const QString& path, const QByteArray& data)
{ QFile file(path); return file.open(QIODevice::WriteOnly) && file.write(data)==data.size(); }
void put16(QByteArray& bytes, quint16 value)
{ bytes.append(char(value)); bytes.append(char(value >> 8)); }
void put32(QByteArray& bytes, quint32 value)
{ put16(bytes,quint16(value)); put16(bytes,quint16(value >> 16)); }
bool writeZip(const QString& path, const QByteArray& name, const QByteArray& data)
{
    const quint32 checksum=quint32(crc32(0,reinterpret_cast<const Bytef*>(data.constData()),uInt(data.size())));
    QByteArray bytes;put32(bytes,0x04034b50);put16(bytes,20);put16(bytes,0);put16(bytes,0);put16(bytes,0);put16(bytes,0);put32(bytes,checksum);put32(bytes,data.size());put32(bytes,data.size());put16(bytes,name.size());put16(bytes,0);bytes+=name;bytes+=data;
    const quint32 offset=bytes.size();put32(bytes,0x02014b50);put16(bytes,20);put16(bytes,20);put16(bytes,0);put16(bytes,0);put16(bytes,0);put16(bytes,0);put32(bytes,checksum);put32(bytes,data.size());put32(bytes,data.size());put16(bytes,name.size());put16(bytes,0);put16(bytes,0);put16(bytes,0);put16(bytes,0);put32(bytes,0);put32(bytes,0);bytes+=name;
    const quint32 size=bytes.size()-offset;put32(bytes,0x06054b50);put16(bytes,0);put16(bytes,0);put16(bytes,1);put16(bytes,1);put32(bytes,size);put32(bytes,offset);put16(bytes,0);return writeFile(path,bytes);
}
int scalar(QSqlDatabase& db, const QString& sql)
{ QSqlQuery q(db); return q.exec(sql)&&q.next()?q.value(0).toInt():-1; }
bool exec(QSqlDatabase& db, const QString& sql)
{ QSqlQuery q(db); if(q.exec(sql))return true;qCritical()<<q.lastError().text();return false; }
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc,argv); QTemporaryDir directory;
    if(!require(directory.isValid(),"Temporary directory failed."))return 1;
    const QString connection="inventory-composition-test";
    QSqlDatabase db=QSqlDatabase::addDatabase("QSQLITE",connection);
    db.setDatabaseName(directory.filePath("test.db"));
    if(!require(db.open()&&DatabaseSchema::initialize(db),"Fresh schema 34 failed."))return 1;
    if(!require(scalar(db,"SELECT version FROM schema_version")==34,"Schema is not 34."))return 1;
    const QStringList tables={"set_inventory_revision","set_inventory_part","set_inventory_minifig","set_inventory_contained_set"};
    for(const QString& table:tables)if(!require(db.tables().contains(table),"Missing table "+table))return 1;
    if(!require(exec(db,"INSERT INTO set_catalog(set_number,name,year,theme_id,num_parts,image_url,created_utc,modified_utc) VALUES('s1','One',2026,1,2,'','n','n'),('s2','Two',2026,1,1,'','n','n')")
        &&exec(db,"INSERT INTO part(part_number,name,rebrickable_part_id,is_active,created_utc,modified_utc,material) VALUES('p1','Part','p1',1,'n','n','Plastic'),('p2','Manual Part','p2',1,'n','n','Plastic')")
        &&exec(db,"INSERT INTO color(name,rgb,is_transparent,rebrickable_id,created_utc,modified_utc) VALUES('Red','ff0000',0,5,'n','n')")
        &&exec(db,"INSERT INTO minifig_catalog(name,num_parts,image_url,is_active,created_utc,modified_utc) VALUES('Fig',1,'',1,'n','n')")
        &&exec(db,"INSERT INTO minifig_external_identifier(minifig_catalog_id,provider,external_id,source,is_active,created_utc,modified_utc) SELECT id,'Rebrickable','fig-1','test',1,'n','n' FROM minifig_catalog"),"Seed failed."))return 1;
    const int legacySetParts=scalar(db,"SELECT COUNT(*) FROM set_catalog_part");
    RebrickableInventoryCompositionImporter importer;
    const QString inventories=directory.filePath("inventories.csv.zip");
    writeZip(inventories,"nested/inventories.csv","id,version,set_num\n10,1,s1\n11,2,s1\n20,1,s2\n30,1,fig-1\n");
    auto result=importer.importInventories(inventories,db);
    if(!require(result.success&&result.rowsRead==4&&result.inserted==3
        &&result.setInventoryRows==3&&result.recognizedMinifigInventories==1
        &&scalar(db,"SELECT COUNT(*) FROM set_inventory_revision")==3
        &&scalar(db,"SELECT COUNT(*) FROM set_inventory_revision WHERE external_inventory_id='30'")==0,
        "Mixed Set/Minifig Inventories import failed."))return 1;
    if(!require(scalar(db,"SELECT COUNT(*) FROM set_inventory_revision WHERE is_preferred=1")==0,"Incomplete revisions were promoted."))return 1;

    RebrickableInventoryCompositionImporter rejectingImporter;
    const QString invalidInventories=directory.filePath("invalid-inventories.csv");
    writeFile(invalidInventories,"id,version,set_num\n98,3,s1\n99,1,unknown-owner\n");
    if(!require(!rejectingImporter.importInventories(invalidInventories,db).success
        &&scalar(db,"SELECT COUNT(*) FROM set_inventory_revision")==3,
        "Unknown Inventory owner did not fail atomically."))return 1;
    if(!require(exec(db,"INSERT INTO minifig_catalog(name,num_parts,image_url,is_active,created_utc,modified_utc) VALUES('Ambiguous Fig',1,'',1,'n','n')")
        &&exec(db,"INSERT INTO minifig_external_identifier(minifig_catalog_id,provider,external_id,source,is_active,created_utc,modified_utc) SELECT id,'Rebrickable','s2','test',1,'n','n' FROM minifig_catalog WHERE name='Ambiguous Fig'"),"Unable to seed ambiguous identity."))return 1;
    writeFile(invalidInventories,"id,version,set_num\n99,1,s2\n");
    if(!require(!rejectingImporter.importInventories(invalidInventories,db).success
        &&scalar(db,"SELECT COUNT(*) FROM set_inventory_revision")==3,
        "Ambiguous Set/Minifig Inventory owner did not fail atomically."))return 1;
    if(!require(exec(db,"UPDATE minifig_external_identifier SET is_active=0 WHERE provider='Rebrickable' AND external_id='s2'"),"Unable to retire ambiguous test identity."))return 1;

    const QString parts=directory.filePath("inventory_parts.csv");
    if(!require(exec(db,"INSERT INTO minifig_catalog_part(minifig_catalog_id,part_id,color_id,quantity_required,is_spare,provider,source,created_utc,modified_utc) SELECT m.id,p.id,c.id,2,0,'Manual','test','n','n' FROM minifig_catalog m,part p,color c WHERE m.name='Fig' AND p.part_number='p2' AND c.rebrickable_id=5"),"Unable to seed manual Minifig composition."))return 1;
    writeFile(parts,"inventory_id,part_num,color_id,quantity,is_spare,img_url\n10,p1,5,2,f,https://x/one.png\n10,p1,5,1,t,\n11,p1,5,3,false,https://x/two.png\n20,p1,5,1,0,\n30,p1,5,4,t,\n");
    result=importer.importParts(parts,db);
    if(!require(result.success&&result.rowsRead==5&&result.setInventoryRows==4
        &&result.minifigPartRows==1
        &&scalar(db,"SELECT COUNT(*) FROM set_inventory_part")==4
        &&scalar(db,"SELECT COUNT(*) FROM minifig_catalog_part WHERE provider='Rebrickable' AND quantity_required=4 AND is_spare=1")==1
        &&scalar(db,"SELECT COUNT(*) FROM minifig_catalog_part WHERE provider='Manual'")==1,"Inventory Parts ownership classification/persistence failed."))return 1;
    if(!require(scalar(db,"SELECT COUNT(*) FROM set_inventory_part WHERE is_spare=1")==1
        &&scalar(db,"SELECT COUNT(*) FROM set_inventory_part WHERE image_url='https://x/two.png'")==1,"Spare/image metadata failed."))return 1;
    result=importer.importParts(parts,db);
    if(!require(result.success&&result.inserted==0&&result.updated==0
        &&result.unchanged==5&&result.replaced==0&&result.minifigPartRows==1,"Inventory Parts reimport was not idempotent."))return 1;
    const QString invalidParts=directory.filePath("invalid-inventory-parts.csv");
    writeFile(invalidParts,"inventory_id,part_num,color_id,quantity,is_spare,img_url\n10,p1,5,99,f,\n30,p1,5,88,t,\n20,missing,5,1,f,\n");
    if(!require(!importer.importParts(invalidParts,db).success
        &&scalar(db,"SELECT quantity FROM set_inventory_part WHERE set_inventory_revision_id=(SELECT id FROM set_inventory_revision WHERE external_inventory_id='10') AND is_spare=0")==2
        &&scalar(db,"SELECT quantity_required FROM minifig_catalog_part WHERE provider='Rebrickable'")==4,
        "Invalid Inventory Parts source did not roll back Set and Minifig changes atomically."))return 1;
    if(!require(exec(db,"WITH RECURSIVE n(x) AS (VALUES(1) UNION ALL SELECT x+1 FROM n WHERE x<512) INSERT INTO part(part_number,name,rebrickable_part_id,is_active,created_utc,modified_utc,material) SELECT 'bulk'||x,'Bulk','bulk'||x,1,'n','n','Plastic' FROM n"),"Unable to seed cancellation Parts."))return 1;
    QByteArray cancellationCsv("inventory_id,part_num,color_id,quantity,is_spare,img_url\n");
    for(int row=0;row<1024;++row)cancellationCsv+=QStringLiteral("%1,bulk%2,5,1,f,\n").arg((row&1)?"30":"10").arg((row/2)+1).toUtf8();
    const QString cancellationParts=directory.filePath("cancelled-inventory-parts.csv");writeFile(cancellationParts,cancellationCsv);
    const int setRowsBeforeCancel=scalar(db,"SELECT COUNT(*) FROM set_inventory_part"),minifigRowsBeforeCancel=scalar(db,"SELECT COUNT(*) FROM minifig_catalog_part");
    RebrickableImportCancellation midImportCancellation;
    const auto cancelledResult=importer.importParts(cancellationParts,db,&midImportCancellation,[&](qint64 rows){if(rows>=1024)midImportCancellation.requestCancellation();});
    if(!require(!cancelledResult.success&&setRowsBeforeCancel==scalar(db,"SELECT COUNT(*) FROM set_inventory_part")&&minifigRowsBeforeCancel==scalar(db,"SELECT COUNT(*) FROM minifig_catalog_part"),"Cancellation did not roll back both Set and Minifig composition."))return 1;
    RebrickableInventoryCompositionImporter partialImporter;
    const auto partialResult=partialImporter.importParts(parts,db);
    if(!require(!partialResult.success
        &&partialResult.message.contains("Inventories source is required"),
        "Inventory Parts ran without a safe ownership classification source."))return 1;
    const QString figures=directory.filePath("inventory_minifigs.csv");
    writeFile(figures,"inventory_id,fig_num,quantity\n11,fig-1,1\n");
    if(!require(importer.importMinifigs(figures,db).success,"Inventory Minifigs import failed."))return 1;
    writeFile(figures,"inventory_id,fig_num,quantity\n30,fig-1,1\n");
    if(!require(!importer.importMinifigs(figures,db).success&&scalar(db,"SELECT COUNT(*) FROM set_inventory_minifig")==1,"Minifig-owned Inventory Minifigs row was accepted."))return 1;
    writeFile(figures,"inventory_id,fig_num,quantity\nunknown,fig-1,1\n");
    if(!require(!importer.importMinifigs(figures,db).success&&scalar(db,"SELECT COUNT(*) FROM set_inventory_minifig")==1,"Unknown Inventory Minifigs owner was accepted."))return 1;
    const QString sets=directory.filePath("inventory_sets.csv");
    writeFile(sets,"inventory_id,set_num,quantity\n20,s1,1\n");
    if(!require(importer.importSets(sets,db).success,"Inventory Sets import failed."))return 1;
    writeFile(sets,"inventory_id,set_num,quantity\n30,s1,1\n");
    if(!require(!importer.importSets(sets,db).success&&scalar(db,"SELECT COUNT(*) FROM set_inventory_contained_set")==1,"Minifig-owned Inventory Sets row was accepted."))return 1;
    writeFile(sets,"inventory_id,set_num,quantity\nunknown,s1,1\n");
    if(!require(!importer.importSets(sets,db).success&&scalar(db,"SELECT COUNT(*) FROM set_inventory_contained_set")==1,"Unknown Inventory Sets owner was accepted."))return 1;
    result=importer.selectPreferredRevisions(db);
    const int s1=scalar(db,"SELECT id FROM set_catalog WHERE set_number='s1'");
    const int s2=scalar(db,"SELECT id FROM set_catalog WHERE set_number='s2'");
    if(!require(result.success&&result.preferredChanged==2&&scalar(db,"SELECT version FROM set_inventory_revision WHERE set_catalog_id=(SELECT id FROM set_catalog WHERE set_number='s1') AND is_preferred=1")==2,"Highest revision or initial preferred-flag counter was incorrect."))return 1;
    if(!require(importer.selectPreferredRevisions(db).preferredChanged==0,"Preferred selection was not idempotent."))return 1;
    QSqlQuery constraint(db);
    constraint.prepare("INSERT INTO set_inventory_revision(provider,external_inventory_id,set_catalog_id,version,is_active,is_preferred,created_utc,modified_utc) VALUES('Rebrickable',:external,:set_id,:version,1,:preferred,'n','n')");
    constraint.bindValue(":external","10"); constraint.bindValue(":set_id",s2); constraint.bindValue(":version",99); constraint.bindValue(":preferred",0);
    if(!require(!constraint.exec(),"Duplicate provider inventory identity was accepted."))return 1;
    constraint.bindValue(":external","duplicate-version"); constraint.bindValue(":set_id",s1); constraint.bindValue(":version",2); constraint.bindValue(":preferred",0);
    if(!require(!constraint.exec(),"Duplicate provider Set/version was accepted."))return 1;
    constraint.bindValue(":external","duplicate-preferred"); constraint.bindValue(":set_id",s1); constraint.bindValue(":version",99); constraint.bindValue(":preferred",1);
    if(!require(!constraint.exec(),"A second preferred revision was accepted."))return 1;

    if(!require(exec(db,QStringLiteral("INSERT INTO set_inventory_revision(provider,external_inventory_id,set_catalog_id,version,is_active,is_preferred,created_utc,modified_utc) VALUES('OtherProvider','other-1',%1,1,1,1,'n','n')").arg(s2))
        &&exec(db,"INSERT INTO set_inventory_part(set_inventory_revision_id,part_id,color_id,quantity,is_spare,image_url,created_utc,modified_utc) SELECT r.id,p.id,c.id,7,0,'other-provider','n','n' FROM set_inventory_revision r,part p,color c WHERE r.provider='OtherProvider' AND p.part_number='p1' AND c.rebrickable_id=5"),
        "Unable to seed another provider's composition."))return 1;
    writeFile(parts,"inventory_id,part_num,color_id,quantity,is_spare,img_url\n10,p1,5,4,f,https://x/replaced.png\n");
    result=importer.importParts(parts,db);
    if(!require(result.success&&result.updated==1&&result.replaced==2
        &&scalar(db,"SELECT COUNT(*) FROM set_inventory_part WHERE set_inventory_revision_id=(SELECT id FROM set_inventory_revision WHERE provider='Rebrickable' AND external_inventory_id='10')")==1
        &&scalar(db,"SELECT COUNT(*) FROM set_inventory_part WHERE set_inventory_revision_id=(SELECT id FROM set_inventory_revision WHERE provider='OtherProvider' AND external_inventory_id='other-1')")==1
        &&scalar(db,"SELECT COUNT(*) FROM set_inventory_part WHERE set_inventory_revision_id=(SELECT id FROM set_inventory_revision WHERE provider='Rebrickable' AND external_inventory_id='11')")==1
        &&scalar(db,"SELECT COUNT(*) FROM minifig_catalog_part WHERE provider='Rebrickable'")==0
        &&scalar(db,"SELECT COUNT(*) FROM minifig_catalog_part WHERE provider='Manual'")==1,
        "Affected-revision replacement was not scoped by revision/provider."))return 1;
    SetInventoryRevisionRepository repository(db);
    const auto revisions=repository.revisionsForSet(s1,"Rebrickable");
    const auto preferred=repository.preferredRevisionForSet(s1,"Rebrickable");
    if(!require(revisions.size()==2&&preferred.version==2
        &&repository.partsForRevision(preferred.id,false).size()==1
        &&repository.minifigsForRevision(preferred.id).size()==1,
        "Revision repository forward queries failed."))return 1;

    writeFile(parts,"inventory_id,part_num,color_id,quantity,is_spare,img_url\n11,missing,5,1,f,\n");
    const int before=scalar(db,"SELECT COUNT(*) FROM set_inventory_part");
    if(!require(!importer.importParts(parts,db).success&&scalar(db,"SELECT COUNT(*) FROM set_inventory_part")==before,"Unresolved Part did not roll back."))return 1;
    writeFile(parts,"inventory_id,part_num,color_id,quantity,is_spare,img_url\n11,p1,999,1,f,\n");
    if(!require(!importer.importParts(parts,db).success&&scalar(db,"SELECT COUNT(*) FROM set_inventory_part")==before,"Unresolved Color did not roll back."))return 1;
    writeFile(parts,"inventory_id,part_num,color_id,quantity,is_spare,img_url\nunknown,p1,5,1,f,\n");
    if(!require(!importer.importParts(parts,db).success&&scalar(db,"SELECT COUNT(*) FROM set_inventory_part")==before,"Unknown Inventory Part owner did not roll back."))return 1;
    writeFile(figures,"inventory_id,fig_num,quantity\n11,missing,1\n");
    if(!require(!importer.importMinifigs(figures,db).success&&scalar(db,"SELECT COUNT(*) FROM set_inventory_minifig")==1,"Unresolved Minifig did not roll back."))return 1;
    writeFile(sets,"inventory_id,set_num,quantity\n20,missing,1\n");
    if(!require(!importer.importSets(sets,db).success&&scalar(db,"SELECT COUNT(*) FROM set_inventory_contained_set")==1,"Unresolved contained Set did not roll back."))return 1;

    QSqlQuery bulkRevision(db);
    bulkRevision.prepare("INSERT INTO set_inventory_revision(provider,external_inventory_id,set_catalog_id,version,is_active,is_preferred,created_utc,modified_utc) SELECT 'Rebrickable',:external,id,:version,1,0,'n','n' FROM set_catalog WHERE set_number='s2'");
    QByteArray largeInventories("id,version,set_num\n10,1,s1\n11,2,s1\n20,1,s2\n30,1,fig-1\n");
    QByteArray large("inventory_id,part_num,color_id,quantity,is_spare,img_url\n");
    if(!require(db.transaction(),"Unable to begin large fixture seed."))return 1;
    for(int i=0;i<1200;++i){
        bulkRevision.bindValue(":external",QStringLiteral("bulk-%1").arg(i));
        bulkRevision.bindValue(":version",100+i);
        if(!require(bulkRevision.exec(),"Unable to seed large streamed fixture."))return 1;
        largeInventories += QStringLiteral("bulk-%1,%2,s2\n").arg(i).arg(100+i).toUtf8();
        large += QStringLiteral("bulk-%1,p1,5,1,f,\n").arg(i).toUtf8();
    }
    if(!require(db.commit(),"Unable to commit large fixture seed."))return 1;
    writeFile(invalidInventories,largeInventories);
    if(!require(importer.importInventories(invalidInventories,db).success,"Unable to establish large-fixture ownership map."))return 1;
    writeFile(parts,large); RebrickableImportCancellation cancellation;
    int progressCalls=0; result=importer.importParts(parts,db,&cancellation,[&](qint64){++progressCalls;cancellation.requestCancellation();});
    if(!require(!result.success&&progressCalls==1&&scalar(db,"SELECT COUNT(*) FROM set_inventory_part")==before,"Mid-stream cancellation did not roll back the active snapshot."))return 1;
    QElapsedTimer largeTimer; largeTimer.start();
    progressCalls=0; result=importer.importParts(parts,db,nullptr,[&](qint64){++progressCalls;});
    if(!require(result.success&&result.rowsRead==1200&&progressCalls==2
        &&scalar(db,"SELECT COUNT(*) FROM set_inventory_part")==before+1200,"Large streamed fixture failed."))return 1;
    qInfo()<<"Large synthetic Inventory Parts elapsed ms:" << largeTimer.elapsed();
    if(!require(scalar(db,"SELECT COUNT(*) FROM set_catalog_part")==legacySetParts,"Legacy composition changed."))return 1;

    // Simulate an existing schema-32 database and verify its data survives the sequential migration.
    if(!require(exec(db,"UPDATE schema_version SET version=32"),"Unable to stage schema 32."))return 1;
    for(const QString& table:QStringList{"set_inventory_part","set_inventory_minifig","set_inventory_contained_set","set_inventory_revision"})
        if(!require(exec(db,"DROP TABLE "+table),"Unable to stage schema 32 tables."))return 1;
    if(!require(DatabaseSchema::initialize(db)&&scalar(db,"SELECT version FROM schema_version")==34
        &&scalar(db,"SELECT COUNT(*) FROM set_catalog")==2,"Schema 32 to 34 migration failed or lost data."))return 1;
    if(!require(DatabaseSchema::initialize(db),"Schema 34 reinitialization was not idempotent."))return 1;
    db.close(); db=QSqlDatabase(); QSqlDatabase::removeDatabase(connection);
    qInfo()<<"Inventory composition persistence tests passed.";return 0;
}
