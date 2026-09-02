#include "../src/database/DatabaseManager.h"
#include "../src/database/DatabaseSchema.h"
#include "../src/import/RebrickableSetPartsImporter.h"
#include "../src/models/Build.h"
#include "../src/repositories/BuildRepository.h"
#include "../src/repositories/SetCatalogPartRepository.h"
#include "../src/services/sets/SetCompositionReplacementService.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QUuid>
#include <limits>
#include <zlib.h>

namespace {
bool require(bool ok, const QString& text) { if (!ok) qCritical().noquote() << text; return ok; }
bool writeBytes(const QString& path, const QByteArray& bytes) { QFile f(path); return f.open(QIODevice::WriteOnly) && f.write(bytes) == bytes.size(); }
void u16(QByteArray& b, quint16 v) { b += char(v); b += char(v >> 8); }
void u32(QByteArray& b, quint32 v) { u16(b, quint16(v)); u16(b, quint16(v >> 16)); }
bool writeZip(const QString& path, const QByteArray& name, const QByteArray& data)
{
    QByteArray b; const quint32 crc = quint32(crc32(0L, reinterpret_cast<const Bytef*>(data.constData()), uInt(data.size())));
    u32(b,0x04034b50);u16(b,20);u16(b,0);u16(b,0);u16(b,0);u16(b,0);u32(b,crc);u32(b,data.size());u32(b,data.size());u16(b,name.size());u16(b,0);b+=name;b+=data;
    const quint32 offset=b.size();u32(b,0x02014b50);u16(b,20);u16(b,20);u16(b,0);u16(b,0);u16(b,0);u16(b,0);u32(b,crc);u32(b,data.size());u32(b,data.size());u16(b,name.size());u16(b,0);u16(b,0);u16(b,0);u16(b,0);u32(b,0);u32(b,0);b+=name;
    const quint32 size=b.size()-offset;u32(b,0x06054b50);u16(b,0);u16(b,0);u16(b,1);u16(b,1);u32(b,size);u32(b,offset);u16(b,0);return writeBytes(path,b);
}
QString snapshot(QSqlDatabase db, int setId)
{
    QStringList rows; QSqlQuery q(db); q.prepare("SELECT part_id,color_id,quantity_required,is_spare,provider,source FROM set_catalog_part WHERE set_catalog_id=:id ORDER BY is_spare,part_id,color_id"); q.bindValue(":id",setId);
    if (!q.exec()) return {}; while(q.next()) rows << QString("%1|%2|%3|%4|%5|%6").arg(q.value(0).toInt()).arg(q.value(1).toInt()).arg(q.value(2).toInt()).arg(q.value(3).toInt()).arg(q.value(4).toString(),q.value(5).toString()); return rows.join(';');
}
bool validateMigration(const QString& path)
{
    const QString connection="v27_"+QUuid::createUuid().toString(QUuid::WithoutBraces); bool ok=false;
    { QSqlDatabase db=QSqlDatabase::addDatabase("QSQLITE",connection); db.setDatabaseName(path); if(db.open()) {
        QSqlQuery q(db); const QString now="2026-01-01T00:00:00.000Z";
        ok=q.exec("PRAGMA foreign_keys=ON")
          &&q.exec("CREATE TABLE schema_version(version INTEGER NOT NULL)")&&q.exec("INSERT INTO schema_version VALUES(27)")
          &&q.exec("CREATE TABLE workspace(id INTEGER PRIMARY KEY)")&&q.exec("INSERT INTO workspace VALUES(1)")
          &&q.exec("CREATE TABLE manufacturer(id INTEGER PRIMARY KEY)")&&q.exec("INSERT INTO manufacturer VALUES(1)")
          &&q.exec("CREATE TABLE minifig_catalog(id INTEGER PRIMARY KEY)")&&q.exec("INSERT INTO minifig_catalog VALUES(1)")
          &&q.exec("CREATE TABLE set_catalog(id INTEGER PRIMARY KEY,set_number TEXT UNIQUE,name TEXT,year INTEGER,theme_id INTEGER,num_parts INTEGER,image_url TEXT,created_utc TEXT,modified_utc TEXT)")&&q.exec("INSERT INTO set_catalog VALUES(1,'1234-1','Set',2026,1,2,NULL,'"+now+"','"+now+"')")
          &&q.exec("CREATE TABLE part(id INTEGER PRIMARY KEY)")&&q.exec("CREATE TABLE color(id INTEGER PRIMARY KEY)")
          &&q.exec("CREATE TABLE build(id INTEGER PRIMARY KEY AUTOINCREMENT,workspace_id INTEGER NOT NULL,build_type TEXT NOT NULL CHECK(build_type IN ('Set','MOC','Minifig')),name TEXT NOT NULL,set_number TEXT,inventory_mode TEXT NOT NULL DEFAULT 'Stock' CHECK(inventory_mode IN ('Stock','CompleteSet')),status TEXT NOT NULL DEFAULT 'Planned' CHECK(status IN ('Planned','Pulling','Complete','Disassembled','Cancelled')),is_active INTEGER NOT NULL DEFAULT 1,notes TEXT,created_utc TEXT NOT NULL,modified_utc TEXT NOT NULL,manufacturer_id INTEGER REFERENCES manufacturer(id),minifig_catalog_id INTEGER REFERENCES minifig_catalog(id),FOREIGN KEY(workspace_id) REFERENCES workspace(id))")
          &&q.exec("INSERT INTO build(workspace_id,build_type,name,set_number,inventory_mode,status,is_active,created_utc,modified_utc,manufacturer_id) VALUES(1,'Set','S','1234-1','Stock','Planned',1,'"+now+"','"+now+"',1)")
          &&q.exec("INSERT INTO build(workspace_id,build_type,name,set_number,inventory_mode,status,is_active,created_utc,modified_utc,manufacturer_id) VALUES(1,'MOC','M','moc-1','Stock','Planned',1,'"+now+"','"+now+"',1)")
          &&q.exec("INSERT INTO build(workspace_id,build_type,name,inventory_mode,status,is_active,created_utc,modified_utc,manufacturer_id,minifig_catalog_id) VALUES(1,'Minifig','F','Stock','Planned',1,'"+now+"','"+now+"',1,1)")
          &&DatabaseSchema::initialize(db)
          &&q.exec("SELECT version FROM schema_version")&&q.next()&&q.value(0).toInt()==28
          &&q.exec("SELECT COUNT(*),SUM(set_catalog_id IS NOT NULL),MAX(minifig_catalog_id) FROM build")&&q.next()&&q.value(0).toInt()==3&&q.value(1).toInt()==0&&q.value(2).toInt()==1
          &&q.exec("PRAGMA foreign_key_check")&&!q.next(); db.close(); }} QSqlDatabase::removeDatabase(connection); return ok;
}
class Cleanup { public: explicit Cleanup(QString p):path(std::move(p)){} ~Cleanup(){DatabaseManager::instance().close();QDir(path).removeRecursively();} QString path; };
}

