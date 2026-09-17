#include "../src/database/DatabaseSchema.h"
#include "../src/services/application/HostBuildMutationService.h"
#include "../src/services/application/HostWriteExecutor.h"
#include "../src/services/application/dto/RemoteBuildMutationDtos.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTimer>
#include <QUuid>
#include <cstdio>

namespace {
bool require(bool value, const char* message)
{
    if (!value) std::fprintf(stderr, "FAILED: %s\n", message);
    return value;
}

int scalar(QSqlDatabase database, const QString& sql)
{
    QSqlQuery query(database);
    return query.exec(sql) && query.next() ? query.value(0).toInt() : -1;
}

struct Awaited {
    bool success = false;
    RemoteMutationDto::Result result;
    RemoteMutationDto::Error error;
};

Awaited run(HostWriteExecutor& executor, const RemoteBuildMutationDto::Request& request)
{
    const auto metadata = RemoteBuildMutationDto::toMetadata(QStringLiteral("builds.add"), request);
    RemoteMutationDto::Error parseError;
    auto mutation = HostBuildMutationService::createMutation(
        QStringLiteral("builds.add"), metadata, &parseError);
    if (!mutation) return {false, {}, parseError};

    RemoteMutationDto::RequestContext context;
    context.operation = QStringLiteral("builds.add");
    context.workspaceId = request.workspaceId;
    context.mutationId = request.mutationId;
    context.clientIdentity = QStringLiteral("host-set-build-test");
    context.protocolMinor = 5;

    Awaited awaited;
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    executor.enqueue(context, RemoteMutationDto::requestHash(context.operation, metadata),
        std::move(mutation), &loop,
        [&](const RemoteMutationDto::Result& result) {
            awaited.success = true;
            awaited.result = result;
            loop.quit();
        },
        [&](const RemoteMutationDto::Error& error) {
            awaited.error = error;
            loop.quit();
        });
    timer.start(10000);
    loop.exec();
    return awaited;
}
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir temporary;
    if (!require(temporary.isValid(), "temporary directory unavailable")) return 1;
    const QString path = temporary.filePath(QStringLiteral("host-set-build.sqlite"));
    const QString connection = QStringLiteral("host-set-build-seed-%1")
                                   .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection);
    database.setDatabaseName(path);
    if (!require(database.open() && DatabaseSchema::initialize(database), "schema initialization failed"))
        return 1;

    QSqlQuery query(database);
    const QString now = QStringLiteral("2026-09-17T12:00:00.000Z");
    if (!require(query.exec(QStringLiteral(
            "INSERT INTO workspace(id,name,description,is_active,created_utc,modified_utc) "
            "VALUES(1,'Host Workspace','',1,'%1','%1')").arg(now)), "workspace seed failed")
        || !require(query.exec(QStringLiteral(
            "INSERT INTO set_catalog(id,set_number,name,year,theme_id,num_parts,image_url,created_utc,modified_utc) "
            "VALUES(10,'1000-1','Snapshot Set',2026,1,2,'','%1','%1'),"
            "(11,'2000-1','Empty Set',2026,1,0,'','%1','%1')").arg(now)), "Set seed failed")
        || !require(query.exec(QStringLiteral(
            "INSERT INTO part(id,part_number,name,rebrickable_part_id,is_active,created_utc,modified_utc,material) "
            "VALUES(20,'3001','Brick 2 x 4','3001',1,'%1','%1','Plastic')").arg(now)), "Part seed failed")
        || !require(query.exec(QStringLiteral(
            "INSERT INTO color(id,name,rebrickable_id,created_utc,modified_utc) "
            "VALUES(30,'Red',4,'%1','%1')").arg(now)), "Color seed failed")
        || !require(query.exec(QStringLiteral(
            "INSERT INTO set_inventory_revision(id,provider,external_inventory_id,set_catalog_id,version,is_active,is_preferred,created_utc,modified_utc) "
            "VALUES(40,'Rebrickable','inventory-1000',10,2,1,1,'%1','%1')").arg(now)), "revision seed failed")
        || !require(query.exec(QStringLiteral(
            "INSERT INTO set_inventory_part(set_inventory_revision_id,part_id,color_id,quantity,is_spare,image_url,created_utc,modified_utc) "
            "VALUES(40,20,30,2,0,'','%1','%1'),(40,20,30,1,1,'','%1','%1')").arg(now)), "composition seed failed"))
        return 1;

    database.close();
    database = {};
    QSqlDatabase::removeDatabase(connection);

    int publications = 0;
    HostMutationPublicationService::Workflow publishedWorkflow =
        HostMutationPublicationService::Workflow::BuildMetadata;
    HostWriteExecutor executor(path, [&](auto workflow, const auto&) {
        ++publications;
        publishedWorkflow = workflow;
    });

    RemoteBuildMutationDto::Request request;
    request.workspaceId = 1;
    request.mutationId = RemoteMutationDto::newMutationId();
    request.buildType = QStringLiteral("Set");
    request.reference = QStringLiteral("1000-1");
    request.inventoryMode = QStringLiteral("Stock");
    request.initialStatus = QStringLiteral("Planned");
    request.name = QStringLiteral("Remote Snapshot");
    request.notes = QStringLiteral("Preserved note");

    const Awaited created = run(executor, request);
    const Awaited replayed = run(executor, request);

    const QString verifyConnection = QStringLiteral("host-set-build-verify-%1")
                                         .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    QSqlDatabase verify = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), verifyConnection);
    verify.setDatabaseName(path);
    if (!require(verify.open(), "verification database open failed")) return 1;
    bool ok = true;
    ok &= require(created.success && !created.result.replayed, "catalog Set/Stock mutation failed");
    ok &= require(replayed.success && replayed.result.replayed, "catalog Set/Stock mutation did not replay");
    ok &= require(scalar(verify, "SELECT COUNT(*) FROM build WHERE set_catalog_id=10") == 1,
                  "catalog Set/Stock replay created a duplicate Build");
    ok &= require(scalar(verify, "SELECT COUNT(*) FROM build_requirement") == 1
                  && scalar(verify, "SELECT SUM(quantity_required) FROM build_requirement") == 2,
                  "authoritative non-spare requirement snapshot is incorrect");
    ok &= require(scalar(verify, "SELECT COUNT(*) FROM build_allocation") == 0,
                  "catalog Set/Stock creation allocated Inventory");
    ok &= require(scalar(verify, "SELECT COUNT(*) FROM build WHERE notes='Preserved note'") == 1,
                  "generic builds.add metadata was not preserved");
    ok &= require(publications == 1
                  && publishedWorkflow == HostMutationPublicationService::Workflow::BuildRequirements,
                  "catalog Set/Stock creation published the wrong invalidation");

    RemoteBuildMutationDto::Request missing = request;
    missing.mutationId = RemoteMutationDto::newMutationId();
    missing.reference = QStringLiteral("2000-1");
    missing.name = QStringLiteral("Must Not Persist");
    const Awaited rejected = run(executor, missing);
    ok &= require(!rejected.success && rejected.error.outcome == RemoteMutationDto::Outcome::DefinitiveFailure,
                  "missing composition was not rejected definitively");
    ok &= require(scalar(verify, "SELECT COUNT(*) FROM build WHERE name='Must Not Persist'") == 0,
                  "missing composition left an empty Build");

    RemoteBuildMutationDto::Request moc = request;
    moc.mutationId = RemoteMutationDto::newMutationId();
    moc.buildType = QStringLiteral("MOC");
    moc.reference = QStringLiteral("MOC-1");
    moc.name = QStringLiteral("Manual MOC");
    const Awaited mocCreated = run(executor, moc);
    ok &= require(mocCreated.success
                  && scalar(verify, "SELECT COUNT(*) FROM build WHERE name='Manual MOC'") == 1,
                  "generic MOC creation regressed");
    ok &= require(scalar(verify,
            "SELECT COUNT(*) FROM build_requirement br JOIN build b ON b.id=br.build_id "
            "WHERE b.name='Manual MOC'") == 0,
                  "generic MOC unexpectedly received requirements");

    executor.shutdown();
    verify.close();
    verify = {};
    QSqlDatabase::removeDatabase(verifyConnection);
    return ok ? 0 : 1;
}
