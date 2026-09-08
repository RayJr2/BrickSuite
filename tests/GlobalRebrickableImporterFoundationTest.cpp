#include "../src/database/DatabaseSchema.h"
#include "../src/import/global/RebrickableDatasetRegistry.h"
#include "../src/import/global/RebrickableImportCancellation.h"
#include "../src/import/global/RebrickableImportDiscoveryService.h"
#include "../src/import/global/RebrickableImportPlanController.h"
#include "../src/import/global/RebrickableWorkerDatabaseSession.h"
#include "../src/import/global/RebrickableGlobalImportService.h"
#include "../src/import/RebrickablePartCatalogImporter.h"

#include <QCoreApplication>
#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QThread>

#include <zlib.h>

#include <atomic>

namespace {
bool require(bool condition, const QString& message)
{
    if (!condition)
        qCritical().noquote() << message;
    return condition;
}

bool writeFile(const QString& path, const QByteArray& data)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(data) == data.size();
}

void put16(QByteArray& bytes, quint16 value)
{
    bytes.append(char(value & 0xff));
    bytes.append(char((value >> 8) & 0xff));
}

void put32(QByteArray& bytes, quint32 value)
{
    put16(bytes, quint16(value & 0xffff));
    put16(bytes, quint16(value >> 16));
}

bool writeStoredZip(const QString& path, const QByteArray& entryName, const QByteArray& data)
{
    const quint32 checksum = quint32(crc32(0, reinterpret_cast<const Bytef*>(data.constData()),
                                           uInt(data.size())));
    QByteArray bytes;
    put32(bytes, 0x04034b50); put16(bytes, 20); put16(bytes, 0); put16(bytes, 0);
    put16(bytes, 0); put16(bytes, 0); put32(bytes, checksum);
    put32(bytes, quint32(data.size())); put32(bytes, quint32(data.size()));
    put16(bytes, quint16(entryName.size())); put16(bytes, 0);
    bytes += entryName; bytes += data;
    const quint32 centralOffset = quint32(bytes.size());
    put32(bytes, 0x02014b50); put16(bytes, 20); put16(bytes, 20); put16(bytes, 0);
    put16(bytes, 0); put16(bytes, 0); put16(bytes, 0); put32(bytes, checksum);
    put32(bytes, quint32(data.size())); put32(bytes, quint32(data.size()));
    put16(bytes, quint16(entryName.size())); put16(bytes, 0); put16(bytes, 0);
    put16(bytes, 0); put16(bytes, 0); put32(bytes, 0); put32(bytes, 0); bytes += entryName;
    const quint32 centralSize = quint32(bytes.size()) - centralOffset;
    put32(bytes, 0x06054b50); put16(bytes, 0); put16(bytes, 0); put16(bytes, 1);
    put16(bytes, 1); put32(bytes, centralSize); put32(bytes, centralOffset); put16(bytes, 0);
    return writeFile(path, bytes);
}

QByteArray csvFor(const RebrickableDatasetDescriptor& descriptor)
{
    return descriptor.requiredHeaders.join(',').toUtf8() + "\n" +
           QByteArray(descriptor.requiredHeaders.size() == 1 ? "value\n" : "1") +
           QByteArray(descriptor.requiredHeaders.size() > 1
                          ? QByteArray(",x").repeated(descriptor.requiredHeaders.size() - 1)
                          : QByteArray()) + "\n";
}

const RebrickableImportPlanEntry& entryFor(const RebrickableImportPlan& plan,
                                           RebrickableDatasetId id)
{
    for (const auto& entry : plan.entries)
        if (entry.dataset == id)
            return entry;
    qFatal("Dataset entry not found");
}
}