int main(int argc,char* argv[])
{
    QCoreApplication app(argc,argv); app.setOrganizationName("RFStateSideTests"); app.setApplicationName("SetCompositionFoundation"); QStandardPaths::setTestModeEnabled(true);
    QTemporaryDir files; if(!require(files.isValid(),"Temporary directory failed."))return 1;
    if(!require(validateMigration(files.filePath("v27.db")),"Schema 27 to 28 migration preservation failed."))return 1;
    const QString appData=QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);QDir(appData).removeRecursively();Cleanup cleanup(appData);
    if(!require(DatabaseManager::instance().initialize(),"Database initialization failed."))return 1;
    QSqlDatabase db=DatabaseManager::instance().database();QSqlQuery q(db);const QString now="2026-01-01T00:00:00.000Z";
    if(!require(q.exec("INSERT INTO set_catalog(set_number,name,year,theme_id,num_parts,image_url,created_utc,modified_utc) VALUES('1234-1','Set',2026,1,5,NULL,'"+now+"','"+now+"')"),"Set seed failed."))return 1;const int setId=q.lastInsertId().toInt();
    if(!require(q.exec("INSERT INTO workspace(name,description,created_utc,modified_utc) VALUES('Test','', '"+now+"','"+now+"')"),"Workspace seed failed."))return 1;const int workspaceId=q.lastInsertId().toInt();
    Build linkedBuild; linkedBuild.setWorkspaceId(workspaceId); linkedBuild.setBuildType("Set"); linkedBuild.setName("Serialization only"); linkedBuild.setSetNumber("1234-1"); linkedBuild.setSetCatalogId(setId); linkedBuild.setInventoryMode("Stock"); linkedBuild.setStatus("Planned");
    BuildRepository buildRepository;
    if(!require(buildRepository.create(linkedBuild),"Set catalog Build-field serialization failed."))return 1;
    const auto roundTripBuild=buildRepository.getById(linkedBuild.id());
    if(!require(roundTripBuild&&roundTripBuild->setCatalogId()==setId&&roundTripBuild->setNumber()=="1234-1","Set catalog Build-field round trip failed."))return 1;
    if(!require(q.exec("INSERT INTO part(part_number,name,rebrickable_part_id,is_active,created_utc,modified_utc,material) VALUES('p1','Part 1','p1',1,'"+now+"','"+now+"','Plastic'),('p2','Part 2','p2',1,'"+now+"','"+now+"','Plastic')"),"Part seed failed."))return 1;
    if(!require(q.exec("INSERT INTO color(name,rebrickable_id,created_utc,modified_utc) VALUES('Red',1,'"+now+"','"+now+"'),('Blue',2,'"+now+"','"+now+"')"),"Color seed failed."))return 1;
    const QByteArray csv="Part,Color,Quantity,Is Spare\np1,1,2,false\nP1,1,3,0\np1,1,1,true\np2,2,4,no\n";const QString direct=files.filePath("rebrickable_parts_1234-1-test.csv");writeBytes(direct,csv);
    RebrickableSetPartsImporter importer;auto imported=importer.importFile(setId,direct);SetCatalogPartRepository repository;auto parts=repository.listForSet(setId);auto counts=repository.countsForSet(setId);
    if(!require(imported.success&&parts.size()==3&&parts.at(0).quantityRequired==5&&parts.at(2).isSpare&&counts.requiredPieces==9&&counts.sparePieces==1&&parts.at(0).source=="Rebrickable Set parts CSV/ZIP","CSV persistence/deduplication/counts failed: "+imported.message))return 1;
    const QString zip=files.filePath("rebrickable_parts_1234-1-test.csv.zip");if(!require(writeZip(zip,"nested/rebrickable_parts_1234-1-test.csv","Part,Color,Quantity\np2,2,7\n"),"ZIP fixture failed."))return 1;
    imported=importer.importFile(setId,zip);if(!require(imported.success&&repository.listForSet(setId).size()==1&&repository.listForSet(setId).first().quantityRequired==7,"Nested ZIP replacement failed."))return 1;
    const QString baseline=snapshot(db,setId);const QString wrong=files.filePath("rebrickable_parts_9999-1.csv");writeBytes(wrong,csv);imported=importer.importFile(setId,wrong);if(!require(!imported.success&&snapshot(db,setId)==baseline,"Set reference mismatch mutated composition."))return 1;
    auto rejected=SetCompositionReplacementService().replace(setId,{{"missing",1,1,false,"test"}},"Rebrickable","API");if(!require(!rejected.success&&snapshot(db,setId)==baseline,"Unresolved Part mutated composition."))return 1;
    rejected=SetCompositionReplacementService().replace(setId,{{"p1",999,1,false,"test"}},"Rebrickable","API");if(!require(!rejected.success&&snapshot(db,setId)==baseline,"Unresolved Color mutated composition."))return 1;
    if(!require(q.exec("CREATE TRIGGER force_set_failure BEFORE INSERT ON set_catalog_part BEGIN SELECT RAISE(ABORT,'forced late failure'); END"),"Failure trigger failed."))return 1;
    rejected=SetCompositionReplacementService().replace(setId,{{"p1",1,2,false,"API row 1"}},"Rebrickable","Rebrickable API: Set parts (including Minifig parts)");if(!require(!rejected.success&&snapshot(db,setId)==baseline,"Late failure did not roll back replacement."))return 1;q.exec("DROP TRIGGER force_set_failure");
    auto api=SetCompositionReplacementService().replace(setId,{{"p1",1,2,false,"1"},{"P1",1,3,false,"2"},{"p1",1,1,true,"3"}},"Rebrickable","Rebrickable API: Set parts (including Minifig parts)");parts=repository.listForSet(setId);
    if(!require(api.success&&parts.size()==2&&parts.at(0).quantityRequired==5&&parts.at(1).isSpare&&parts.at(0).provider=="Rebrickable","API-equivalent replacement/provenance failed."))return 1;
    if(!require(q.exec("INSERT INTO part(part_number,name,rebrickable_part_id,is_active,created_utc,modified_utc,material) VALUES('p3','Ambiguous','P1',1,'"+now+"','"+now+"','Plastic')"),"Ambiguous seed failed."))return 1;const QString apiBaseline=snapshot(db,setId);
    rejected=SetCompositionReplacementService().replace(setId,{{"p1",1,1,false,"test"}},"Rebrickable","API");if(!require(!rejected.success&&rejected.message.contains("ambiguous")&&snapshot(db,setId)==apiBaseline,"Ambiguous Part was accepted."))return 1;
    qInfo()<<"M23.10.1 Set composition foundation validation passed.";return 0;
}
