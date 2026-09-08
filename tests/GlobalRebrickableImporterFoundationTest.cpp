#include "../src/database/DatabaseSchema.h"
#include "../src/import/global/RebrickableDatasetRegistry.h"
#include "../src/import/global/RebrickableImportCancellation.h"
#include "../src/import/global/RebrickableImportDiscoveryService.h"
#include "../src/import/global/RebrickableImportPlanController.h"
#include "../src/import/global/RebrickableWorkerDatabaseSession.h"

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
                     == RebrickableImportStatus::BlockedByDependency,
                 "Missing Part Categories did not conservatively block Parts.")) return 1;

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

    qInfo() << "Global Rebrickable importer foundation tests passed.";
    return 0;
}
