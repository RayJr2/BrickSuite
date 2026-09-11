#include "../src/repositories/BuildRepository.h"
#include "../src/repositories/CollectionRepository.h"
#include "../src/services/collection/CollectionItemService.h"

#include <QCoreApplication>
#include <QDebug>
#include <QSqlDatabase>
#include <QSqlQuery>

namespace {
bool check(bool value, const char* message)
{
    if (!value) qCritical() << "FAILED:" << message;
    return value;
}

int scalar(QSqlDatabase db, const QString& sql)
{
    QSqlQuery query(db);
    return query.exec(sql) && query.next() ? query.value(0).toInt() : -1;
}

bool seed(QSqlDatabase db)
{
    QSqlQuery q(db);
    const QStringList sql = {
        "PRAGMA foreign_keys=ON",
        "CREATE TABLE workspace(id INTEGER PRIMARY KEY,name TEXT,description TEXT,created_utc TEXT,modified_utc TEXT,is_active INTEGER)",
        "CREATE TABLE storage_location(id INTEGER PRIMARY KEY,workspace_id INTEGER,parent_location_id INTEGER,location_type_id INTEGER,name TEXT,description TEXT,sort_order INTEGER,is_active INTEGER,allows_inventory INTEGER,allows_collection INTEGER,created_utc TEXT,modified_utc TEXT)",
        "CREATE TABLE set_catalog(id INTEGER PRIMARY KEY,set_number TEXT,name TEXT,year INTEGER,theme_id INTEGER,num_parts INTEGER,image_url TEXT,created_utc TEXT,modified_utc TEXT)",
        "CREATE TABLE minifig_catalog(id INTEGER PRIMARY KEY,name TEXT,num_parts INTEGER,image_url TEXT,is_active INTEGER,created_utc TEXT,modified_utc TEXT)",
        "CREATE TABLE minifig_external_identifier(id INTEGER PRIMARY KEY,minifig_catalog_id INTEGER,provider TEXT,external_id TEXT,is_active INTEGER)",
        "CREATE TABLE manufacturer(id INTEGER PRIMARY KEY,name TEXT,is_active INTEGER)",
        "CREATE TABLE build(id INTEGER PRIMARY KEY,workspace_id INTEGER,build_type TEXT,name TEXT,set_number TEXT,set_catalog_id INTEGER,minifig_catalog_id INTEGER,inventory_mode TEXT,manufacturer_id INTEGER,status TEXT,is_active INTEGER,notes TEXT,created_utc TEXT,modified_utc TEXT)",
        "CREATE TABLE collection_item(id INTEGER PRIMARY KEY AUTOINCREMENT,workspace_id INTEGER,item_type TEXT,set_catalog_id INTEGER,minifig_catalog_id INTEGER,state TEXT,condition TEXT,completeness TEXT,storage_location_id INTEGER,source_build_id INTEGER UNIQUE,nickname TEXT,notes TEXT,allow_parts_source INTEGER,is_active INTEGER,created_utc TEXT,modified_utc TEXT)",
        "INSERT INTO workspace VALUES(1,'Test','','x','x',1)",
        "INSERT INTO storage_location VALUES(10,1,NULL,1,'Display','',0,1,0,1,'x','x')",
        "INSERT INTO set_catalog VALUES(20,'1000-1','Set',2026,1,10,'','x','x')",
        "INSERT INTO minifig_catalog VALUES(30,'Fig',4,'',1,'x','x')",
        "INSERT INTO minifig_external_identifier VALUES(1,30,'Rebrickable','fig-1',1)",
        "INSERT INTO manufacturer VALUES(1,'LEGO',1)",
        "INSERT INTO build VALUES(40,1,'Set','Linked Set','1000-1',20,NULL,'Stock',1,'Complete',1,'','x','x')",
        "INSERT INTO build VALUES(41,1,'Minifig','Linked Fig','',NULL,30,'Stock',1,'Complete',1,'','x','x')",
        "INSERT INTO build VALUES(42,1,'MOC','MOC','MOC-1',NULL,NULL,'Stock',1,'Complete',1,'','x','x')",
        "INSERT INTO build VALUES(43,1,'Set','Legacy','1000-1',NULL,NULL,'CompleteSet',1,'Complete',1,'','x','x')"
    };
    for (const QString& statement : sql) if (!q.exec(statement)) return false;
    return true;
}
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    bool ok = true;
    {
        QSqlDatabase poisoned = QSqlDatabase::addDatabase("QSQLITE");
        poisoned.setDatabaseName(":memory:");
        ok &= check(poisoned.open(), "open poisoned default connection");
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "collection-seams");
        db.setDatabaseName(":memory:");
        ok &= check(db.open() && seed(db), "seed explicit named connection");
        CollectionItemService service(db);

