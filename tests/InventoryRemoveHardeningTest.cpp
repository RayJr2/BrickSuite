#include "../src/database/DatabaseManager.h"
#include "../src/repositories/InventoryRecordRepository.h"

#include <QCoreApplication>
#include <QDir>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QUuid>

#include <cstdio>

namespace
{
bool require(bool condition, const QString& message)
{
    if (!condition)
        std::fprintf(stderr, "%s\n", message.toUtf8().constData());
    return condition;
}

QVariant scalar(QSqlDatabase database, const QString& sql)
{
    QSqlQuery query(database);
    return query.exec(sql) && query.next() ? query.value(0) : QVariant();
}

class Cleanup
{
public:
    explicit Cleanup(QString path) : m_path(std::move(path)) {}
    ~Cleanup()
    {
        DatabaseManager::instance().close();
        QDir(m_path).removeRecursively();
    }

private:
    QString m_path;
};
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setOrganizationName(QStringLiteral("RFStateSideTests"));
    QCoreApplication::setApplicationName(
        QStringLiteral("InventoryRemove_")
        + QUuid::createUuid().toString(QUuid::WithoutBraces));
    const QString dataPath =
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    Cleanup cleanup(dataPath);

    if (!require(DatabaseManager::instance().initialize(),
                 QStringLiteral("Database initialization failed."))) return 1;

    QSqlDatabase database = DatabaseManager::instance().database();
    QSqlQuery query(database);
    const QString now = QStringLiteral("2026-09-07T12:00:00.000Z");
    if (!require(query.exec("INSERT INTO workspace(name,description,created_utc,modified_utc,is_active) "
                            "VALUES('Test Workspace','', '" + now + "','" + now + "',1)"),
                 QStringLiteral("Workspace seed failed: ") + query.lastError().text())) return 1;
    const int workspaceId = query.lastInsertId().toInt();
    const int locationTypeId = scalar(database,
        "SELECT id FROM storage_location_type WHERE is_active=1 ORDER BY id LIMIT 1").toInt();
    query.prepare("INSERT INTO storage_location(workspace_id,location_type_id,name,is_active,"
                  "created_utc,modified_utc) VALUES(:workspace,:type,'Test Bin',1,:now,:now)");
    query.bindValue(":workspace", workspaceId);
    query.bindValue(":type", locationTypeId);
    query.bindValue(":now", now);
    if (!require(query.exec(), QStringLiteral("Storage seed failed: ") + query.lastError().text())) return 1;
    const int storageId = query.lastInsertId().toInt();
    const int manufacturerId = scalar(database,
        "SELECT id FROM manufacturer WHERE is_active=1 ORDER BY id LIMIT 1").toInt();
    if (!require(workspaceId > 0 && storageId > 0 && manufacturerId > 0,
                 QStringLiteral("Required seed reference data is unavailable."))) return 1;

    query.prepare("INSERT INTO color(name,rgb,is_transparent,rebrickable_id,created_utc,modified_utc) "
                  "VALUES('Remove Color','123456',0,987654,:now,:now)");
    query.bindValue(":now", now);
    if (!require(query.exec(), QStringLiteral("Color seed failed: ") + query.lastError().text())) return 1;
    const int colorId = query.lastInsertId().toInt();

    int recordSequence = 0;
    auto addRecord = [&](int quantity) {
        QSqlQuery partInsert(database);
        partInsert.prepare("INSERT INTO part(part_number,name,is_active,created_utc,modified_utc,material) "
                           "VALUES(:number,:name,1,:now,:now,'Plastic')");
        const QString number = QString("remove-test-%1").arg(++recordSequence);
        partInsert.bindValue(":number", number);
        partInsert.bindValue(":name", number);
        partInsert.bindValue(":now", now);
        if (!partInsert.exec())
            return 0;
        const int recordPartId = partInsert.lastInsertId().toInt();
        QSqlQuery insert(database);
        insert.prepare("INSERT INTO inventory_record(workspace_id,part_id,color_id,storage_location_id,"
                       "manufacturer_id,condition,ownership_type,quantity,created_utc,modified_utc) "
                       "VALUES(:workspace,:part,:color,:storage,:manufacturer,'Used','Owned',:quantity,:now,:now)");
        insert.bindValue(":workspace", workspaceId);
        insert.bindValue(":part", recordPartId);
        insert.bindValue(":color", colorId);
        insert.bindValue(":storage", storageId);
        insert.bindValue(":manufacturer", manufacturerId);
        insert.bindValue(":quantity", quantity);
        insert.bindValue(":now", now);
        return insert.exec() ? insert.lastInsertId().toInt() : 0;
    };
    auto addBuild = [&]() {
        QSqlQuery insert(database);
        insert.prepare("INSERT INTO build(workspace_id,build_type,name,inventory_mode,status,is_active,"
                       "created_utc,modified_utc) VALUES(:workspace,'MOC','Allocation Test','Stock',"
                       "'Planned',1,:now,:now)");
        insert.bindValue(":workspace", workspaceId);
        insert.bindValue(":now", now);
        return insert.exec() ? insert.lastInsertId().toInt() : 0;
    };
    auto addAllocation = [&](int recordId, int quantity) {
        const int recordPartId = scalar(
            database, QString("SELECT part_id FROM inventory_record WHERE id=%1").arg(recordId)).toInt();
        QSqlQuery insert(database);
        insert.prepare("INSERT INTO build_allocation(build_id,inventory_record_id,part_id,color_id,"
                       "storage_location_id,quantity_allocated,created_utc,modified_utc) "
                       "VALUES(:build,:record,:part,:color,:storage,:quantity,:now,:now)");
        insert.bindValue(":build", addBuild());
        insert.bindValue(":record", recordId);
        insert.bindValue(":part", recordPartId);
        insert.bindValue(":color", colorId);
        insert.bindValue(":storage", storageId);
        insert.bindValue(":quantity", quantity);
        insert.bindValue(":now", now);
        return insert.exec() ? insert.lastInsertId().toInt() : 0;
    };
    auto quantity = [&](int recordId) {
        return scalar(database, QString("SELECT quantity FROM inventory_record WHERE id=%1").arg(recordId)).toInt();
    };
    auto movementCount = [&](int recordId) {
        return scalar(database, QString("SELECT COUNT(*) FROM inventory_movement WHERE "
                                        "inventory_record_id=%1 AND movement_type='EntryRemoved'")
                                    .arg(recordId)).toInt();
    };

