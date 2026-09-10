#include "../src/database/DatabaseSchema.h"
#include "../src/repositories/InventoryRecordRepository.h"

#include <QCoreApplication>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>

#include <cstdio>

namespace {
bool require(bool condition, const QString& message)
{
    if (!condition)
        std::fprintf(stderr, "%s\n", message.toUtf8().constData());
    return condition;
}

QVariant scalar(const QSqlDatabase& database, const QString& sql)
{
    QSqlQuery query(database);
    return query.exec(sql) && query.next() ? query.value(0) : QVariant();
}

struct Fixture
{
    QTemporaryDir directory;
    QString connectionName = QStringLiteral("inventory-seams-%1")
                                 .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    QSqlDatabase database;
    int workspaceId = 0;
    int storageId = 0;
    int secondStorageId = 0;
    int colorId = 0;
    int manufacturerId = 0;
    int sequence = 0;

    bool initialize()
    {
        database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(directory.filePath(QStringLiteral("seams.db")));
        if (!database.open() || !DatabaseSchema::initialize(database))
            return false;

        QSqlQuery query(database);
        if (!query.exec("INSERT INTO workspace(name,description,created_utc,modified_utc,is_active) "
                        "VALUES('Seam Test','',CURRENT_TIMESTAMP,CURRENT_TIMESTAMP,1)"))
            return false;
        workspaceId = query.lastInsertId().toInt();
        const int typeId = scalar(database,
            "SELECT id FROM storage_location_type WHERE is_active=1 ORDER BY id LIMIT 1").toInt();
        manufacturerId = scalar(database,
            "SELECT id FROM manufacturer WHERE is_active=1 ORDER BY id LIMIT 1").toInt();

        query.prepare("INSERT INTO storage_location(workspace_id,location_type_id,name,is_active,"
                      "created_utc,modified_utc) VALUES(:workspace,:type,'Seam Bin',1,"
                      "CURRENT_TIMESTAMP,CURRENT_TIMESTAMP)");
        query.bindValue(":workspace", workspaceId);
        query.bindValue(":type", typeId);
        if (!query.exec()) return false;
        storageId = query.lastInsertId().toInt();
        query.prepare("INSERT INTO storage_location(workspace_id,location_type_id,name,is_active,"
                      "created_utc,modified_utc) VALUES(:workspace,:type,'Second Seam Bin',1,"
                      "CURRENT_TIMESTAMP,CURRENT_TIMESTAMP)");
        query.bindValue(":workspace", workspaceId);
        query.bindValue(":type", typeId);
        if (!query.exec()) return false;
        secondStorageId = query.lastInsertId().toInt();

        query.prepare("INSERT INTO color(name,rgb,is_transparent,rebrickable_id,created_utc,modified_utc) "
                      "VALUES('Seam Color','123456',0,876543,CURRENT_TIMESTAMP,CURRENT_TIMESTAMP)");
        if (!query.exec()) return false;
        colorId = query.lastInsertId().toInt();
        return workspaceId > 0 && storageId > 0 && secondStorageId > 0
               && colorId > 0 && manufacturerId > 0;
    }

    int addPart()
    {
        QSqlQuery query(database);
        const QString number = QStringLiteral("seam-part-%1").arg(++sequence);
        query.prepare("INSERT INTO part(part_number,name,is_active,created_utc,modified_utc,material) "
                      "VALUES(:number,:number,1,CURRENT_TIMESTAMP,CURRENT_TIMESTAMP,'Plastic')");
        query.bindValue(":number", number);
        return query.exec() ? query.lastInsertId().toInt() : 0;
    }

    int addRecord(int partId, int quantity, int targetStorageId = 0,
                  const QString& condition = QStringLiteral("Used"),
                  const QString& ownership = QStringLiteral("Owned"))
    {
        QSqlQuery query(database);
        query.prepare("INSERT INTO inventory_record(workspace_id,part_id,color_id,storage_location_id,"
                      "manufacturer_id,condition,ownership_type,quantity,created_utc,modified_utc) "
                      "VALUES(:workspace,:part,:color,:storage,:manufacturer,:condition,:ownership,:quantity,"
                      "CURRENT_TIMESTAMP,CURRENT_TIMESTAMP)");
        query.bindValue(":workspace", workspaceId);
        query.bindValue(":part", partId);
        query.bindValue(":color", colorId);
        query.bindValue(":storage", targetStorageId > 0 ? targetStorageId : storageId);
        query.bindValue(":manufacturer", manufacturerId);
        query.bindValue(":condition", condition);
        query.bindValue(":ownership", ownership);
        query.bindValue(":quantity", quantity);
        return query.exec() ? query.lastInsertId().toInt() : 0;
    }

