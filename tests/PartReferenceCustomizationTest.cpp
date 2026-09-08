#include "../src/database/DatabaseManager.h"
#include "../src/database/DatabaseSchema.h"
#include "../src/repositories/PartRepository.h"
#include "../src/repositories/UserPartReferenceRepository.h"
#include "../src/services/parts/PartReferenceCustomizationService.h"
#include "../src/services/parts/PartReferenceManifest.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QUuid>
#include <cstdio>

namespace {
bool check(bool value, const QString& message) { if (!value) std::fprintf(stderr, "%s\n", message.toUtf8().constData()); return value; }
QVariant scalar(QSqlDatabase db, const QString& sql) { QSqlQuery q(db); return q.exec(sql) && q.next() ? q.value(0) : QVariant(); }
class Cleanup { QString path; public: explicit Cleanup(QString p):path(std::move(p)){} ~Cleanup(){DatabaseManager::instance().close(); QDir(path).removeRecursively();} };
int indexOf(const QList<PartReferenceEntry>& entries, const QString& number) { for (int i=0;i<entries.size();++i) if(entries.at(i).partNumber.compare(number,Qt::CaseInsensitive)==0)return i; return -1; }
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv); QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setOrganizationName("RFStateSideTests");
    QCoreApplication::setApplicationName("PartReference_"+QUuid::createUuid().toString(QUuid::WithoutBraces));
    const QString root=QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation); Cleanup cleanup(root);
    QDir().mkpath(root);

    // Representative schema-31 migration with existing domain sentinels.
    const QString legacyPath=QDir(root).filePath("schema31.db"); const QString connection="schema31";
    {
        QSqlDatabase legacy=QSqlDatabase::addDatabase("QSQLITE",connection); legacy.setDatabaseName(legacyPath);
        if(!check(legacy.open(),"open schema 31"))return 1; QSqlQuery q(legacy);
        if(!check(q.exec("CREATE TABLE schema_version(version INTEGER NOT NULL)"),"legacy version table")
            || !check(q.exec("INSERT INTO schema_version VALUES(31)"),"legacy version")
            || !check(q.exec("CREATE TABLE part(id INTEGER PRIMARY KEY,part_number TEXT NOT NULL UNIQUE,name TEXT NOT NULL,part_category_id INTEGER,rebrickable_part_id TEXT,material TEXT NOT NULL DEFAULT '',is_active INTEGER NOT NULL DEFAULT 1,created_utc TEXT NOT NULL,modified_utc TEXT NOT NULL)"),"legacy part")
            || !check(q.exec("INSERT INTO part VALUES(1,'legacy','Legacy',NULL,NULL,'Plastic',1,'x','x')"),"legacy part row")
            || !check(q.exec("CREATE TABLE migration_sentinel(kind TEXT PRIMARY KEY,value TEXT)"),"legacy sentinels")
            || !check(q.exec("INSERT INTO migration_sentinel VALUES('inventory','kept'),('build','kept'),('collection','kept'),('catalog','kept')"),"legacy sentinel rows")
            || !check(DatabaseSchema::initialize(legacy),"31 to 32 migration")
            || !check(scalar(legacy,"SELECT version FROM schema_version").toInt()==33,"schema migrated to 33")
            || !check(scalar(legacy,"SELECT count(*) FROM migration_sentinel WHERE value='kept'").toInt()==4,"existing data retained")
            || !check(scalar(legacy,"SELECT count(*) FROM sqlite_master WHERE type='table' AND name='user_part_reference_entry'").toInt()==1,"new table exists")
            || !check(scalar(legacy,"SELECT count(*) FROM sqlite_master WHERE type='index' AND name='idx_user_part_reference_destination'").toInt()==1,"new index exists")
            || !check(DatabaseSchema::initialize(legacy),"migration idempotent")
            || !check(scalar(legacy,"SELECT count(*) FROM user_part_reference_entry").toInt()==0,"idempotence creates no rows")) return 1;
        legacy.close();
    }
    QSqlDatabase::removeDatabase(connection);

    if(!check(DatabaseManager::instance().initialize(),"fresh database initialization"))return 1;
    QSqlDatabase db=DatabaseManager::instance().database();
    if(!check(scalar(db,"SELECT version FROM schema_version").toInt()==33,"fresh schema is 33"))return 1;
    PartReferenceManifest manifest; QString manifestError;
    if(!check(manifest.load(&manifestError),"built-in manifest loads: "+manifestError)
       || !check(manifest.entryCount()==PartReferenceManifest::ExpectedEntryCount,"built-in count unchanged"))return 1;
    const PartReferenceEntry* correctedEntry = manifest.findByPartNumber("4032a");
    if(!check(correctedEntry != nullptr,"production Part Reference contains 4032a")
       || !check(correctedEntry->partName == "Plate Round 2 x 2 with Axle Hole Type 1 (+ Opening)","4032a uses canonical catalog name")
       || !check(correctedEntry->catalog == "Plates" && correctedEntry->section == "Round & Curved","4032a catalog and family retained")
       || !check(correctedEntry->displayOrder == 4 && correctedEntry->sourceCategoryId == 21
                 && correctedEntry->sourceCategory == "Plates Round Curved and Dishes","4032a ordering and category metadata retained")
       || !check(manifest.findByPartNumber("4032b") == nullptr,"4032b no longer occupies a built-in reference entry"))return 1;
    QFile catalogParts(":/rebrickable/parts.csv");
    if(!check(catalogParts.open(QIODevice::ReadOnly | QIODevice::Text),"embedded Parts catalog opens"))return 1;
    const QByteArray catalogData = catalogParts.readAll();
    if(!check(catalogData.contains("4032a,Plate Round 2 x 2 with Axle Hole Type 1 (+ Opening),21,Plastic"),"4032a remains a distinct canonical catalog Part")
       || !check(catalogData.contains("4032b,Plate Round 2 x 2 with Axle Hole Type 2 (X Opening),21,Plastic"),"4032b remains a distinct canonical catalog Part"))return 1;
    PartReferenceCustomizationService service(manifest);
    PartReferenceDestination ordinary;
    for(const auto& d:service.destinations()) if(!d.structured){ordinary=d;break;}
    if(!check(!ordinary.catalog.isEmpty(),"ordinary destination available"))return 1;
    PartReferenceEntry anchor;
    for(const auto& e:manifest.entries()) if(e.catalog==ordinary.catalog&&e.section==ordinary.section){anchor=e;break;}

    auto addPart=[&](const QString& number,bool active=true){ Part p; p.setPartNumber(number); p.setName("Test "+number); p.setMaterial("Plastic"); p.setIsActive(active); return PartRepository().create(p)?p.id():0; };
    const int beforeId=addPart("user-before"), afterId=addPart("user-after"), appendId=addPart("user-append"), secondBeforeId=addPart("user-before-2");
    if(!check(beforeId&&afterId&&appendId&&secondBeforeId,"test parts created"))return 1;
    if(!check(service.add(beforeId,ordinary.catalog,ordinary.section,PartReferencePlacement::Before,anchor.partNumber).success,"before add")
       || !check(service.add(afterId,ordinary.catalog,ordinary.section,PartReferencePlacement::After,anchor.partNumber).success,"after add")
       || !check(service.add(appendId,ordinary.catalog,ordinary.section,PartReferencePlacement::Append).success,"append add")
       || !check(service.add(secondBeforeId,ordinary.catalog,ordinary.section,PartReferencePlacement::Before,anchor.partNumber).success,"second before add"))return 1;
    QList<PartReferenceEntry> effective=service.effectiveEntries(); const int anchorIndex=indexOf(effective,anchor.partNumber);
    if(!check(anchorIndex>=2 && effective.at(anchorIndex-2).partNumber=="user-before" && effective.at(anchorIndex-1).partNumber=="user-before-2","deterministic before ordering")
       || !check(effective.at(anchorIndex+1).partNumber=="user-after","after placement")
       || !check(indexOf(effective,"user-append")>anchorIndex,"append placement")
       || !check(service.effectiveEntries().size()==effective.size(),"entries persist and reload"))return 1;

    const int duplicateBuiltin=addPart(anchor.partNumber);
    if(!check(duplicateBuiltin>0 && !service.add(duplicateBuiltin,ordinary.catalog,ordinary.section,PartReferencePlacement::Append).success,"built-in duplicate rejected")
       || !check(!service.add(beforeId,ordinary.catalog,ordinary.section,PartReferencePlacement::Append).success,"user duplicate globally rejected"))return 1;

    // Missing/moved anchors fall back to append in the stored destination.
    const int missingId=addPart("missing-anchor"), movedId=addPart("moved-anchor");
    UserPartReferenceEntry missing; missing.partId=missingId; missing.catalog=ordinary.catalog; missing.section=ordinary.section; missing.placement=PartReferencePlacement::Before; missing.anchorPartNumber="gone";
    UserPartReferenceEntry moved=missing; moved.partId=movedId; moved.anchorPartNumber=manifest.entries().last().partNumber;
    if(!check(UserPartReferenceRepository().create(missing),"store missing anchor")||!check(UserPartReferenceRepository().create(moved),"store moved anchor"))return 1;
    effective=service.effectiveEntries();
    if(!check(indexOf(effective,"missing-anchor")>anchorIndex && indexOf(effective,"moved-anchor")>anchorIndex,"invalid anchors append locally"))return 1;

    // A future built-in collision suppresses display but retains its row.
    UserPartReferenceEntry collision; collision.partId=duplicateBuiltin; collision.catalog=ordinary.catalog; collision.section=ordinary.section;
    if(!check(UserPartReferenceRepository().create(collision),"store future collision")
       || !check(service.effectiveEntries().size()==effective.size(),"future built-in collision suppressed")
       || !check(scalar(db,"SELECT count(*) FROM user_part_reference_entry WHERE id="+QString::number(collision.id)).toInt()==1,"suppressed row retained"))return 1;

    // Structured entries append to the Other gallery; relative placement is normalized.
    PartReferenceDestination structured; for(const auto& d:service.destinations())if(d.structured){structured=d;break;}
    const int structuredId=addPart("structured-user");
    if(!check(service.add(structuredId,structured.catalog,structured.section,PartReferencePlacement::Before,manifest.entries().first().partNumber).success,"structured add")
       || !check(scalar(db,"SELECT placement_mode FROM user_part_reference_entry WHERE part_id="+QString::number(structuredId)).toString()=="Append","structured placement protected"))return 1;

    const int inactiveId=addPart("inactive-user",false); UserPartReferenceEntry inactive; inactive.partId=inactiveId; inactive.catalog=ordinary.catalog; inactive.section=ordinary.section;
    UserPartReferenceEntry invalid=inactive; invalid.partId=addPart("invalid-destination"); invalid.catalog="Not a catalog";
    if(!check(UserPartReferenceRepository().create(inactive),"inactive row stored")||!check(UserPartReferenceRepository().create(invalid),"invalid destination stored")
       || !check(indexOf(service.effectiveEntries(),"inactive-user")==-1 && indexOf(service.effectiveEntries(),"invalid-destination")==-1,"unusable rows safely suppressed"))return 1;

    const int removePartId=beforeId; const int userRow=scalar(db,"SELECT id FROM user_part_reference_entry WHERE part_id="+QString::number(removePartId)).toInt();
    QSqlQuery protectedPartDelete(db); protectedPartDelete.prepare("DELETE FROM part WHERE id=?"); protectedPartDelete.addBindValue(removePartId);
    if(!check(!protectedPartDelete.exec(),"foreign key protects referenced Part"))return 1;
    if(!check(!service.remove(0).success,"built-in removal rejected") || !check(service.remove(userRow).success,"user removal succeeds")
       || !check(PartRepository().getById(removePartId).has_value(),"removal preserves catalog Part")
       || !check(indexOf(service.effectiveEntries(),"user-before")==-1,"removed customization refreshes"))return 1;
    if(!check(scalar(db,"PRAGMA foreign_keys").toInt()==1,"foreign keys enabled"))return 1;
    return 0;
}
