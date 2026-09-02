#include "../src/database/DatabaseManager.h"
#include "../src/database/DatabaseSchema.h"
#include "../src/import/RebrickableMinifigCatalogImporter.h"
#include "../src/models/MinifigCatalogSearchCriteria.h"
#include "../src/repositories/MinifigCatalogRepository.h"

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
#include <utility>
#include <zlib.h>

namespace {
const QByteArray FullCatalogCsv(
    "fig_num,name,num_parts,img_url\n"
    "fig-000001,Toy Store Employee,4,https://example.test/fig-000001.jpg\n"
    "fig-000002,Customer Kid,4,https://example.test/fig-000002.jpg\n"
    "fig-000003,\"Assassin Droid, White\",8,https://example.test/fig-000003.jpg\n");

class TestEnvironmentGuard
{
public:
    explicit TestEnvironmentGuard(QString applicationDataPath)
        : m_applicationDataPath(std::move(applicationDataPath))
    {
    }

    ~TestEnvironmentGuard()
    {
        DatabaseManager::instance().close();
        QDir(m_applicationDataPath).removeRecursively();
    }

private:
    QString m_applicationDataPath;
};

bool require(bool condition, const QString& message)
{
    if (!condition)
        qCritical().noquote() << message;
    return condition;
}

bool writeBytes(const QString& path, const QByteArray& contents)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(contents) == contents.size();
}

void appendUInt16(QByteArray& bytes, quint16 value)
{
    bytes.append(static_cast<char>(value & 0xff));
    bytes.append(static_cast<char>((value >> 8) & 0xff));
}

void appendUInt32(QByteArray& bytes, quint32 value)
{
    appendUInt16(bytes, static_cast<quint16>(value & 0xffff));
    appendUInt16(bytes, static_cast<quint16>((value >> 16) & 0xffff));
}

bool writeStoredZip(const QString& path,
                    const QByteArray& entryName,
                    const QByteArray& contents)
{
    if (entryName.size() > 0xffff
        || contents.size() > static_cast<qsizetype>(std::numeric_limits<quint32>::max())) {
        return false;
    }

    const quint32 size = static_cast<quint32>(contents.size());
    const quint32 crc = static_cast<quint32>(
        crc32(0L,
              reinterpret_cast<const Bytef*>(contents.constData()),
              static_cast<uInt>(contents.size())));
    QByteArray archive;

    appendUInt32(archive, 0x04034b50);
    appendUInt16(archive, 20);
    appendUInt16(archive, 0);
    appendUInt16(archive, 0);
    appendUInt16(archive, 0);
    appendUInt16(archive, 0);
    appendUInt32(archive, crc);
    appendUInt32(archive, size);
    appendUInt32(archive, size);
    appendUInt16(archive, static_cast<quint16>(entryName.size()));
    appendUInt16(archive, 0);
    archive.append(entryName);
    archive.append(contents);

    const quint32 centralOffset = static_cast<quint32>(archive.size());
    appendUInt32(archive, 0x02014b50);
    appendUInt16(archive, 20);
    appendUInt16(archive, 20);
    appendUInt16(archive, 0);
    appendUInt16(archive, 0);
    appendUInt16(archive, 0);
    appendUInt16(archive, 0);
    appendUInt32(archive, crc);
    appendUInt32(archive, size);
    appendUInt32(archive, size);
    appendUInt16(archive, static_cast<quint16>(entryName.size()));
    appendUInt16(archive, 0);
    appendUInt16(archive, 0);
    appendUInt16(archive, 0);
    appendUInt16(archive, 0);
    appendUInt32(archive, 0);
    appendUInt32(archive, 0);
    archive.append(entryName);

    const quint32 centralSize = static_cast<quint32>(archive.size()) - centralOffset;
    appendUInt32(archive, 0x06054b50);
    appendUInt16(archive, 0);
    appendUInt16(archive, 0);
    appendUInt16(archive, 1);
    appendUInt16(archive, 1);
    appendUInt32(archive, centralSize);
    appendUInt32(archive, centralOffset);
    appendUInt16(archive, 0);
    return writeBytes(path, archive);
}

