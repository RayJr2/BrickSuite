#include "../src/database/DatabaseManager.h"
#include "../src/database/DatabaseSchema.h"
#include "../src/import/RebrickableMinifigPartsImporter.h"
#include "../src/models/MinifigCatalogPart.h"
#include "../src/repositories/MinifigCatalogPartRepository.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QUuid>

#include <limits>
#include <utility>
#include <zlib.h>

namespace {
class TestEnvironmentGuard
{
public:
    explicit TestEnvironmentGuard(QString path) : m_path(std::move(path)) {}
    ~TestEnvironmentGuard()
    {
        DatabaseManager::instance().close();
        QDir(m_path).removeRecursively();
    }
private:
    QString m_path;
};

bool require(bool condition, const QString& message)
{
    if (!condition)
        qCritical().noquote() << message;
    return condition;
}

bool writeBytes(const QString& path, const QByteArray& bytes)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}

void u16(QByteArray& bytes, quint16 value)
{
    bytes.append(static_cast<char>(value));
    bytes.append(static_cast<char>(value >> 8));
}

void u32(QByteArray& bytes, quint32 value)
{
    u16(bytes, static_cast<quint16>(value));
    u16(bytes, static_cast<quint16>(value >> 16));
}

bool writeZip(const QString& path, const QList<QPair<QByteArray, QByteArray>>& entries)
{
    struct Central { QByteArray name; quint32 crc; quint32 size; quint32 offset; };
    QByteArray archive;
    QList<Central> central;
    for (const auto& entry : entries) {
        if (entry.first.size() > 0xffff
            || entry.second.size() > static_cast<qsizetype>(std::numeric_limits<quint32>::max()))
            return false;
        Central item{entry.first,
                     static_cast<quint32>(crc32(0L,
                         reinterpret_cast<const Bytef*>(entry.second.constData()),
                         static_cast<uInt>(entry.second.size()))),
                     static_cast<quint32>(entry.second.size()),
                     static_cast<quint32>(archive.size())};
        u32(archive, 0x04034b50); u16(archive, 20); u16(archive, 0); u16(archive, 0);
        u16(archive, 0); u16(archive, 0); u32(archive, item.crc); u32(archive, item.size);
        u32(archive, item.size); u16(archive, static_cast<quint16>(item.name.size()));
        u16(archive, 0); archive += item.name; archive += entry.second; central += item;
    }
    const quint32 offset = static_cast<quint32>(archive.size());
    for (const Central& item : central) {
        u32(archive, 0x02014b50); u16(archive, 20); u16(archive, 20); u16(archive, 0);
        u16(archive, 0); u16(archive, 0); u16(archive, 0); u32(archive, item.crc);
        u32(archive, item.size); u32(archive, item.size);
        u16(archive, static_cast<quint16>(item.name.size())); u16(archive, 0); u16(archive, 0);
        u16(archive, 0); u16(archive, 0); u32(archive, 0); u32(archive, item.offset);
        archive += item.name;
    }
    const quint32 size = static_cast<quint32>(archive.size()) - offset;
    u32(archive, 0x06054b50); u16(archive, 0); u16(archive, 0);
    u16(archive, static_cast<quint16>(central.size()));
    u16(archive, static_cast<quint16>(central.size()));
    u32(archive, size); u32(archive, offset); u16(archive, 0);
    return writeBytes(path, archive);
}

bool validateMigration(const QString& path)
{
    const QString name = "Migrate_" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    bool ok = false;
    {
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", name);
        db.setDatabaseName(path);
        if (db.open()) {
            QSqlQuery query(db);
            ok = query.exec("CREATE TABLE schema_version(version INTEGER NOT NULL)")
                 && query.exec("INSERT INTO schema_version VALUES(25)")
                 && query.exec("CREATE TABLE build(id INTEGER PRIMARY KEY AUTOINCREMENT, workspace_id INTEGER NOT NULL, build_type TEXT NOT NULL, name TEXT NOT NULL, set_number TEXT, inventory_mode TEXT NOT NULL DEFAULT 'Stock', status TEXT NOT NULL DEFAULT 'Planned', is_active INTEGER NOT NULL DEFAULT 1, notes TEXT, created_utc TEXT NOT NULL, modified_utc TEXT NOT NULL, manufacturer_id INTEGER)")
                 && DatabaseSchema::initialize(db)
                 && query.exec("SELECT version FROM schema_version") && query.next()
                 && query.value(0).toInt() == 27
                 && query.exec("SELECT COUNT(*) FROM pragma_table_info('minifig_catalog_part')")
                 && query.next() && query.value(0).toInt() == 10;
            db.close();
        }
    }
    QSqlDatabase::removeDatabase(name);
    return ok;
}