int main(int argc, char** argv)
{
    QCoreApplication application(argc, argv);
    if (!require(RebrickableDatasetRegistry::datasets().size() == 12,
                 "Registry must contain exactly twelve datasets.")) return 1;
    for (int index = 0; index < 12; ++index)
        if (!require(RebrickableDatasetRegistry::datasets().at(index).importOrder == index + 1,
                     "Registry order is not deterministic.")) return 1;

    QTemporaryDir allSources;
    if (!require(allSources.isValid(), "Unable to create source fixture directory.")) return 1;
    int index = 0;
    for (const auto& descriptor : RebrickableDatasetRegistry::datasets()) {
        const QByteArray csv = csvFor(descriptor);
        QString name = descriptor.csvFileName;
        if (index % 3 == 0)
            name = name.toUpper();
        if (index % 2 == 0) {
            if (!require(writeFile(allSources.filePath(name), csv), "Unable to write CSV fixture.")) return 1;
        } else {
            if (!require(writeStoredZip(allSources.filePath(name + ".zip"),
                                        ("nested/" + descriptor.csvFileName).toUtf8(), csv),
                         "Unable to write ZIP fixture.")) return 1;
        }
        ++index;
    }
    writeFile(allSources.filePath("unrelated.csv"), "anything\nvalue\n");
    writeFile(allSources.filePath("unrelated.zip"), "not a zip");
    const auto allPlan = RebrickableImportDiscoveryService().buildPlan(allSources.path());
    if (!require(allPlan.entries.size() == 12, "Discovery did not return all registry rows.")) return 1;
    for (const auto& entry : allPlan.entries) {
        if (!require(!entry.sourcePath.isEmpty(), "A supported source was not recognized.")) return 1;
        if (!require(entry.status == RebrickableImportStatus::Ready
                         || entry.status == RebrickableImportStatus::NotImplemented,
                     "A valid complete folder produced an unexpected status.")) return 1;
    }

    QTemporaryDir edgeCases;
    const auto* parts = RebrickableDatasetRegistry::descriptor(RebrickableDatasetId::Parts);
    if (!require(parts, "Parts descriptor missing.")) return 1;
    writeFile(edgeCases.filePath("parts.csv"), csvFor(*parts));
    writeStoredZip(edgeCases.filePath("PARTS.CSV.ZIP"), "parts.csv", csvFor(*parts));
    writeFile(edgeCases.filePath("sets.csv"), "wrong,header\n1,2\n");
    writeFile(edgeCases.filePath("minifigs.csv.zip"), "broken");
    const auto edgePlan = RebrickableImportDiscoveryService().buildPlan(edgeCases.path());
    if (!require(entryFor(edgePlan, RebrickableDatasetId::Parts).status
                     == RebrickableImportStatus::Ambiguous,
                 "CSV/ZIP duplicate was not ambiguous.")) return 1;
    if (!require(entryFor(edgePlan, RebrickableDatasetId::Sets).status
                     == RebrickableImportStatus::Invalid,
                 "Missing headers were not invalid.")) return 1;
    if (!require(entryFor(edgePlan, RebrickableDatasetId::Minifigs).status
                     == RebrickableImportStatus::Invalid,
                 "Invalid ZIP was not rejected.")) return 1;
    if (!require(entryFor(edgePlan, RebrickableDatasetId::Themes).status
                     == RebrickableImportStatus::Missing,
                 "Missing dataset was treated as a failure.")) return 1;

    QTemporaryDir dependencyFixture;
    writeFile(dependencyFixture.filePath("parts.csv"), csvFor(*parts));
    const auto dependencyPlan = RebrickableImportDiscoveryService().buildPlan(dependencyFixture.path());
    if (!require(entryFor(dependencyPlan, RebrickableDatasetId::Parts).status
                     == RebrickableImportStatus::Ready,
                 "Parts with a missing dependency file must reach source-aware execution validation.")) return 1;

    QTemporaryDir partialInventoryParts;
    writeFile(partialInventoryParts.filePath("inventory_parts.csv"),
              "inventory_id,part_num,color_id,quantity,is_spare,img_url\n100,p1,1,1,f,\n");
    const auto partialInventoryPlan = RebrickableImportDiscoveryService().buildPlan(partialInventoryParts.path());
    const auto& partialInventoryEntry = entryFor(partialInventoryPlan, RebrickableDatasetId::InventoryParts);
    if (!require(partialInventoryEntry.status == RebrickableImportStatus::BlockedByDependency
                     && partialInventoryEntry.message.contains("Inventories source is required"),
                 "Inventory Parts without ownership classification was not conservatively blocked.")) return 1;

    RebrickableImportPlan executionPlan = allPlan;
    if (!require(RebrickableImportPlanController::beginDataset(
                     executionPlan, RebrickableDatasetId::PartCategories),
                 "A ready dataset could not enter Importing state.")) return 1;
    RebrickableImportCounters completedCounters;
    completedCounters.rowsRead = 2;
    RebrickableImportPlanController::completeDataset(
        executionPlan, RebrickableDatasetId::PartCategories, completedCounters, 12, false);
    if (!require(entryFor(executionPlan, RebrickableDatasetId::PartCategories).status
                     == RebrickableImportStatus::Imported,
                 "Successful dataset transition was not retained.")) return 1;
    RebrickableImportPlanController::failDataset(
        executionPlan, RebrickableDatasetId::Parts, "Synthetic rollback", false);
    if (!require(entryFor(executionPlan, RebrickableDatasetId::PartRelationships).status
                     == RebrickableImportStatus::BlockedByDependency,
                 "A failed dataset did not block its dependent.")) return 1;
    if (!require(entryFor(executionPlan, RebrickableDatasetId::Sets).status
                     == RebrickableImportStatus::Ready,
                 "A failed dataset incorrectly blocked an independent dataset.")) return 1;

    RebrickableImportCancellation cancellation;
    RebrickableImportCancellation cancellationCopy = cancellation;
    std::atomic_bool observed{false};
    QThread cancellationThread;
    QObject::connect(&cancellationThread, &QThread::started, [&]() {
        cancellation.requestCancellation();
        observed.store(cancellationCopy.isCancellationRequested());
        cancellationThread.quit();
    });
    cancellationThread.start(); cancellationThread.wait();
    if (!require(observed.load(), "Cancellation was not safely shared between threads.")) return 1;

    RebrickableImportProgress progress;
    progress.dataset = RebrickableDatasetId::InventoryParts;
    progress.datasetIndex = 10; progress.datasetTotal = 12;
    progress.currentRow = 25; progress.totalRows = 100;
    progress.phase = "Validating"; progress.counters.rowsRead = 25;
    if (!require(progress.currentRow == progress.counters.rowsRead && progress.totalRows == 100,
                 "Progress contract did not preserve row/counter state.")) return 1;

    QTemporaryDir databaseDirectory;
    const QString databasePath = databaseDirectory.filePath("foundation.db");
    std::atomic_bool workerThreadCorrect{false};
    bool workerSuccess = false;
    QString workerError;
    QThread* worker = QThread::create([&]() {
        workerThreadCorrect.store(QThread::currentThread() != application.thread());
        workerSuccess = RebrickableWorkerDatabaseSession::execute(
            databasePath,
            [](QSqlDatabase& database, QString& error) {
                if (!DatabaseSchema::initialize(database)) { error = "Schema initialization failed."; return false; }
                QSqlQuery read(database);
                if (!read.exec("SELECT version FROM schema_version") || !read.next()) {
                    error = read.lastError().text(); return false;
                }
                if (!database.transaction()) { error = database.lastError().text(); return false; }
                QSqlQuery write(database);
                if (!write.exec("CREATE TABLE m25_worker_probe(value INTEGER)")) {
                    error = write.lastError().text(); database.rollback(); return false;
                }
                if (!database.rollback()) { error = database.lastError().text(); return false; }
                return true;
            }, workerError);
    });
    worker->start(); worker->wait(); delete worker;
    if (!require(workerThreadCorrect.load() && workerSuccess,
                 "Worker-owned database probe failed: " + workerError)) return 1;
    {
        const QString verificationName = "m25-verification";
        QSqlDatabase database = QSqlDatabase::addDatabase("QSQLITE", verificationName);
        database.setDatabaseName(databasePath);
        if (!require(database.open(), "Unable to verify worker database.")) return 1;
        if (!require(!database.tables().contains("m25_worker_probe"),
                     "Synthetic worker write was not rolled back.")) return 1;
        database.close();
    }
    QSqlDatabase::removeDatabase("m25-verification");
    for (const QString& connection : QSqlDatabase::connectionNames())
        if (!require(!connection.startsWith("rebrickable-import-"),
                     "Worker database connection leaked.")) return 1;

    QTemporaryDir executionSources;
    writeFile(executionSources.filePath("themes.csv"), "id,name,parent_id\n1,Root,\n2,Child,1\n");
    writeFile(executionSources.filePath("colors.csv"), "id,name,rgb,is_trans\n1,Black,000000,false\n");
    writeFile(executionSources.filePath("part_categories.csv"), "id,name\n1,Bricks\n");
    writeFile(executionSources.filePath("parts.csv"), "part_num,name,part_cat_id,part_material\np1,Part One,1,Plastic\np2,Part Two,1,Plastic\n");
    writeFile(executionSources.filePath("part_relationships.csv"),
              "rel_type,child_part_num,parent_part_num\nA,p2,p1\nR,p1,p1\n");
    writeFile(executionSources.filePath("sets.csv"), "set_num,name,year,theme_id,num_parts,img_url\n1-1,Set One,2026,2,2,https://example.invalid/set.png\n");
    writeFile(executionSources.filePath("minifigs.csv"), "fig_num,name,num_parts,img_url\nfig-1,Figure One,2,https://example.invalid/fig.png\n");
    writeFile(executionSources.filePath("inventories.csv"), "id,version,set_num\n100,1,1-1\n200,1,fig-1\n");
    writeFile(executionSources.filePath("inventory_parts.csv"), "inventory_id,part_num,color_id,quantity,is_spare,img_url\n100,p1,1,2,f,\n200,not-a-set-part,999,1,f,\n");
    writeFile(executionSources.filePath("inventory_minifigs.csv"), "inventory_id,fig_num,quantity\n100,fig-1,1\n");
    writeFile(executionSources.filePath("inventory_sets.csv"), "inventory_id,set_num,quantity\n100,1-1,1\n");
    QByteArray cancelledParts("part_num,name,part_cat_id,part_material\n");
    for (int row = 0; row < 300; ++row)
        cancelledParts += QStringLiteral("cancel-%1,Cancelled Part,1,Plastic\n").arg(row).toUtf8();
    writeFile(executionSources.filePath("cancelled-parts.csv"), cancelledParts);
    RebrickableImportPlan importPlan = RebrickableImportDiscoveryService().buildPlan(executionSources.path());
    int progressEvents = 0; bool executionSuccess = false; QString executionError;
    QThread* importThread = QThread::create([&]() {
        executionSuccess = RebrickableWorkerDatabaseSession::execute(databasePath,
            [&](QSqlDatabase& database, QString&) {
                RebrickableImportCancellation token;
                importPlan = RebrickableGlobalImportService().run(importPlan, database, token,
                    [&](const RebrickableImportProgress&) { ++progressEvents; });
                RebrickableImportCancellation cancelled;
                cancelled.requestCancellation();
                const auto cancelledResult = RebrickablePartCatalogImporter().importFile(
                    executionSources.filePath("cancelled-parts.csv"), database, &cancelled);
                if (cancelledResult.success) return false;
                QSqlQuery counts(database);
                return counts.exec("SELECT (SELECT COUNT(*) FROM theme_external_identifier WHERE provider='Rebrickable'), (SELECT COUNT(*) FROM part), (SELECT COUNT(*) FROM part_relationship WHERE source='Rebrickable'), (SELECT COUNT(*) FROM set_catalog), (SELECT COUNT(*) FROM minifig_catalog), (SELECT COUNT(*) FROM set_inventory_revision WHERE is_preferred=1), (SELECT COUNT(*) FROM set_inventory_part), (SELECT COUNT(*) FROM set_inventory_minifig), (SELECT COUNT(*) FROM set_inventory_contained_set)")
                    && counts.next() && counts.value(0).toInt()==2 && counts.value(1).toInt()==2
                    && counts.value(2).toInt()==1 && counts.value(3).toInt()==1 && counts.value(4).toInt()==1
                    && counts.value(5).toInt()==1 && counts.value(6).toInt()==1
                    && counts.value(7).toInt()==1 && counts.value(8).toInt()==1;
            }, executionError);
    });
    importThread->start(); importThread->wait(); delete importThread;
    if (!require(executionSuccess, "Synthetic seven-dataset import failed: " + executionError)) return 1;
    for (int dataset = int(RebrickableDatasetId::Themes); dataset <= int(RebrickableDatasetId::InventorySets); ++dataset) {
        if (RebrickableDatasetId(dataset) == RebrickableDatasetId::Elements)
            continue;
        const auto status = entryFor(importPlan, RebrickableDatasetId(dataset)).status;
        if (!require(status == RebrickableImportStatus::Imported || status == RebrickableImportStatus::NoChanges,
                     "An implemented dataset did not complete.")) return 1;
    }
    if (!require(progressEvents >= 14, "Global import did not publish validation/import progress.")) return 1;
    const auto& relationshipResult = entryFor(importPlan, RebrickableDatasetId::PartRelationships);
    if (!require(relationshipResult.counters.rowsRead == 2
                     && relationshipResult.counters.selfReferencesIgnored == 1
                     && relationshipResult.counters.unresolved == 0,
                 "Resolved self-reference was not reported as a distinct provider no-op.")) return 1;
    if (!require(relationshipResult.message.contains("1 self-reference ignored"),
                 "Self-reference summary was not user-visible.")) return 1;
    const auto& inventoriesResult = entryFor(importPlan, RebrickableDatasetId::Inventories);
    if (!require(inventoriesResult.counters.rowsRead == 2
                     && inventoriesResult.counters.setInventoryRows == 1
                     && inventoriesResult.counters.recognizedMinifigInventories == 1
                     && inventoriesResult.message.contains("1 Minifig inventories recognized"),
                 "Mixed Inventory ownership was not reported accurately.")) return 1;
    const auto& inventoryPartsResult = entryFor(importPlan, RebrickableDatasetId::InventoryParts);
    if (!require(inventoryPartsResult.counters.rowsRead == 2
                     && inventoryPartsResult.counters.setInventoryRows == 1
                     && inventoryPartsResult.counters.ignoredMinifigPartRows == 1
                     && inventoryPartsResult.counters.unresolved == 0
                     && inventoryPartsResult.message.contains("1 Minifig-owned Part rows ignored"),
                 "Minifig-owned Inventory Part rows were not reported as intentional no-ops.")) return 1;

    QTemporaryDir malformedRelationships;
    QTemporaryDir unresolvedParent;
    QTemporaryDir unresolvedChild;
    QTemporaryDir replacementRelationships;
    writeFile(malformedRelationships.filePath("part_relationships.csv"),
              "rel_type,child_part_num,parent_part_num\nA,p2\n");
    writeFile(unresolvedParent.filePath("part_relationships.csv"),
              "rel_type,child_part_num,parent_part_num\nA,p2,missing-parent\n");
    writeFile(unresolvedChild.filePath("part_relationships.csv"),
              "rel_type,child_part_num,parent_part_num\nA,missing-child,p1\n");
    writeFile(replacementRelationships.filePath("part_relationships.csv"),
              "rel_type,child_part_num,parent_part_num\nM,p1,p2\n");

    bool relationshipSafetyPassed = false;
    QString relationshipSafetyError;
    QThread* relationshipThread = QThread::create([&]() {
        relationshipSafetyPassed = RebrickableWorkerDatabaseSession::execute(
            databasePath, [&](QSqlDatabase& database, QString& error) {
                QSqlQuery schema(database);
                if (!schema.exec("SELECT sql FROM sqlite_master WHERE type='table' AND name='part_relationship'")
                    || !schema.next()
                    || !schema.value(0).toString().contains("CHECK(parent_part_id <> child_part_id)")) {
                    error = "Part Relationship self-reference CHECK changed.";
                    return false;
                }

                QSqlQuery seed(database);
                if (!seed.exec("INSERT INTO part_relationship(parent_part_id,child_part_id,relationship_type,source_relationship_type,source,is_active,created_utc,modified_utc) SELECT p1.id,p2.id,'Alternate','A','OtherProvider',1,'2026-01-01','2026-01-01' FROM part p1,part p2 WHERE p1.part_number='p1' AND p2.part_number='p2'")) {
                    error = seed.lastError().text();
                    return false;
                }

                auto runRelationships = [&](const QString& directory) {
                    RebrickableImportPlan plan = RebrickableImportDiscoveryService().buildPlan(directory);
                    for (auto& entry : plan.entries) {
                        if (entry.dataset == RebrickableDatasetId::PartRelationships
                            && !entry.sourcePath.isEmpty()) {
                            entry.status = RebrickableImportStatus::Ready;
                            entry.message.clear();
                        }
                    }
                    RebrickableImportCancellation token;
                    return RebrickableGlobalImportService().run(plan, database, token);
                };
                auto countActive = [&](const QString& source) {
                    QSqlQuery count(database);
                    count.prepare("SELECT COUNT(*) FROM part_relationship WHERE source=:source AND is_active=1");
                    count.bindValue(":source", source);
                    return count.exec() && count.next() ? count.value(0).toInt() : -1;
                };

                const auto malformed = runRelationships(malformedRelationships.path());
                if (entryFor(malformed, RebrickableDatasetId::PartRelationships).status
                        != RebrickableImportStatus::Failed
                    || countActive("Rebrickable") != 1 || countActive("OtherProvider") != 1) {
                    error = "Malformed snapshot failed to preserve relationships.";
                    return false;
                }
                const auto missingParent = runRelationships(unresolvedParent.path());
                if (entryFor(missingParent, RebrickableDatasetId::PartRelationships).status
                        != RebrickableImportStatus::Failed
                    || countActive("Rebrickable") != 1) {
                    error = "Unresolved parent did not fail without deactivation.";
                    return false;
                }
                const auto missingChild = runRelationships(unresolvedChild.path());
                if (entryFor(missingChild, RebrickableDatasetId::PartRelationships).status
                        != RebrickableImportStatus::Failed
                    || countActive("Rebrickable") != 1) {
                    error = "Unresolved child did not fail without deactivation.";
                    return false;
                }
                const auto replacement = runRelationships(replacementRelationships.path());
                const auto& replacementResult = entryFor(
                    replacement, RebrickableDatasetId::PartRelationships);
                if (replacementResult.status != RebrickableImportStatus::Imported
                    || replacementResult.counters.inserted != 1
                    || replacementResult.counters.deactivated != 1
                    || countActive("Rebrickable") != 1
                    || countActive("OtherProvider") != 1) {
                    error = "Complete snapshot synchronization or provider isolation failed.";
                    return false;
                }
                return true;
            }, relationshipSafetyError);
    });
    relationshipThread->start(); relationshipThread->wait(); delete relationshipThread;
    if (!require(relationshipSafetyPassed,
                 "Relationship snapshot-safety validation failed: " + relationshipSafetyError)) return 1;

    qInfo() << "Global Rebrickable importer foundation tests passed.";
    return 0;
}