    int quantity(int recordId) const
    {
        return scalar(database, QStringLiteral("SELECT quantity FROM inventory_record WHERE id=%1")
                                    .arg(recordId)).toInt();
    }

    int movements(int recordId, const QString& type) const
    {
        QSqlQuery query(database);
        query.prepare("SELECT COUNT(*) FROM inventory_movement WHERE inventory_record_id=:record "
                      "AND movement_type=:type");
        query.bindValue(":record", recordId);
        query.bindValue(":type", type);
        return query.exec() && query.next() ? query.value(0).toInt() : -1;
    }

    bool addAllocation(int recordId, int quantity)
    {
        const int partId = scalar(database,
            QStringLiteral("SELECT part_id FROM inventory_record WHERE id=%1").arg(recordId)).toInt();
        QSqlQuery query(database);
        query.prepare("INSERT INTO build(workspace_id,build_type,name,inventory_mode,status,is_active,"
                      "created_utc,modified_utc) VALUES(:workspace,'MOC','Seam Build','Stock','Planned',1,"
                      "CURRENT_TIMESTAMP,CURRENT_TIMESTAMP)");
        query.bindValue(":workspace", workspaceId);
        if (!query.exec()) return false;
        const int buildId = query.lastInsertId().toInt();
        query.prepare("INSERT INTO build_allocation(build_id,inventory_record_id,part_id,color_id,"
                      "storage_location_id,quantity_allocated,created_utc,modified_utc) "
                      "VALUES(:build,:record,:part,:color,:storage,:quantity,CURRENT_TIMESTAMP,CURRENT_TIMESTAMP)");
        query.bindValue(":build", buildId);
        query.bindValue(":record", recordId);
        query.bindValue(":part", partId);
        query.bindValue(":color", colorId);
        query.bindValue(":storage", storageId);
        query.bindValue(":quantity", quantity);
        return query.exec();
    }

    void close()
    {
        database.close();
        database = {};
        QSqlDatabase::removeDatabase(connectionName);
    }
};
}

