#include "../src/database/DatabaseManager.h"
#include "../src/import/RebrickablePartRelationshipImporter.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QUuid>
#include <zlib.h>

namespace {
bool require(bool value, const QString& message) { if (!value) qCritical().noquote() << message; return value; }
void u16(QByteArray& b, quint16 v) { b += char(v); b += char(v >> 8); }
void u32(QByteArray& b, quint32 v) { u16(b, quint16(v)); u16(b, quint16(v >> 16)); }
bool writeFile(const QString& path, const QByteArray& bytes) { QFile f(path); return f.open(QIODevice::WriteOnly) && f.write(bytes) == bytes.size(); }
bool writeZip(const QString& path, const QList<QPair<QByteArray, QByteArray>>& entries)
{
    struct E { QByteArray n; quint32 crc, size, offset; }; QByteArray out; QList<E> central;
    for (const auto& p : entries) {
        E e{p.first, quint32(crc32(0L, reinterpret_cast<const Bytef*>(p.second.constData()), uInt(p.second.size()))), quint32(p.second.size()), quint32(out.size())};
        u32(out, 0x04034b50); u16(out, 20); u16(out, 0); u16(out, 0); u16(out, 0); u16(out, 0);
        u32(out, e.crc); u32(out, e.size); u32(out, e.size); u16(out, quint16(e.n.size())); u16(out, 0); out += e.n; out += p.second; central += e;
    }
    quint32 offset = quint32(out.size());
    for (const E& e : central) {
        u32(out, 0x02014b50); u16(out, 20); u16(out, 20); u16(out, 0); u16(out, 0); u16(out, 0); u16(out, 0);
        u32(out, e.crc); u32(out, e.size); u32(out, e.size); u16(out, quint16(e.n.size())); u16(out, 0); u16(out, 0);
        u16(out, 0); u16(out, 0); u32(out, 0); u32(out, e.offset); out += e.n;
    }
    u32(out, 0x06054b50); u16(out, 0); u16(out, 0); u16(out, quint16(central.size())); u16(out, quint16(central.size()));
    u32(out, quint32(out.size()) - offset); u32(out, offset); u16(out, 0); return writeFile(path, out);
}
int activeCount(QSqlDatabase db) { QSqlQuery q(db); return q.exec("SELECT COUNT(*) FROM part_relationship WHERE is_active=1") && q.next() ? q.value(0).toInt() : -1; }
class Cleanup { QString path; public: explicit Cleanup(QString p):path(std::move(p)){} ~Cleanup(){ DatabaseManager::instance().close(); QDir(path).removeRecursively(); } };
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv); QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setOrganizationName("RFStateSideTests");
    QCoreApplication::setApplicationName("PartRelationship_" + QUuid::createUuid().toString(QUuid::WithoutBraces));
    const QString appData = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation); Cleanup cleanup(appData);
    QTemporaryDir files; if (!require(files.isValid(), "Temporary directory failed.")) return 1;
    if (!require(DatabaseManager::instance().initialize(), "Database initialization failed.")) return 1;
    QSqlDatabase db = DatabaseManager::instance().database(); QSqlQuery q(db);
    const QString now = "2026-01-01T00:00:00.000Z";
    if (!require(q.exec("INSERT INTO part(part_number,name,is_active,created_utc,modified_utc,material) VALUES ('p1','One',1,'"+now+"','"+now+"','Plastic'),('p2','Two',1,'"+now+"','"+now+"','Plastic')"), "Part seed failed.")) return 1;
    const QByteArray csv("rel_type,child_part_num,parent_part_num\nA,p2,p1\n");
    RebrickablePartRelationshipImporter importer;
    const QString direct = files.filePath("part_relationships.csv"); writeFile(direct, csv);
    if (!require(importer.importFile(direct).success && activeCount(db)==1, "Direct CSV regression failed.")) return 1;
    auto rejected = [&](const QString& name) { const int before=activeCount(db); auto r=importer.importFile(files.filePath(name)); return !r.success && activeCount(db)==before; };
    writeZip(files.filePath("root.zip"), {{"part_relationships.csv", csv}});
    if (!require(importer.importFile(files.filePath("root.zip")).success, "Root ZIP failed.")) return 1;
    writeZip(files.filePath("nested.zip"), {{"folder/part_relationships.CSV", csv}});
    if (!require(importer.importFile(files.filePath("nested.zip")).success, "Nested ZIP failed.")) return 1;
    writeFile(files.filePath("invalid.zip"), "not a zip"); if (!require(rejected("invalid.zip"), "Invalid ZIP changed data.")) return 1;
    writeZip(files.filePath("missing.zip"), {{"other.txt", csv}}); if (!require(rejected("missing.zip"), "Missing CSV changed data.")) return 1;
    writeZip(files.filePath("ambiguous.zip"), {{"a/part_relationships.csv", csv},{"b/part_relationships.csv",csv}}); if (!require(rejected("ambiguous.zip"), "Ambiguous ZIP changed data.")) return 1;
    writeZip(files.filePath("traversal.zip"), {{"../part_relationships.csv", csv}}); if (!require(rejected("traversal.zip"), "Traversal ZIP changed data.")) return 1;
    writeFile(files.filePath("invalid.csv"), "wrong,header\nx,y\n"); if (!require(rejected("invalid.csv"), "CSV validation changed data.")) return 1;
    const QString rollback = files.filePath("rollback.csv");
    writeFile(rollback, "rel_type,child_part_num,parent_part_num\nM,p2,p1\nB,p1,p2\n");
    if (!require(q.exec("CREATE TRIGGER force_relationship_failure BEFORE INSERT ON part_relationship "
                        "WHEN NEW.source_relationship_type='B' BEGIN SELECT RAISE(ABORT,'forced'); END"),
                 "Failure trigger creation failed.")) return 1;
    const auto failed = importer.importFile(rollback);
    QSqlQuery preserved(db);
    if (!require(!failed.success && activeCount(db)==1
                 && preserved.exec("SELECT relationship_type FROM part_relationship WHERE is_active=1")
                 && preserved.next() && preserved.value(0).toString()=="Alternate",
                 "Late database failure did not roll back all relationship changes.")) return 1;
    return 0;
}