QString snapshot(QSqlDatabase db, int minifigId)
{
    QStringList rows;
    QSqlQuery query(db);
    query.prepare("SELECT part_id,color_id,quantity_required,is_spare,provider,source "
                  "FROM minifig_catalog_part WHERE minifig_catalog_id=:id "
                  "ORDER BY is_spare,part_id,color_id");
    query.bindValue(":id", minifigId);
    if (!query.exec())
        return QString();
    while (query.next()) {
        rows << QString("%1|%2|%3|%4|%5|%6")
                    .arg(query.value(0).toInt()).arg(query.value(1).toInt())
                    .arg(query.value(2).toInt()).arg(query.value(3).toInt())
                    .arg(query.value(4).toString(), query.value(5).toString());
    }
    return rows.join(';');
}
} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    app.setOrganizationName("RFStateSideTests");
    app.setApplicationName("BrickSuiteM2373Test");
    QStandardPaths::setTestModeEnabled(true);
    QTemporaryDir files;
    if (!require(files.isValid(), "Unable to create test directory.")) return 1;
    if (!require(validateMigration(files.filePath("v25.db")), "Schema 25 to 27 migration failed.")) return 1;

    const QString appData = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir(appData).removeRecursively();
    TestEnvironmentGuard cleanup(appData);
    DatabaseManager& manager = DatabaseManager::instance();
    if (!require(manager.initialize(), "Unable to initialize test database.")) return 1;
    QSqlDatabase db = manager.database();
    QSqlQuery seed(db);
    const QString now = "2026-01-01T00:00:00.000Z";
    if (!require(seed.exec("INSERT INTO minifig_catalog(name,num_parts,is_active,created_utc,modified_utc) "
                           "VALUES('Pirate',4,1,'" + now + "','" + now + "')"), "Seed Minifig failed.")) return 1;
    const int minifigId = seed.lastInsertId().toInt();
    seed.prepare("INSERT INTO minifig_external_identifier(minifig_catalog_id,provider,external_id,source,is_active,created_utc,modified_utc) "
                 "VALUES(:id,'Rebrickable','fig-005210','test',1,:now,:now)");
    seed.bindValue(":id", minifigId); seed.bindValue(":now", now);
    if (!require(seed.exec(), "Seed Minifig identity failed.")) return 1;
    if (!require(seed.exec("INSERT INTO part(part_number,name,rebrickable_part_id,is_active,created_utc,modified_utc,material) VALUES"
                           "('p1','Part 1','p1',1,'" + now + "','" + now + "','Plastic'),"
                           "('p2','Part 2','p2',1,'" + now + "','" + now + "','Plastic')"), "Seed Parts failed.")) return 1;
    if (!require(seed.exec("INSERT INTO color(name,rebrickable_id,created_utc,modified_utc) VALUES"
                           "('Red',1,'" + now + "','" + now + "'),('Blue',2,'" + now + "','" + now + "')"), "Seed Colors failed.")) return 1;

    const QByteArray valid("Part,Color,Quantity,Is Spare\np1,1,2,FALSE\np1,1,3,0\np1,1,1,TRUE\np2,2,4,no\n");
    const QString matching = files.filePath("rebrickable_parts_fig-005210-pirate.csv");
    if (!require(writeBytes(matching, valid), "Write valid CSV failed.")) return 1;
    RebrickableMinifigPartsImporter importer;
    auto result = importer.importFile(minifigId, matching);
    if (!require(result.success && result.rowsRead == 4 && result.compositionRows == 3,
                 "Valid CSV import or duplicate/spare aggregation failed: " + result.message)) return 1;
    MinifigCatalogPartRepository repository;
    QList<MinifigCatalogPart> parts = repository.listForMinifig(minifigId);
    if (!require(parts.size() == 3 && parts.at(0).quantityRequired == 5
                 && !parts.at(0).isSpare && parts.at(1).quantityRequired == 4
                 && parts.at(2).isSpare && parts.at(0).partNumber == "p1"
                 && parts.at(0).partName == "Part 1" && parts.at(0).colorName == "Red"
                 && parts.at(0).rebrickableColorId == 1,
                 "Persisted composition or joined display data is incorrect.")) return 1;

    const QByteArray zipCsv("Part,Color,Quantity,Is Spare\np2,2,7,yes\n");
    const QString zipPath = files.filePath("rebrickable_parts_fig-005210-pirate.csv.zip");
    if (!require(writeZip(zipPath, {{"nested/rebrickable_parts_fig-005210-pirate.csv", zipCsv}}),
                 "Write ZIP failed.")) return 1;
    result = importer.importFile(minifigId, zipPath);
    if (!require(result.success && repository.listForMinifig(minifigId).size() == 1
                 && repository.listForMinifig(minifigId).first().isSpare,
                 "ZIP replace import failed: " + result.message)) return 1;

    const QString noIdentityName = files.filePath("custom-parts.csv");
    if (!require(writeBytes(noIdentityName, "Part,Color,Quantity\np1,1,6\n"), "Write CSV failed.")) return 1;
    result = importer.importFile(minifigId, noIdentityName);
    const QString baseline = snapshot(db, minifigId);
    if (!require(result.success && !baseline.isEmpty(), "Missing Is Spare/default or identifier-free filename failed.")) return 1;
    result = importer.importFile(minifigId, noIdentityName);
    if (!require(result.success && snapshot(db, minifigId) == baseline, "Reimport accumulated quantities.")) return 1;

    struct Rejection { QString name; QByteArray csv; QString message; };
    const QList<Rejection> rejections = {
        {"wrong_fig-999999.csv", "Part,Color,Quantity\np1,1,1\n", "mismatch"},
        {"unknown-part.csv", "Part,Color,Quantity\nmissing,1,1\n", "unresolved Part"},
        {"unknown-color.csv", "Part,Color,Quantity\np1,999,1\n", "unresolved Color"},
        {"bad-quantity.csv", "Part,Color,Quantity\np1,1,zero\n", "malformed quantity"},
        {"bad-spare.csv", "Part,Color,Quantity,Is Spare\np1,1,1,perhaps\n", "malformed spare"},
        {"bad-header.csv", "Part,Colour,Quantity\np1,1,1\n", "invalid headers"}
    };
    for (const Rejection& rejection : rejections) {
        const QString path = files.filePath(rejection.name);
        if (!require(writeBytes(path, rejection.csv), "Unable to write " + rejection.message)) return 1;
        result = importer.importFile(minifigId, path);
        if (!require(!result.success && snapshot(db, minifigId) == baseline,
                     rejection.message + " did not preserve composition.")) return 1;
    }

    const QString ambiguous = files.filePath("ambiguous.zip");
    if (!require(writeZip(ambiguous, {{"a.csv", valid}, {"b.csv", valid}}), "Write ambiguous ZIP failed.")) return 1;
    result = importer.importFile(minifigId, ambiguous);
    if (!require(!result.success && snapshot(db, minifigId) == baseline,
                 "Ambiguous ZIP was not rejected safely.")) return 1;

    QSqlQuery ids(db);
    if (!require(ids.exec("SELECT id,rebrickable_part_id FROM part ORDER BY id") && ids.next(),
                 "Unable to read seeded Part IDs.")) return 1;
    const int firstPartId = ids.value(0).toInt();
    if (!require(ids.next(), "Second Part ID missing.")) return 1;
    const int secondPartId = ids.value(0).toInt();
    QSqlQuery trigger(db);
    if (!require(trigger.exec(QString("CREATE TRIGGER force_late_minifig_failure BEFORE INSERT ON minifig_catalog_part "
                                     "WHEN NEW.part_id=%1 BEGIN SELECT RAISE(ABORT,'forced late failure'); END")
                                  .arg(secondPartId)), "Unable to create failure trigger.")) return 1;
    const QString lateFile = files.filePath("late-failure.csv");
    if (!require(writeBytes(lateFile, "Part,Color,Quantity\np1,1,1\np2,2,1\n"), "Write late failure CSV failed.")) return 1;
    result = importer.importFile(minifigId, lateFile);
    if (!require(!result.success && firstPartId < secondPartId
                 && result.message.contains("forced late failure")
                 && snapshot(db, minifigId) == baseline,
                 "Late database failure did not roll back the exact prior composition.")) return 1;
    qInfo() << "M23.7.3 Minifig parts import validation passed.";
    return 0;
}
