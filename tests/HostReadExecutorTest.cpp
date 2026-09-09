#include "../src/database/DatabaseManager.h"
#include "../src/database/DatabaseSchema.h"
#include "../src/services/application/HostReadExecutor.h"

#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTimer>
#include <cstdio>

namespace {
bool check(bool value, const char* message)
{
    if (!value) std::fprintf(stderr, "FAILED: %s\n", message);
    return value;
}

bool waitFor(const std::function<void(QEventLoop&)>& start)
{
    QEventLoop loop;
    bool timedOut = false;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, [&]() { timedOut = true; loop.quit(); });
    timer.start(5000);
    start(loop);
    loop.exec();
    return !timedOut;
}

bool seedDatabase(const QString& path, const QString& workspaceName)
{
    const QString name = QStringLiteral("HostReadSeed_%1").arg(workspaceName);
    bool ok = false;
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), name);
        database.setDatabaseName(path);
        ok = database.open() && DatabaseSchema::initialize(database);
        QSqlQuery query(database);
        query.prepare(QStringLiteral(
            "INSERT INTO workspace(name,description,created_utc,modified_utc,is_active) "
            "VALUES(:name,'','2026-01-01T00:00:00.000Z','2026-01-01T00:00:00.000Z',1)"));
        query.bindValue(QStringLiteral(":name"), workspaceName);
        ok = ok && query.exec();
        database.close();
    }
    QSqlDatabase::removeDatabase(name);
    return ok;
}
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("RFStateSideTests"));
    app.setApplicationName(QStringLiteral("HostReadExecutor"));
    QStandardPaths::setTestModeEnabled(true);
    QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)).removeRecursively();

    QTemporaryDir directory;
    bool ok = check(directory.isValid(), "temporary directory");
    const QString hostPath = directory.filePath(QStringLiteral("host.db"));
    ok &= check(seedDatabase(hostPath, QStringLiteral("Host Workspace")), "seed Host database");
    ok &= check(DatabaseManager::instance().initialize(), "initialize poison default database");
    QSqlQuery poison(DatabaseManager::instance().database());
    ok &= check(poison.exec(QStringLiteral(
        "INSERT INTO workspace(name,description,created_utc,modified_utc,is_active) "
        "VALUES('Poison Local','','2026-01-01','2026-01-01',1)")), "seed poison local database");

    QString connectionName;
    {
        HostReadExecutor executor(hostPath);
        connectionName = executor.connectionName();
        ok &= check(connectionName.startsWith(QStringLiteral("BrickSuite_HostRead_")),
                    "unique Host connection name");
        QList<Workspace> workspaces;
        QString failure;
        ok &= check(waitFor([&](QEventLoop& loop) {
            executor.listWorkspaces(&app, [&](const QList<Workspace>& result) {
                workspaces = result; loop.quit();
            }, [&](const QString& error) { failure = error; loop.quit(); });
        }), "Workspace read completion");
        ok &= check(failure.isEmpty() && workspaces.size() == 1
                        && workspaces.first().name() == QStringLiteral("Host Workspace"),
                    "worker uses Host database, never poison default database");

        InventorySearchCriteria inventoryCriteria;
        inventoryCriteria.workspaceId = workspaces.first().id();
        InventoryApplicationService::Page inventory;
        ok &= check(waitFor([&](QEventLoop& loop) {
            executor.searchInventory(inventoryCriteria, &app,
                [&](const InventoryApplicationService::Page& result) { inventory = result; loop.quit(); },
                [&](const QString& error) { failure = error; loop.quit(); });
        }), "Inventory read completion");
        ok &= check(inventory.rows.isEmpty() && inventory.total == 0, "empty Inventory projection");

        QList<Build> builds;
        ok &= check(waitFor([&](QEventLoop& loop) {
            executor.listBuilds(workspaces.first().id(), false, &app,
                [&](const QList<Build>& result) { builds = result; loop.quit(); });
        }), "Build read completion");
        ok &= check(builds.isEmpty(), "empty Build projection");

        CollectionSearchCriteria collectionCriteria;
        collectionCriteria.workspaceId = workspaces.first().id();
        CollectionApplicationService::Page collection;
        ok &= check(waitFor([&](QEventLoop& loop) {
            executor.searchCollection(collectionCriteria, &app,
                [&](const CollectionApplicationService::Page& result) { collection = result; loop.quit(); });
        }), "Collection read completion");
        ok &= check(collection.rows.isEmpty() && collection.total == 0, "empty Collection projection");

        executor.shutdown();
        ok &= check(!executor.isAccepting(), "shutdown stops task acceptance");
    }
    ok &= check(!QSqlDatabase::contains(connectionName), "named worker connection removed");
    DatabaseManager::instance().close();
    QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)).removeRecursively();
    return ok ? 0 : 1;
}
