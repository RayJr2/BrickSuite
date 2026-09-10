#include "../src/database/DatabaseSchema.h"
#include "../src/services/application/HostWriteExecutor.h"
#include "../src/services/application/HostMutationProtocolService.h"
#include "../src/services/application/dto/RemoteMutationDtos.h"
#include "../src/network/BrickSuiteOperationDispatcher.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QFileInfo>
#include <QJsonObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSemaphore>
#include <QTemporaryDir>
#include <QTimer>
#include <QUuid>
#include <iostream>

namespace {
bool check(bool value, const char* message)
{
    if (!value) std::cerr << "FAIL: " << message << '\n';
    return value;
}

RemoteMutationDto::RequestContext context(const QString& id)
{ return {QStringLiteral("test.mutation"), 1, id, QStringLiteral("test-client"), 1, 2}; }

struct Awaited {
    bool success = false;
    RemoteMutationDto::Result result;
    RemoteMutationDto::Error error;
};

Awaited run(HostWriteExecutor& executor, const RemoteMutationDto::RequestContext& ctx,
            const QString& hash, HostWriteExecutor::Mutation mutation)
{
    Awaited value;
    QEventLoop loop;
    executor.enqueue(ctx, hash, std::move(mutation), &loop,
        [&](const RemoteMutationDto::Result& result) { value.success=true; value.result=result; loop.quit(); },
        [&](const RemoteMutationDto::Error& error) { value.error=error; loop.quit(); });
    QTimer::singleShot(20000, &loop, &QEventLoop::quit);
    loop.exec();
    return value;
}
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir temporary;
    if (!check(temporary.isValid(), "temporary directory")) return 1;
    const QString path = temporary.filePath(QStringLiteral("foundation.db"));
    const QString connection = QStringLiteral("foundation-%1").arg(QUuid::createUuid().toString());
    QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection);
    database.setDatabaseName(path);
    if (!check(database.open(), "open database") || !check(DatabaseSchema::initialize(database), "schema 35")) return 1;
    QSqlQuery seed(database);
    if (!check(seed.exec(QStringLiteral("INSERT INTO workspace(name,description,created_utc,modified_utc,is_active) "
        "VALUES('Test','',CURRENT_TIMESTAMP,CURRENT_TIMESTAMP,1)")), "seed workspace")
        || !check(seed.exec(QStringLiteral("DROP TABLE remote_mutation_receipt")), "stage schema 34")
        || !check(seed.exec(QStringLiteral("UPDATE schema_version SET version=34")), "set schema 34")
        || !check(DatabaseSchema::initialize(database), "migrate schema 34 to 35")
        || !check(seed.exec(QStringLiteral("SELECT COUNT(*) FROM workspace")) && seed.next()
                  && seed.value(0).toInt()==1, "migration preserves operational data")
        || !check(seed.exec(QStringLiteral("SELECT COUNT(*) FROM sqlite_master WHERE type='index' "
                  "AND name IN ('idx_remote_mutation_receipt_committed_utc',"
                  "'idx_remote_mutation_receipt_workspace')")) && seed.next()
                  && seed.value(0).toInt()==2, "receipt indexes exist")
        || !check(seed.exec(QStringLiteral("CREATE TABLE mutation_probe(value INTEGER NOT NULL)")), "probe table")
        || !check(seed.exec(QStringLiteral("INSERT INTO mutation_probe VALUES(0)")), "seed probe")) return 1;
    database.close(); database = {}; QSqlDatabase::removeDatabase(connection);

    RemoteMutationDto::Metadata first{1, RemoteMutationDto::newMutationId(),
                                      {{QStringLiteral("prior"), 0}}, {{QStringLiteral("amount"), 1}}};
    RemoteMutationDto::Metadata reordered{1, first.mutationId,
                                      {{QStringLiteral("prior"), 0}},
                                      {{QStringLiteral("z"), 2}, {QStringLiteral("amount"), 1}}};
    first.mutation.insert(QStringLiteral("z"), 2);
    if (!check(RemoteMutationDto::requestHash(QStringLiteral("test.mutation"), first)
               == RemoteMutationDto::requestHash(QStringLiteral("test.mutation"), reordered),
               "canonical hash stable")) return 1;
    RemoteMutationDto::Metadata changed = first;
    changed.mutation.insert(QStringLiteral("amount"), 2);
    if (!check(RemoteMutationDto::requestHash(QStringLiteral("test.mutation"), first)
               != RemoteMutationDto::requestHash(QStringLiteral("test.mutation"), changed),
               "business payload changes hash")) return 1;

    int publications = 0;
    QString failedMutationId;
    auto mutation = [](const QSqlDatabase& db) {
        QSqlQuery query(db); query.exec(QStringLiteral("UPDATE mutation_probe SET value=value+1"));
        HostWriteExecutor::MutationOutcome out; out.success=query.numRowsAffected()==1;
        out.authoritative={{QStringLiteral("value"), 1}}; return out;
    };
    const QString hash = RemoteMutationDto::requestHash(QStringLiteral("test.mutation"), first);
    {
        HostWriteExecutor executor(path, [&](auto, const auto&){ ++publications; });
        const Awaited committed = run(executor, context(first.mutationId), hash, mutation);
        if (!check(committed.success && !committed.result.replayed, "first mutation commits")) return 1;
    }
    {
        HostWriteExecutor restarted(path, [&](auto, const auto&){ ++publications; });
        const Awaited replay = run(restarted, context(first.mutationId), hash, mutation);
        if (!check(replay.success && replay.result.replayed, "restart replay")
            || !check(publications == 1, "replay does not republish")) return 1;
        const Awaited conflict = run(restarted, context(first.mutationId),
            RemoteMutationDto::requestHash(QStringLiteral("test.mutation"), changed), mutation);
        if (!check(!conflict.success && conflict.error.code==QStringLiteral("IDEMPOTENCY_CONFLICT"),
                   "idempotency conflict")) return 1;

        RemoteMutationDto::Metadata unknownWorkspace{999, RemoteMutationDto::newMutationId(), {}, {}};
        const Awaited missing = run(restarted,
            {QStringLiteral("test.mutation"), 999, unknownWorkspace.mutationId,
             QStringLiteral("test-client"), 1, 2},
            RemoteMutationDto::requestHash(QStringLiteral("test.mutation"), unknownWorkspace), mutation);
        if (!check(!missing.success && missing.error.code==QStringLiteral("WORKSPACE_NOT_FOUND"),
                   "unknown Workspace rejected")) return 1;

        RemoteMutationDto::Metadata failing{1, RemoteMutationDto::newMutationId(), {}, {}};
        failedMutationId = failing.mutationId;
        const Awaited rolledBack = run(restarted, context(failing.mutationId),
            RemoteMutationDto::requestHash(QStringLiteral("test.mutation"), failing),
            [](const QSqlDatabase& db) {
                QSqlQuery query(db); query.exec(QStringLiteral("UPDATE mutation_probe SET value=99"));
                HostWriteExecutor::MutationOutcome out;
                out.error={QStringLiteral("CONFLICT"), QStringLiteral("Synthetic failure."), false};
                return out;
            });
        if (!check(!rolledBack.success, "failure returned")) return 1;
    }

    // Keep one mutation executing so the bounded queue can be filled without sleeps.
    {
        HostWriteExecutor queued(path);
        QSemaphore entered, release;
        bool overflowBusy = false;
        const auto base = context(RemoteMutationDto::newMutationId());
        queued.enqueue(base, QStringLiteral("blocking"),
            [&](const QSqlDatabase&) {
                entered.release(); release.acquire();
                HostWriteExecutor::MutationOutcome out; out.success=true; return out;
            }, &app, [](const auto&){}, [&](const auto& error){ overflowBusy |= error.code==QStringLiteral("BUSY"); });
        entered.acquire();
        for (int i=0; i<16; ++i) {
            auto next = context(RemoteMutationDto::newMutationId());
            queued.enqueue(next, QStringLiteral("queued-%1").arg(i),
                [](const QSqlDatabase&) { HostWriteExecutor::MutationOutcome out; out.success=true; return out; },
                &app, [](const auto&){}, [&](const auto& error){ overflowBusy |= error.code==QStringLiteral("BUSY"); });
        }
        if (!check(queued.queuedMutationCount() <= HostWriteExecutor::MaximumQueuedMutations,
                   "queue remains bounded")) return 1;
        release.release();
        QEventLoop drain;
        QTimer poll;
        QObject::connect(&poll, &QTimer::timeout, &drain, [&] {
            if (queued.queuedMutationCount()==0 && overflowBusy) drain.quit();
        });
        poll.start(5); QTimer::singleShot(10000, &drain, &QEventLoop::quit); drain.exec();
        if (!check(overflowBusy && queued.queuedMutationCount()==0, "queue overflow returns BUSY")) return 1;
    }

    // Force receipt insertion to fail after the domain callback has changed data.
    const QString receiptFailureId = RemoteMutationDto::newMutationId();
    const QString adminName = QStringLiteral("admin-%1").arg(QUuid::createUuid().toString());
    QSqlDatabase admin = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), adminName);
    admin.setDatabaseName(path); admin.open();
    QSqlQuery adminQuery(admin);
    if (!check(adminQuery.exec(QStringLiteral("CREATE TRIGGER reject_receipt BEFORE INSERT ON "
        "remote_mutation_receipt BEGIN SELECT RAISE(ABORT,'test'); END")), "receipt failure trigger")) return 1;
    admin.close(); admin={}; QSqlDatabase::removeDatabase(adminName);
    {
        HostWriteExecutor executor(path);
        const Awaited failure = run(executor, context(receiptFailureId), QStringLiteral("receipt-failure"),
            [](const QSqlDatabase& db) {
                QSqlQuery query(db); query.exec(QStringLiteral("UPDATE mutation_probe SET value=88"));
                HostWriteExecutor::MutationOutcome out; out.success=true; return out;
            });
        if (!check(!failure.success && failure.error.code==QStringLiteral("INTERNAL_ERROR"),
                   "receipt failure rejects transaction")) return 1;
    }
    QSqlDatabase cleanup = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), adminName);
    cleanup.setDatabaseName(path); cleanup.open(); QSqlQuery cleanupQuery(cleanup);
    if (!check(cleanupQuery.exec(QStringLiteral("DROP TRIGGER reject_receipt")), "drop receipt trigger")) return 1;
    cleanup.close(); cleanup={}; QSqlDatabase::removeDatabase(adminName);
    {
        HostWriteExecutor executor(path);
        const Awaited retry = run(executor, context(receiptFailureId), QStringLiteral("receipt-failure"), mutation);
        if (!check(retry.success && !retry.result.replayed, "retry after atomic rollback succeeds once")) return 1;
    }

    // Exercise the actual protocol registration gate; production registers no mutation operation yet.
    {
        HostMutationProtocolService protocol(path);
        BrickSuiteOperationDispatcher dispatcher;
        protocol.registerInternalOperation(dispatcher, QStringLiteral("test.mutation"),
            QStringLiteral("test.mutation.write"), mutation);
        if (!check(!dispatcher.operations(1).contains(QStringLiteral("test.mutation"))
                   && dispatcher.operations(2).contains(QStringLiteral("test.mutation"))
                   && !dispatcher.capabilities(1).contains(QStringLiteral("test.mutation.write"))
                   && dispatcher.capabilities(2).contains(QStringLiteral("test.mutation.write")),
                   "capability is negotiated only for protocol 1.2")) return 1;

        RemoteMutationDto::Metadata wire{1, RemoteMutationDto::newMutationId(), {}, {}};
        BrickSuiteProtocol::Message request = BrickSuiteProtocol::request(
            QStringLiteral("test.mutation"), {{QStringLiteral("workspaceId"), 1},
                {QStringLiteral("mutationId"), wire.mutationId},
                {QStringLiteral("expected"), QJsonObject{}},
                {QStringLiteral("mutation"), QJsonObject{}}});
        BrickSuiteProtocol::Message response;
        dispatcher.dispatchAsync(request, false, [&](auto value){ response=value; });
        if (!check(response.error.code==QStringLiteral("AUTH_REQUIRED"),
                   "unauthenticated mutation rejected")) return 1;
        request.protocolMinor = 1;
        dispatcher.dispatchAsync(request, true, [&](auto value){ response=value; });
        if (!check(response.error.code==QStringLiteral("FORBIDDEN"),
                   "protocol 1.1 mutation rejected")) return 1;
    }

    // A Host-local exclusive writer causes a bounded, retryable BUSY result.
    const QString blockerName = QStringLiteral("blocker-%1").arg(QUuid::createUuid().toString());
    QSqlDatabase blocker = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), blockerName);
    blocker.setDatabaseName(path); blocker.open();
    QSqlQuery blockerQuery(blocker);
    if (!check(blockerQuery.exec(QStringLiteral("BEGIN EXCLUSIVE")), "acquire Host-local lock")) return 1;
    RemoteMutationDto::Metadata contended{1, RemoteMutationDto::newMutationId(), {}, {}};
    {
        HostWriteExecutor executor(path);
        const Awaited busy = run(executor, context(contended.mutationId),
            RemoteMutationDto::requestHash(QStringLiteral("test.mutation"), contended), mutation);
        if (!check(!busy.success && busy.error.code==QStringLiteral("BUSY") && busy.error.retryable,
                   "lock exhaustion is retryable BUSY")) return 1;
    }
    blockerQuery.exec(QStringLiteral("ROLLBACK")); blocker.close(); blocker={};
    QSqlDatabase::removeDatabase(blockerName);
    {
        HostWriteExecutor executor(path);
        const Awaited retry = run(executor, context(contended.mutationId),
            RemoteMutationDto::requestHash(QStringLiteral("test.mutation"), contended), mutation);
        if (!check(retry.success, "retry succeeds after Host-local lock release")) return 1;
    }

    const QString verifyName = QStringLiteral("verify-%1").arg(QUuid::createUuid().toString());
    QSqlDatabase verify = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), verifyName);
    verify.setDatabaseName(path); verify.open();
    QSqlQuery query(verify);
    if (!check(query.exec(QStringLiteral("SELECT value FROM mutation_probe")) && query.next()
               && query.value(0).toInt()==3, "domain rollback and committed mutations execute once")) return 1;
    query.prepare(QStringLiteral("SELECT COUNT(*) FROM remote_mutation_receipt WHERE mutation_id=:id"));
    query.bindValue(QStringLiteral(":id"), failedMutationId);
    if (!check(query.exec() && query.next() && query.value(0).toInt()==0,
               "failed mutation receipt rolled back")) return 1;
    verify.close(); verify={}; QSqlDatabase::removeDatabase(verifyName);
    std::cout << "Remote mutation foundation tests passed.\n";
    return 0;
}