    InventoryRecordRepository repository;
    QString error;

    const int unallocated = addRecord(10);
    if (!require(repository.removeEntry(unallocated, 3, "Count correction", &error)
                 && quantity(unallocated) == 7,
                 QStringLiteral("Unallocated partial removal failed: ") + error)) return 1;
    if (!require(scalar(database, QString("SELECT quantity_change || '|' || reference_type || '|' || notes "
                                         "FROM inventory_movement WHERE inventory_record_id=%1 "
                                         "AND movement_type='EntryRemoved'").arg(unallocated)).toString()
                     == QStringLiteral("-3|Correction|Count correction"),
                 QStringLiteral("Removal history fields were not preserved."))) return 1;
    if (!require(repository.removeEntry(unallocated, 7, QString(), &error)
                 && quantity(unallocated) == 0
                 && scalar(database, QString("SELECT COUNT(*) FROM inventory_record WHERE id=%1")
                                        .arg(unallocated)).toInt() == 1
                 && scalar(database, QString("SELECT notes FROM inventory_movement WHERE "
                                             "inventory_record_id=%1 ORDER BY id DESC LIMIT 1")
                                        .arg(unallocated)).toString()
                        == QStringLiteral("Inventory entry removed as a correction."),
                 QStringLiteral("Full history-preserving removal failed: ") + error)) return 1;

    const int partialAllocation = addRecord(10);
    if (!require(addAllocation(partialAllocation, 3) > 0,
                 QStringLiteral("Partial allocation seed failed."))) return 1;
    if (!require(repository.removeEntry(partialAllocation, 7, QString(), &error)
                 && quantity(partialAllocation) == 3,
                 QStringLiteral("Exact unallocated removal was rejected: ") + error)) return 1;
    const int beforeRejectedMovements = movementCount(partialAllocation);
    if (!require(!repository.removeEntry(partialAllocation, 1, QString(), &error)
                 && error.contains("allocated", Qt::CaseInsensitive)
                 && quantity(partialAllocation) == 3
                 && movementCount(partialAllocation) == beforeRejectedMovements,
                 QStringLiteral("Over-allocation removal was not safely rejected."))) return 1;

    const int fullyAllocated = addRecord(4);
    const int releasedAllocation = addAllocation(fullyAllocated, 4);
    if (!require(releasedAllocation > 0
                 && !repository.removeEntry(fullyAllocated, 1, QString(), &error)
                 && error.contains("all", Qt::CaseInsensitive)
                 && quantity(fullyAllocated) == 4
                 && movementCount(fullyAllocated) == 0,
                 QStringLiteral("Fully allocated inventory was removable."))) return 1;
    if (!require(query.exec(QString("DELETE FROM build_allocation WHERE id=%1").arg(releasedAllocation))
                 && repository.removeEntry(fullyAllocated, 4, QString(), &error)
                 && quantity(fullyAllocated) == 0,
                 QStringLiteral("Released allocation still blocked removal: ") + error)) return 1;

    const int staleRecord = addRecord(4);
    if (!require(addAllocation(staleRecord, 2) > 0,
                 QStringLiteral("Stale-state allocation seed failed."))) return 1;
    if (!require(!repository.removeEntry(staleRecord, 3, QString(), &error)
                 && quantity(staleRecord) == 4 && movementCount(staleRecord) == 0,
                 QStringLiteral("Authoritative removal did not catch changed allocation state."))) return 1;

    const int rollbackRecord = addRecord(5);
    if (!require(query.exec("CREATE TRIGGER fail_removed_movement BEFORE INSERT ON inventory_movement "
                            "WHEN NEW.movement_type='EntryRemoved' BEGIN SELECT RAISE(ABORT,'forced'); END"),
                 QStringLiteral("Rollback trigger creation failed."))) return 1;
    if (!require(!repository.removeEntry(rollbackRecord, 2, QString(), &error)
                 && quantity(rollbackRecord) == 5 && movementCount(rollbackRecord) == 0,
                 QStringLiteral("Movement failure did not roll back quantity."))) return 1;

    qInfo() << "Inventory Remove hardening validation passed.";
    return 0;
}