        ok &= check(db.transaction(), "begin create rollback");
        const auto set = service.createSetInCurrentTransaction(1, 20,
            CollectionItemState::Sealed, 10, 0, "Copy", "notes");
        ok &= check(set.success && set.item.id > 0 && set.item.workspaceId == 1
                    && set.item.type == CollectionItemType::Set && set.changed,
                    "Set seam returns authoritative item");
        ok &= check(scalar(db, "SELECT COUNT(*) FROM collection_item") == 1,
                    "Set visible inside caller transaction");
        ok &= check(db.rollback() && scalar(db, "SELECT COUNT(*) FROM collection_item") == 0,
                    "Set creation rolls back");

        ok &= check(db.transaction(), "begin multi-create commit");
        const auto fig = service.createMinifigInCurrentTransaction(1, 30,
            CollectionItemState::Assembled, 10);
        const auto linked = service.createFromBuildInCurrentTransaction(1, 40,
            CollectionItemState::Assembled, 10);
        const auto moc = service.createFromBuildInCurrentTransaction(1, 42,
            CollectionItemState::Assembled, 10);
        ok &= check(fig.success && linked.success && moc.success && db.commit(),
                    "Minifig and linked Set/MOC seams commit");
        ok &= check(scalar(db, "SELECT COUNT(*) FROM collection_item") == 3,
                    "caller commit persists creates only on explicit connection");
        ok &= check(scalar(poisoned, "SELECT COUNT(*) FROM sqlite_master WHERE name='collection_item'") == 0,
                    "poisoned default connection remains untouched");

        ok &= check(db.transaction(), "begin linked Minifig rollback");
        ok &= check(service.createFromBuildInCurrentTransaction(1, 41,
            CollectionItemState::Assembled).success, "linked Minifig create seam");
        ok &= check(db.rollback() && scalar(db,
            "SELECT COUNT(*) FROM collection_item WHERE source_build_id=41") == 0,
            "linked Minifig creation rolls back");

        const int itemId = fig.collectionItemId;
        ok &= check(db.transaction(), "begin edit rollback");
        const auto edited = service.updateDetailsInCurrentTransaction(itemId,
            CollectionItemState::PartiallyAssembled, 0, "Changed", "changed", false,
            CollectionItemCondition::New, CollectionItemCompleteness::Incomplete);
        ok &= check(edited.success && edited.item.nickname == "Changed", "edit seam mutates all fields");
        ok &= check(db.rollback() && scalar(db,
            QString("SELECT COUNT(*) FROM collection_item WHERE id=%1 AND state='Assembled' AND condition='Used' AND completeness='Unknown' AND storage_location_id=10 AND COALESCE(nickname,'')='' AND COALESCE(notes,'')=''").arg(itemId)) == 1,
            "edit rollback restores complete row");

        ok &= check(db.transaction(), "begin archive rollback");
        ok &= check(service.setActiveInCurrentTransaction(itemId, false).success,
                    "archive seam succeeds");
        ok &= check(db.rollback() && scalar(db,
            QString("SELECT is_active FROM collection_item WHERE id=%1").arg(itemId)) == 1,
            "archive rollback restores active flag");
        ok &= check(service.setActive(itemId, false).success
                    && scalar(db, QString("SELECT is_active FROM collection_item WHERE id=%1").arg(itemId)) == 0
                    && service.setActive(itemId, true).success,
                    "local archive/reactivate wrappers commit");

        ok &= check(db.transaction(), "begin legacy link rollback");
        ok &= check(service.linkLegacySetBuildInCurrentTransaction(43, 20).success,
                    "legacy link seam succeeds");
        ok &= check(db.rollback() && scalar(db, "SELECT set_catalog_id IS NULL FROM build WHERE id=43") == 1,
                    "legacy link rolls back");
        ok &= check(service.linkLegacySetBuild(43, 20).success
                    && scalar(db, "SELECT set_catalog_id FROM build WHERE id=43") == 20,
                    "local legacy link wrapper commits");

        ok &= check(db.transaction(), "begin disassembly sync rollback");
        const auto synchronized = service.updateStateForDisassemblyInCurrentTransaction(
            40, CollectionItemState::Unassembled);
        ok &= check(synchronized.success && synchronized.item.state == CollectionItemState::Unassembled,
                    "disassembly seam returns authoritative state");
        ok &= check(db.rollback() && scalar(db,
            "SELECT COUNT(*) FROM collection_item WHERE source_build_id=40 AND state='Assembled'") == 1,
            "disassembly synchronization rolls back with outer transaction");

        db.close(); poisoned.close();
    }
    QSqlDatabase::removeDatabase("collection-seams");
    QSqlDatabase::removeDatabase(QSqlDatabase::defaultConnection);
    return ok ? 0 : 1;
}