int main(int argc, char** argv)
{
    QCoreApplication application(argc, argv);
    bool ok = true;

    const QString defaultName = QSqlDatabase::defaultConnection;
    QSqlDatabase defaultDatabase = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"));
    defaultDatabase.setDatabaseName(QStringLiteral(":memory:"));
    ok &= require(defaultDatabase.open(), QStringLiteral("Default isolation database did not open."));
    QSqlQuery defaultQuery(defaultDatabase);
    ok &= require(defaultQuery.exec("CREATE TABLE isolation_probe(value INTEGER NOT NULL)"),
                  QStringLiteral("Default isolation sentinel could not be created."));
    ok &= require(defaultQuery.exec("INSERT INTO isolation_probe VALUES(17)"),
                  QStringLiteral("Default isolation sentinel could not be seeded."));

    Fixture fixture;
    ok &= require(fixture.initialize(), QStringLiteral("Named schema fixture initialization failed."));
    InventoryRecordRepository repository(fixture.database);

    auto testRemove = [&](bool commit) {
        const int record = fixture.addRecord(fixture.addPart(), 8);
        QString error;
        ok &= require(fixture.database.transaction(), QStringLiteral("Remove outer transaction failed."));
        ok &= require(repository.removeEntryInCurrentTransaction(record, 3, QStringLiteral("seam"), &error),
                      QStringLiteral("Remove seam failed: ") + error);
        ok &= require(fixture.quantity(record) == 5 && fixture.movements(record, "EntryRemoved") == 1,
                      QStringLiteral("Remove seam effects were not visible inside the transaction."));
        ok &= require(commit ? fixture.database.commit() : fixture.database.rollback(),
                      QStringLiteral("Remove caller transaction completion failed."));
        ok &= require(fixture.quantity(record) == (commit ? 5 : 8)
                      && fixture.movements(record, "EntryRemoved") == (commit ? 1 : 0),
                      QStringLiteral("Remove outer commit/rollback did not govern all effects."));
    };
    testRemove(false);
    testRemove(true);

    const int allocated = fixture.addRecord(fixture.addPart(), 4);
    ok &= require(fixture.addAllocation(allocated, 4), QStringLiteral("Allocation seed failed."));
    ok &= require(fixture.database.transaction(), QStringLiteral("Allocation rejection transaction failed."));
    QString allocationError;
    ok &= require(!repository.removeEntryInCurrentTransaction(allocated, 1, {}, &allocationError)
                  && allocationError.contains(QStringLiteral("allocated"), Qt::CaseInsensitive)
                  && fixture.quantity(allocated) == 4
                  && fixture.movements(allocated, "EntryRemoved") == 0,
                  QStringLiteral("Allocated Remove was not rejected without side effects."));
    QSqlQuery namedProbe(fixture.database);
    ok &= require(namedProbe.exec("SELECT COUNT(*) FROM inventory_record") && namedProbe.next()
                  && defaultQuery.exec("UPDATE isolation_probe SET value=value+1")
                  && fixture.database.rollback(),
                  QStringLiteral("Caller could not roll back after rejected Remove."));

    auto testLost = [&](bool commit) {
        const int record = fixture.addRecord(fixture.addPart(), 7);
        ok &= require(fixture.database.transaction(), QStringLiteral("Lost outer transaction failed."));
        ok &= require(repository.markLostInCurrentTransaction(record, 2, QStringLiteral("lost seam")),
                      QStringLiteral("Lost seam failed."));
        ok &= require(fixture.quantity(record) == 5 && fixture.movements(record, "Lost") == 1,
                      QStringLiteral("Lost seam effects were not visible inside the transaction."));
        ok &= require(commit ? fixture.database.commit() : fixture.database.rollback(),
                      QStringLiteral("Lost caller transaction completion failed."));
        ok &= require(fixture.quantity(record) == (commit ? 5 : 7)
                      && fixture.movements(record, "Lost") == (commit ? 1 : 0),
                      QStringLiteral("Lost outer commit/rollback did not govern all effects."));
    };
    testLost(false);
    testLost(true);

    const int rejectedLost = fixture.addRecord(fixture.addPart(), 3);
    ok &= require(fixture.database.transaction(), QStringLiteral("Rejected Lost transaction failed."));
    ok &= require(!repository.markLostInCurrentTransaction(rejectedLost, 4, {})
                  && fixture.quantity(rejectedLost) == 3
                  && fixture.movements(rejectedLost, "Lost") == 0
                  && fixture.database.rollback(),
                  QStringLiteral("Rejected Lost changed state or ended caller transaction."));

    auto testCorrection = [&](bool commit) {
        const int sourcePart = fixture.addPart();
        const int replacementPart = fixture.addPart();
        const int source = fixture.addRecord(sourcePart, 6);
        const int manufacturerCount = scalar(fixture.database, "SELECT COUNT(*) FROM manufacturer").toInt();
        ok &= require(fixture.database.transaction(), QStringLiteral("Correct outer transaction failed."));
        ok &= require(repository.correctEntryInCurrentTransaction(source, replacementPart, 2,
                                                                  QStringLiteral("correct seam")),
                      QStringLiteral("Correct seam failed."));
        const int destination = scalar(fixture.database,
            QStringLiteral("SELECT id FROM inventory_record WHERE part_id=%1 AND id<>%2")
                .arg(replacementPart).arg(source)).toInt();
        ok &= require(fixture.quantity(source) == 4 && destination > 0
                      && fixture.quantity(destination) == 2
                      && fixture.movements(source, "CorrectionRemoved") == 1
                      && fixture.movements(destination, "CorrectionAdded") == 1,
                      QStringLiteral("Both Correct seam halves were not visible inside the transaction."));
        ok &= require(commit ? fixture.database.commit() : fixture.database.rollback(),
                      QStringLiteral("Correct caller transaction completion failed."));
        ok &= require(fixture.quantity(source) == (commit ? 4 : 6)
                      && scalar(fixture.database,
                          QStringLiteral("SELECT COUNT(*) FROM inventory_record WHERE part_id=%1")
                              .arg(replacementPart)).toInt() == (commit ? 1 : 0)
                      && fixture.movements(source, "CorrectionRemoved") == (commit ? 1 : 0)
                      && scalar(fixture.database,
                          QStringLiteral("SELECT COUNT(*) FROM inventory_movement WHERE "
                                         "movement_type='CorrectionAdded' AND part_id=%1")
                              .arg(replacementPart)).toInt() == (commit ? 1 : 0)
                      && scalar(fixture.database, "SELECT COUNT(*) FROM manufacturer").toInt()
                             == manufacturerCount,
                      QStringLiteral("Correct outer commit/rollback did not govern all effects/provenance."));
    };
    testCorrection(false);
    testCorrection(true);

    const int failedSource = fixture.addRecord(fixture.addPart(), 5);
    const int failedReplacement = fixture.addPart();
    QSqlQuery failureTrigger(fixture.database);
    ok &= require(failureTrigger.exec(
                      "CREATE TRIGGER fail_correction_added BEFORE INSERT ON inventory_movement "
                      "WHEN NEW.movement_type='CorrectionAdded' BEGIN SELECT RAISE(ABORT,'forced'); END"),
                  QStringLiteral("Correct late-failure trigger could not be created."));
    ok &= require(fixture.database.transaction(), QStringLiteral("Failed Correct transaction failed."));
    ok &= require(!repository.correctEntryInCurrentTransaction(failedSource, failedReplacement, 2, {})
                  && fixture.quantity(failedSource) == 3
                  && fixture.movements(failedSource, "CorrectionRemoved") == 1,
                  QStringLiteral("Correct failure did not reach the intended second-half failure point."));
    ok &= require(fixture.database.rollback()
                  && fixture.quantity(failedSource) == 5
                  && fixture.movements(failedSource, "CorrectionRemoved") == 0
                  && scalar(fixture.database,
                      QStringLiteral("SELECT COUNT(*) FROM inventory_record WHERE part_id=%1")
                          .arg(failedReplacement)).toInt() == 0,
                  QStringLiteral("Caller rollback did not remove the partial Correct first half."));
    ok &= require(failureTrigger.exec("DROP TRIGGER fail_correction_added"),
                  QStringLiteral("Correct late-failure trigger could not be removed."));

    auto seedLost = [&](int partId, int quantity) {
        const int record = fixture.addRecord(partId, quantity);
        return repository.markLost(record, quantity, QStringLiteral("seed lost"));
    };
    auto testFound = [&](bool merge, bool commit) {
        const int partId = fixture.addPart();
        ok &= require(seedLost(partId, 4), QStringLiteral("Found Lost-history seed failed."));
        int destination = 0;
        const int priorQuantity = merge ? 3 : 0;
        if (merge)
            destination = fixture.addRecord(partId, priorQuantity, fixture.secondStorageId);
        const int recordsBefore = scalar(fixture.database,
            QStringLiteral("SELECT COUNT(*) FROM inventory_record WHERE part_id=%1").arg(partId)).toInt();
        const int manufacturersBefore = scalar(fixture.database,
            "SELECT COUNT(*) FROM manufacturer").toInt();
        ok &= require(fixture.database.transaction(), QStringLiteral("Found outer transaction failed."));
        InventoryRecordRepository::FoundResult result;
        ok &= require(repository.markFoundInCurrentTransaction(
                          fixture.workspaceId, partId, fixture.colorId, 2,
                          fixture.secondStorageId, QStringLiteral("Used"),
                          QStringLiteral("Owned"), QStringLiteral("found seam"), &result),
                      QStringLiteral("Found seam failed."));
        if (!merge) destination = result.inventoryRecordId;
        ok &= require(result.inventoryRecordId == destination
                      && result.quantityRestored == 2
                      && result.destinationStorageLocationId == fixture.secondStorageId
                      && result.outstandingLostQuantity == 2
                      && result.created == !merge && result.merged == merge
                      && fixture.quantity(destination) == priorQuantity + 2
                      && fixture.movements(destination, "Found") == 1,
                      QStringLiteral("Found result/create-or-merge state is incorrect."));
        ok &= require(commit ? fixture.database.commit() : fixture.database.rollback(),
                      QStringLiteral("Found caller transaction completion failed."));
        ok &= require(scalar(fixture.database,
                          QStringLiteral("SELECT COUNT(*) FROM inventory_record WHERE part_id=%1")
                              .arg(partId)).toInt() == recordsBefore + (commit && !merge ? 1 : 0)
                      && (!merge || fixture.quantity(destination) == priorQuantity + (commit ? 2 : 0))
                      && scalar(fixture.database,
                          QStringLiteral("SELECT COUNT(*) FROM inventory_movement WHERE part_id=%1 "
                                         "AND movement_type='Found'").arg(partId)).toInt()
                             == (commit ? 1 : 0)
                      && scalar(fixture.database,
                          QStringLiteral("SELECT COUNT(*) FROM inventory_movement WHERE part_id=%1 "
                                         "AND movement_type='Lost'").arg(partId)).toInt() == 1
                      && scalar(fixture.database, "SELECT COUNT(*) FROM manufacturer").toInt()
                             == manufacturersBefore,
                      QStringLiteral("Found outer commit/rollback did not govern inventory/history/provenance."));
    };
    testFound(false, false);
    testFound(false, true);
    testFound(true, false);
    testFound(true, true);

    const int rejectedFoundPart = fixture.addPart();
    ok &= require(seedLost(rejectedFoundPart, 1), QStringLiteral("Rejected Found seed failed."));
    ok &= require(fixture.database.transaction(), QStringLiteral("Rejected Found transaction failed."));
    InventoryRecordRepository::FoundResult rejectedFoundResult;
    ok &= require(!repository.markFoundInCurrentTransaction(
                      fixture.workspaceId, rejectedFoundPart, fixture.colorId, 2,
                      fixture.secondStorageId, "Used", "Owned", {}, &rejectedFoundResult)
                  && fixture.database.rollback(),
                  QStringLiteral("Rejected Found changed state or ended caller transaction."));

    auto testMove = [&](int sourceQuantity, int destinationQuantity, int moved,
                        bool commit) {
        const int partId = fixture.addPart();
        const int source = fixture.addRecord(partId, sourceQuantity);
        int destination = destinationQuantity > 0
                              ? fixture.addRecord(partId, destinationQuantity,
                                                  fixture.secondStorageId)
                              : 0;
        const int recordsBefore = scalar(fixture.database,
            QStringLiteral("SELECT COUNT(*) FROM inventory_record WHERE part_id=%1").arg(partId)).toInt();
        ok &= require(fixture.database.transaction(), QStringLiteral("Move outer transaction failed."));
        InventoryRecordRepository::MoveResult result;
        ok &= require(repository.moveInventoryInCurrentTransaction(
                          source, fixture.secondStorageId, moved, &result),
                      QStringLiteral("Move seam failed."));
        if (destination == 0) destination = result.destinationRecordId;
        ok &= require(result.sourceRecordId == source
                      && result.destinationRecordId == destination
                      && result.movedQuantity == moved
                      && result.resultingSourceQuantity == sourceQuantity - moved
                      && result.resultingDestinationQuantity == destinationQuantity + moved
                      && result.destinationCreated == (destinationQuantity == 0)
                      && fixture.quantity(source) == sourceQuantity - moved
                      && fixture.quantity(destination) == destinationQuantity + moved
                      && fixture.movements(source, "Move") == 1,
                      QStringLiteral("Move result/source/destination state is incorrect."));
        ok &= require(commit ? fixture.database.commit() : fixture.database.rollback(),
                      QStringLiteral("Move caller transaction completion failed."));
        ok &= require(fixture.quantity(source) == sourceQuantity - (commit ? moved : 0)
                      && (destinationQuantity == 0
                              ? scalar(fixture.database,
                                  QStringLiteral("SELECT COUNT(*) FROM inventory_record WHERE part_id=%1")
                                      .arg(partId)).toInt()
                                    == recordsBefore + (commit ? 1 : 0)
                              : fixture.quantity(destination)
                                    == destinationQuantity + (commit ? moved : 0))
                      && fixture.movements(source, "Move") == (commit ? 1 : 0),
                      QStringLiteral("Move outer commit/rollback did not govern all effects."));
    };
    testMove(4, 0, 4, false);
    testMove(4, 0, 4, true);
    testMove(6, 3, 2, false);
    testMove(6, 3, 2, true);

    const int rejectedMove = fixture.addRecord(fixture.addPart(), 2);
    ok &= require(fixture.database.transaction(), QStringLiteral("Rejected Move transaction failed."));
    ok &= require(!repository.moveInventoryInCurrentTransaction(
                      rejectedMove, fixture.storageId, 1, nullptr)
                  && fixture.quantity(rejectedMove) == 2
                  && fixture.database.rollback(),
                  QStringLiteral("Rejected Move changed state or ended caller transaction."));

    auto testAdd = [&](bool merge, bool commit) {
        const int partId = fixture.addPart();
        int destination = merge ? fixture.addRecord(partId, 3) : 0;
        const int recordsBefore = scalar(fixture.database,
            QStringLiteral("SELECT COUNT(*) FROM inventory_record WHERE part_id=%1").arg(partId)).toInt();
        InventoryRecord added;
        added.setWorkspaceId(fixture.workspaceId);
        added.setPartId(partId);
        added.setColorId(fixture.colorId);
        added.setStorageLocationId(fixture.storageId);
        added.setManufacturerId(fixture.manufacturerId);
        added.setCondition(QStringLiteral("Used"));
        added.setOwnershipType(QStringLiteral("Owned"));
        added.setQuantity(2);
        ok &= require(fixture.database.transaction(), QStringLiteral("Add outer transaction failed."));
        InventoryRecordRepository::AddResult result;
        ok &= require(repository.addOrIncreaseQuantityInCurrentTransaction(
                          added, {}, {}, {}, QStringLiteral("add seam"), &result),
                      QStringLiteral("Add seam failed."));
        if (!merge) destination = result.inventoryRecordId;
        ok &= require(result.inventoryRecordId == destination
                      && result.resultingQuantity == (merge ? 5 : 2)
                      && result.created == !merge && result.merged == merge
                      && added.id() == destination && fixture.quantity(destination) == (merge ? 5 : 2),
                      QStringLiteral("Add result/create-or-merge state is incorrect."));
        ok &= require(commit ? fixture.database.commit() : fixture.database.rollback(),
                      QStringLiteral("Add caller transaction completion failed."));
        ok &= require(scalar(fixture.database,
                          QStringLiteral("SELECT COUNT(*) FROM inventory_record WHERE part_id=%1")
                              .arg(partId)).toInt() == recordsBefore + (commit && !merge ? 1 : 0)
                      && (!merge || fixture.quantity(destination) == 3 + (commit ? 2 : 0))
                      && scalar(fixture.database,
                          QStringLiteral("SELECT COUNT(*) FROM inventory_movement WHERE part_id=%1")
                              .arg(partId)).toInt() == (commit ? 1 : 0),
                      QStringLiteral("Add outer commit/rollback did not govern inventory/history."));
    };
    testAdd(false, false);
    testAdd(false, true);
    testAdd(true, false);
    testAdd(true, true);

    InventoryRecord invalidAdd;
    ok &= require(fixture.database.transaction(), QStringLiteral("Rejected Add transaction failed."));
    ok &= require(!repository.addOrIncreaseQuantityInCurrentTransaction(invalidAdd)
                  && fixture.database.rollback(),
                  QStringLiteral("Rejected Add ended caller transaction."));

    auto testUpdate = [&](bool merge, bool commit) {
        const int partId = fixture.addPart();
        const int source = fixture.addRecord(partId, 4);
        int destination = 0;
        if (merge)
            destination = fixture.addRecord(partId, 3, fixture.secondStorageId,
                                            QStringLiteral("New"), QStringLiteral("Borrowed"));
        InventoryRecord edited = *repository.getById(source);
        edited.setStorageLocationId(fixture.secondStorageId);
        edited.setCondition(QStringLiteral("New"));
        edited.setOwnershipType(QStringLiteral("Borrowed"));
        edited.setQuantity(merge ? 4 : 5);
        ok &= require(fixture.database.transaction(), QStringLiteral("Update outer transaction failed."));
        InventoryRecordRepository::UpdateResult result;
        ok &= require(repository.updateOrMergeInCurrentTransaction(edited, &result),
                      QStringLiteral("Update/Merge seam failed."));
        ok &= require(result.sourceRecordId == source
                      && result.survivingRecordId == (merge ? destination : source)
                      && result.resultingQuantity == (merge ? 7 : 5)
                      && result.storageLocationId == fixture.secondStorageId
                      && result.merged == merge && edited.id() == result.survivingRecordId
                      && fixture.quantity(source) == (merge ? 0 : 5)
                      && (!merge || fixture.quantity(destination) == 7)
                      && scalar(fixture.database,
                          QStringLiteral("SELECT COUNT(*) FROM inventory_movement WHERE "
                                         "inventory_record_id=%1").arg(result.survivingRecordId)).toInt()
                             >= (merge ? 1 : 4),
                      QStringLiteral("Update/Merge result and movement state is incorrect."));
        ok &= require(commit ? fixture.database.commit() : fixture.database.rollback(),
                      QStringLiteral("Update caller transaction completion failed."));
        ok &= require(fixture.quantity(source) == (commit ? (merge ? 0 : 5) : 4)
                      && (!merge || fixture.quantity(destination) == 3 + (commit ? 4 : 0))
                      && scalar(fixture.database,
                          QStringLiteral("SELECT COUNT(*) FROM inventory_movement WHERE "
                                         "part_id=%1 AND movement_type IN "
                                         "('QuantityIncrease','Move','ConditionChange','OwnershipChange','Merge')")
                              .arg(partId)).toInt() == (commit ? (merge ? 4 : 4) : 0),
                      QStringLiteral("Update/Merge outer commit/rollback did not govern all effects."));
    };
    testUpdate(false, false);
    testUpdate(false, true);
    testUpdate(true, false);
    testUpdate(true, true);

    const int failedUpdate = fixture.addRecord(fixture.addPart(), 3);
    InventoryRecord invalidUpdate = *repository.getById(failedUpdate);
    invalidUpdate.setQuantity(0);
    ok &= require(fixture.database.transaction(), QStringLiteral("Rejected Update transaction failed."));
    ok &= require(!repository.updateOrMergeInCurrentTransaction(invalidUpdate)
                  && fixture.quantity(failedUpdate) == 3
                  && fixture.database.rollback(),
                  QStringLiteral("Rejected Update changed state or ended caller transaction."));

    const int localRemove = fixture.addRecord(fixture.addPart(), 3);
    const int localLost = fixture.addRecord(fixture.addPart(), 3);
    const int localCorrect = fixture.addRecord(fixture.addPart(), 3);
    const int localReplacement = fixture.addPart();
    const int localMove = fixture.addRecord(fixture.addPart(), 3);
    const int localUpdate = fixture.addRecord(fixture.addPart(), 3);
    const int localFoundPart = fixture.addPart();
    const int localAddPart = fixture.addPart();
    InventoryRecord localAdd;
    localAdd.setWorkspaceId(fixture.workspaceId);
    localAdd.setPartId(localAddPart);
    localAdd.setColorId(fixture.colorId);
    localAdd.setStorageLocationId(fixture.storageId);
    localAdd.setManufacturerId(fixture.manufacturerId);
    localAdd.setCondition(QStringLiteral("Used"));
    localAdd.setOwnershipType(QStringLiteral("Owned"));
    localAdd.setQuantity(2);
    InventoryRecord localEdited = *repository.getById(localUpdate);
    localEdited.setQuantity(4);
    QString localError;
    ok &= require(repository.removeEntry(localRemove, 1, {}, &localError)
                  && fixture.quantity(localRemove) == 2
                  && repository.markLost(localLost, 1, {})
                  && fixture.quantity(localLost) == 2
                  && repository.correctEntry(localCorrect, localReplacement, 1, {})
                  && fixture.quantity(localCorrect) == 2
                  && repository.moveInventory(localMove, fixture.secondStorageId, 1)
                  && fixture.quantity(localMove) == 2
                  && repository.updateOrMerge(localEdited)
                  && fixture.quantity(localUpdate) == 4
                  && repository.addOrIncreaseQuantity(localAdd)
                  && localAdd.id() > 0 && fixture.quantity(localAdd.id()) == 2
                  && seedLost(localFoundPart, 2)
                  && repository.markFound(fixture.workspaceId, localFoundPart, fixture.colorId, 1,
                                          fixture.secondStorageId, "Used", "Owned", {})
                  && scalar(fixture.database,
                      QStringLiteral("SELECT SUM(quantity) FROM inventory_record WHERE part_id=%1")
                          .arg(localFoundPart)).toInt() == 1,
                  QStringLiteral("A transaction-owning local wrapper regressed: ") + localError);

    ok &= require(scalar(defaultDatabase, "SELECT value FROM isolation_probe").toInt() == 18
                  && !defaultDatabase.tables().contains(QStringLiteral("inventory_record")),
                  QStringLiteral("Named repository operations touched the default connection."));

    fixture.close();
    defaultDatabase.close();
    defaultDatabase = {};
    QSqlDatabase::removeDatabase(defaultName);

    if (!ok) return 1;
    qInfo("Inventory transaction seam validation passed.");
    return 0;
}
