#include "../src/services/builds/PartUsageDiscoveryService.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTextStream>
#include <QTimer>

namespace {
bool require(bool value, const QString& message)
{
    if (!value)
        QTextStream(stderr) << message << '\n';
    return value;
}
}

int main(int argc, char** argv)
{
    QCoreApplication application(argc, argv);
    QTemporaryDir directory;
    bool ok = require(directory.isValid(), QStringLiteral("temporary directory"));
    const QString path = directory.filePath(QStringLiteral("catalog.db"));

    {
        QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                                           QStringLiteral("setup"));
        database.setDatabaseName(path);
        ok &= require(database.open(), database.lastError().text());
        QSqlQuery query(database);
        const QStringList statements = {
            QStringLiteral("CREATE TABLE theme_catalog(id INTEGER PRIMARY KEY,name TEXT)"),
            QStringLiteral("CREATE TABLE set_catalog(id INTEGER PRIMARY KEY,set_number TEXT,name TEXT,year INTEGER,theme_id INTEGER,num_parts INTEGER,image_url TEXT)"),
            QStringLiteral("CREATE TABLE set_inventory_revision(id INTEGER PRIMARY KEY,set_catalog_id INTEGER,provider TEXT,is_active INTEGER,is_preferred INTEGER)"),
            QStringLiteral("CREATE TABLE set_inventory_part(set_inventory_revision_id INTEGER,part_id INTEGER,color_id INTEGER,quantity INTEGER,is_spare INTEGER)"),
            QStringLiteral("CREATE TABLE set_catalog_part(set_catalog_id INTEGER,part_id INTEGER,color_id INTEGER,quantity_required INTEGER,is_spare INTEGER)"),
            QStringLiteral("INSERT INTO theme_catalog VALUES(1,'City')"),
            QStringLiteral("INSERT INTO set_catalog VALUES(1,'100-1','First',2024,1,1,''),(2,'200-1','Second',2024,1,1,'')"),
            QStringLiteral("INSERT INTO set_catalog_part VALUES(1,1,1,1,0),(2,2,1,1,0)")
        };
        for (const QString& statement : statements)
            ok &= require(query.exec(statement), query.lastError().text());
        database.close();
        database = {};
        QSqlDatabase::removeDatabase(QStringLiteral("setup"));
    }

    int callbacks = 0;
    int returnedSetId = 0;
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);

    {
        PartUsageDiscoveryService service(path);
        PartUsageSearch first;
        first.criteria = {{1, QStringLiteral("1"), QStringLiteral("First"), 0,
                           QStringLiteral("Any Color"), 1}};
        PartUsageSearch second;
        second.criteria = {{2, QStringLiteral("2"), QStringLiteral("Second"), 0,
                            QStringLiteral("Any Color"), 1}};
        service.search(first, &loop, [&](quint64, const PartUsageSearchResult&) {
            ++callbacks;
        });
        service.search(second, &loop, [&](quint64, const PartUsageSearchResult& result) {
            ++callbacks;
            if (result.success && result.sets.size() == 1)
                returnedSetId = result.sets.constFirst().setCatalogId;
            loop.quit();
        });
        timeout.start(5000);
        loop.exec();
        ok &= require(timeout.isActive(), QStringLiteral("asynchronous search timed out"));
        ok &= require(callbacks == 1 && returnedSetId == 2,
                      QStringLiteral("superseded search delivered a stale callback"));

        callbacks = 0;
        service.search(first, &loop, [&](quint64, const PartUsageSearchResult&) {
            ++callbacks;
        });
        service.cancel();
        QTimer::singleShot(100, &loop, &QEventLoop::quit);
        loop.exec();
        ok &= require(callbacks == 0, QStringLiteral("cancelled search delivered a callback"));
    }

    ok &= require(QFileInfo::exists(path), QStringLiteral("worker modified catalog storage"));
    if (ok)
        QTextStream(stdout) << "PartUsageDiscoveryServiceTest passed\n";
    return ok ? 0 : 1;
}