bool validateVersion23Migration(const QString& path)
{
    const QString connectionName = QStringLiteral("Migration_%1")
                                       .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    bool success = false;
    {
        QSqlDatabase database = QSqlDatabase::addDatabase("QSQLITE", connectionName);
        database.setDatabaseName(path);
        if (database.open()) {
            QSqlQuery query(database);
            success = query.exec("CREATE TABLE schema_version (version INTEGER NOT NULL)")
                      && query.exec("INSERT INTO schema_version VALUES (23)")
                      && DatabaseSchema::initialize(database)
                      && query.exec("SELECT version FROM schema_version")
                      && query.next() && query.value(0).toInt() == 24
                      && query.exec("SELECT COUNT(*) FROM minifig_catalog")
                      && query.next() && query.value(0).toInt() == 0;
            database.close();
        }
    }
    QSqlDatabase::removeDatabase(connectionName);
    return success;
}
} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication application(argc, argv);
    application.setOrganizationName(QStringLiteral("RFStateSideTests"));
    application.setApplicationName(QStringLiteral("BrickSuiteM2371Test"));
    QStandardPaths::setTestModeEnabled(true);

    QTemporaryDir temporaryDirectory;
    if (!require(temporaryDirectory.isValid(), "Unable to create test directory."))
        return 1;
    if (!require(validateVersion23Migration(temporaryDirectory.filePath("schema23.db")),
                 "Schema 23 to 24 migration validation failed."))
        return 1;

    const QString applicationData = QStandardPaths::writableLocation(
        QStandardPaths::AppLocalDataLocation);
    QDir(applicationData).removeRecursively();
    TestEnvironmentGuard cleanup(applicationData);

    DatabaseManager& manager = DatabaseManager::instance();
    if (!require(manager.initialize(), "Unable to initialize isolated test database."))
        return 1;

    const QString sampleZip = temporaryDirectory.filePath("synthetic-minifigs.zip");
    if (!require(writeStoredZip(sampleZip, "nested/minifigs.csv", FullCatalogCsv),
                 "Unable to create synthetic minifigs.csv ZIP."))
        return 1;

    RebrickableMinifigCatalogImporter importer;
    const auto zipResult = importer.importFile(sampleZip);
    if (!require(zipResult.success && zipResult.rowsRead == 3 && zipResult.inserted == 3,
                 QString("ZIP import failed: %1").arg(zipResult.message)))
        return 1;

    MinifigCatalogRepository repository;
    const auto first = repository.getByExternalIdentifier("rebrickable", "FIG-000001");
    if (!require(first.has_value() && first->name() == "Toy Store Employee",
                 "Case-insensitive provider identity lookup failed."))
        return 1;
    const int stableFirstId = first->id();

    MinifigCatalogSearchCriteria searchCriteria;
    searchCriteria.searchText = QStringLiteral("fig-000003");
    searchCriteria.provider = QStringLiteral("Rebrickable");
    const QList<MinifigCatalogSearchResult> searchResults = repository.search(searchCriteria);
    if (!require(searchResults.size() == 1
                     && searchResults.constFirst().rebrickableExternalId == "fig-000003",
                 "Paged search did not return the Rebrickable external ID."))
        return 1;

    const QString reducedCsv = temporaryDirectory.filePath("minifigs.csv");
    if (!require(writeBytes(reducedCsv,
                            "fig_num,name,num_parts,img_url\n"
                            "fig-000001,Updated Employee,5,https://example.test/updated.jpg\n"
                            "fig-test-new,Synthetic Figure,1,https://example.test/new.jpg\n"),
                 "Unable to write reduced snapshot."))
        return 1;

    QSqlQuery failureTrigger(manager.database());
    if (!require(failureTrigger.exec(R"(
            CREATE TRIGGER reject_synthetic_minifig
            BEFORE INSERT ON minifig_catalog
            WHEN NEW.name = 'Synthetic Figure'
            BEGIN SELECT RAISE(ABORT, 'synthetic import failure'); END
        )"),
                 "Unable to install transactional rollback test trigger."))
        return 1;
    const auto forcedFailure = importer.importFile(reducedCsv);
    const auto unchangedFirst = repository.getByExternalIdentifier("Rebrickable", "fig-000001");
    if (!require(!forcedFailure.success && repository.count() == 3
                     && unchangedFirst.has_value() && unchangedFirst->id() == stableFirstId
                     && unchangedFirst->name() == "Toy Store Employee",
                 "A mid-import database failure was not fully rolled back."))
        return 1;
    if (!require(failureTrigger.exec("DROP TRIGGER reject_synthetic_minifig"),
                 "Unable to remove transactional rollback test trigger."))
        return 1;

    const auto reducedResult = importer.importFile(reducedCsv);
    const auto updatedFirst = repository.getByExternalIdentifier("Rebrickable", "fig-000001");
    if (!require(reducedResult.success && reducedResult.inserted == 1
                     && reducedResult.updated == 1 && reducedResult.deactivated == 2
                     && repository.count() == 2 && updatedFirst.has_value()
                     && updatedFirst->id() == stableFirstId
                     && updatedFirst->name() == "Updated Employee",
                 QString("Update/deactivation import failed: %1").arg(reducedResult.message)))
        return 1;

    const QString duplicateCsv = temporaryDirectory.filePath("duplicate-minifigs.csv");
    if (!require(writeBytes(duplicateCsv,
                            "fig_num,name,num_parts,img_url\n"
                            "fig-duplicate,First,1,\n"
                            "FIG-DUPLICATE,Second,1,\n"),
                 "Unable to write duplicate snapshot."))
        return 1;
    const int activeBeforeRejectedImports = repository.count();
    if (!require(!importer.importFile(duplicateCsv).success
                     && repository.count() == activeBeforeRejectedImports,
                 "Duplicate snapshot changed catalog data."))
        return 1;

    const QString malformedCsv = temporaryDirectory.filePath("malformed-minifigs.csv");
    if (!require(writeBytes(malformedCsv,
                            "fig_num,name,num_parts,img_url\n"
                            "fig-bad,Bad Figure,not-a-number,\n"),
                 "Unable to write malformed snapshot."))
        return 1;
    if (!require(!importer.importFile(malformedCsv).success
                     && repository.count() == activeBeforeRejectedImports,
                 "Malformed snapshot changed catalog data."))
        return 1;

    const QString fullCsv = temporaryDirectory.filePath("restored-minifigs.csv");
    if (!require(writeBytes(fullCsv, FullCatalogCsv), "Unable to write restoration snapshot."))
        return 1;
    const auto restoreResult = importer.importFile(fullCsv);
    const auto restoredFirst = repository.getByExternalIdentifier("Rebrickable", "fig-000001");
    const auto restoredSecond = repository.getByExternalIdentifier("Rebrickable", "fig-000002");
    if (!require(restoreResult.success && repository.count() == 3
                     && restoredFirst.has_value() && restoredFirst->id() == stableFirstId
                     && restoredFirst->name() == "Toy Store Employee"
                     && restoredSecond.has_value(),
                 QString("Reactivation/stable-ID import failed: %1").arg(restoreResult.message)))
        return 1;

    qInfo() << "Minifig Catalog foundation validation passed.";
    return 0;
}
