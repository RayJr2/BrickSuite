#include "../src/database/DatabaseSchema.h"
#include "../src/services/application/HostWriteExecutor.h"
#include "../src/services/application/HostMutationProtocolService.h"
#include "../src/services/application/dto/RemoteMutationDtos.h"
#include "../src/network/BrickSuiteOperationDispatcher.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QEventLoop>
#include <QFileInfo>
#include <QJsonObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSemaphore>
#include <QStringList>
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

    // Startup cleanup catches up across several bounded batches while keeping
    // the complete 90-day replay window intact.
    {
        const QString cleanupConnection = QStringLiteral("receipt-cleanup-seed");
        QSqlDatabase cleanupDb = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), cleanupConnection);
        cleanupDb.setDatabaseName(path);
        if (!check(cleanupDb.open(), "open receipt cleanup seed database")) return 1;
        QSqlQuery insert(cleanupDb);
        insert.prepare(QStringLiteral("INSERT INTO remote_mutation_receipt "
            "(mutation_id,operation,workspace_id,request_hash,result_code,result_json,committed_utc,client_identity) "
            "VALUES(:id,'test.mutation',1,'hash','SUCCESS','{}',:utc,'test')"));
        const QString expired = QDateTime::currentDateTimeUtc().addDays(-91).toString(Qt::ISODateWithMs);
        for (int i = 0; i < 601; ++i) {
            insert.bindValue(QStringLiteral(":id"), QStringLiteral("expired-%1").arg(i));
            insert.bindValue(QStringLiteral(":utc"), expired);
            if (!check(insert.exec(), "seed expired receipt")) return 1;
        }
        insert.bindValue(QStringLiteral(":id"), QStringLiteral("retained-boundary"));
        insert.bindValue(QStringLiteral(":utc"),
                         QDateTime::currentDateTimeUtc().addDays(-90).addSecs(2).toString(Qt::ISODateWithMs));
        if (!check(insert.exec(), "seed retained receipt")) return 1;
        cleanupDb.close(); cleanupDb = {}; QSqlDatabase::removeDatabase(cleanupConnection);
        { HostWriteExecutor cleanupExecutor(path); }
        QSqlDatabase verifyCleanup = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), cleanupConnection);
        verifyCleanup.setDatabaseName(path);
        QSqlQuery verifyQuery(verifyCleanup);
        if (!check(verifyCleanup.open()
                   && verifyQuery.exec(QStringLiteral("SELECT mutation_id FROM remote_mutation_receipt"))
                   && verifyQuery.next()
                   && verifyQuery.value(0).toString() == QStringLiteral("retained-boundary")
                   && !verifyQuery.next(),
                   "cleanup removes more than 250 expired receipts and preserves retention window")) return 1;
        verifyCleanup.close(); verifyCleanup = {};
        QSqlDatabase::removeDatabase(cleanupConnection);
    }

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

    const QJsonObject validResult{{"mutationId",first.mutationId},{"operation","test.mutation"},
        {"replayed",false},{"committedUtc","2026-09-12T12:34:56.000Z"},
        {"authoritative",QJsonObject{{"value",1}}}};
    RemoteMutationDto::Result decoded; RemoteMutationDto::Error decodeError;
    if (!check(RemoteMutationDto::resultFromJson(validResult,&decoded,&decodeError)
               && !decoded.replayed, "strict normal success decoding")) return 1;
    QJsonObject replay=validResult; replay["replayed"]=true;
    if (!check(RemoteMutationDto::resultFromJson(replay,&decoded,&decodeError)
               && decoded.replayed, "strict replay decoding")) return 1;
    auto rejected=[&](QJsonObject value,const char*message){
        RemoteMutationDto::Result ignored; return check(!RemoteMutationDto::resultFromJson(value,&ignored),message);};
    QJsonObject malformed=validResult; malformed["mutationId"]="not-a-uuid";
    if(!rejected(malformed,"malformed result mutation ID rejected"))return 1;
    malformed=validResult;malformed["replayed"]="false";
    if(!rejected(malformed,"non-Boolean replayed rejected"))return 1;
    malformed=validResult;malformed["committedUtc"]=42;
    if(!rejected(malformed,"non-string commit timestamp rejected"))return 1;
    malformed=validResult;malformed["committedUtc"]="not-a-time";
    if(!rejected(malformed,"invalid commit timestamp rejected"))return 1;
    malformed=validResult;malformed.remove("authoritative");
    if(!rejected(malformed,"missing authoritative result rejected"))return 1;
    malformed=validResult;malformed["unexpected"]=true;
    if(!rejected(malformed,"unexpected result field rejected"))return 1;

    int publications = 0;
    QStringList deliveryOrder;
    QString failedMutationId;
    auto mutation = [](const QSqlDatabase& db) {
        QSqlQuery query(db); query.exec(QStringLiteral("UPDATE mutation_probe SET value=value+1"));
        HostWriteExecutor::MutationOutcome out; out.success=query.numRowsAffected()==1;
        out.authoritative={{QStringLiteral("value"), 1}}; return out;
    };
    const QString hash = RemoteMutationDto::requestHash(QStringLiteral("test.mutation"), first);
    {
        HostWriteExecutor executor(path, [&](auto, const auto&){
            QMetaObject::invokeMethod(&app, [&]{ ++publications; deliveryOrder.append("invalidation"); },
                                      Qt::QueuedConnection);
        });
        Awaited committed; QEventLoop completionLoop;
        executor.enqueue(context(first.mutationId), hash, mutation, &app,
            [&](const auto& result){committed.success=true;committed.result=result;
                deliveryOrder.append("response");QTimer::singleShot(0,&completionLoop,&QEventLoop::quit);},
            [&](const auto& error){committed.error=error;completionLoop.quit();});
        QTimer::singleShot(20000,&completionLoop,&QEventLoop::quit);completionLoop.exec();
        QCoreApplication::processEvents();
        if (!check(committed.success && !committed.result.replayed, "first mutation commits")
            || !check(deliveryOrder == QStringList{"response","invalidation"},
                      "authoritative response is delivered before invalidation")) return 1;
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

    // A committed mutation remains replayable when its response context is
    // destroyed before delivery (for example, a socket disconnect).
    const QString lostResponseMutationId = RemoteMutationDto::newMutationId();
    RemoteMutationDto::Metadata lostResponseMetadata{
        1, lostResponseMutationId, {}, {{QStringLiteral("response"), QStringLiteral("lost")}}};
    const QString lostResponseHash = RemoteMutationDto::requestHash(
        QStringLiteral("test.mutation"), lostResponseMetadata);
    {
        HostWriteExecutor executor(path);
        QSemaphore entered;
        QSemaphore release;
        auto* responseContext = new QObject;
        executor.enqueue(context(lostResponseMutationId), lostResponseHash,
            [&, mutation](const QSqlDatabase& db) {
                entered.release();
                release.acquire();
                return mutation(db);
            }, responseContext, [](const auto&) {}, [](const auto&) {});
        entered.acquire();
        delete responseContext;
        release.release();
        QEventLoop drain;
        QTimer poll;
        QObject::connect(&poll, &QTimer::timeout, &drain, [&] {
            if (executor.isIdle()) drain.quit();
        });
        poll.start(5);
        QTimer::singleShot(10000, &drain, &QEventLoop::quit);
        drain.exec();
        if (!check(executor.isIdle(), "mutation commits after response context is lost")) return 1;
    }
    {
        HostWriteExecutor restarted(path);
        const Awaited replayed = run(restarted, context(lostResponseMutationId),
                                     lostResponseHash, mutation);
        if (!check(replayed.success && replayed.result.replayed,
                   "lost mutation response is recovered from persisted receipt")) return 1;
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
        if (!check(queued.activeMutationCount() == 1 && !queued.isIdle(),
                   "active mutation is exposed for maintenance drain")) return 1;
        queued.stopAccepting();
        if (!check(!queued.isAccepting(), "write admission can stop while active work drains")) return 1;
        queued.startAccepting();
        if (!check(queued.isAccepting(), "write admission can resume")) return 1;
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
            if (queued.isIdle() && overflowBusy) drain.quit();
        });
        poll.start(5); QTimer::singleShot(10000, &drain, &QEventLoop::quit); drain.exec();
        if (!check(overflowBusy && queued.isIdle(), "queue overflow returns BUSY and executor drains")) return 1;
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

    QString pairedMutationId;
    QString pairedReconnectMutationId;
    QString otherDeviceMutationId;
    QString legacyMutationId;
    // Exercise the actual protocol registration gate and trusted Host attribution.
    {
        const QString hostEpoch = QStringLiteral("11111111-1111-4111-8111-111111111111");
        HostMutationProtocolService protocol(path, {}, hostEpoch);
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
        request.protocolMinor = 2;
        request.payload.insert(QStringLiteral("dataEpoch"),
                               QStringLiteral("22222222-2222-4222-8222-222222222222"));
        dispatcher.dispatchAsync(request, true, [&](auto value){ response=value; });
        if (!check(response.error.code == QStringLiteral("STALE_DATA_EPOCH")
                       && !response.error.retryable,
                   "stale data epoch is rejected before mutation execution")) return 1;

        auto dispatchMutation = [&](const HostRequestContext& hostContext,
                                    const QString& mutationId,
                                    BrickSuiteProtocol::Message* result) {
            BrickSuiteProtocol::Message attributedRequest = BrickSuiteProtocol::request(
                QStringLiteral("test.mutation"), {{QStringLiteral("workspaceId"), 1},
                    {QStringLiteral("mutationId"), mutationId},
                    {QStringLiteral("expected"), QJsonObject{}},
                    {QStringLiteral("mutation"), QJsonObject{}}});
            attributedRequest.protocolMinor = hostContext.protocolMinor;
            QEventLoop loop;
            dispatcher.dispatchAsync(attributedRequest, true, hostContext,
                [&](auto value) { *result = value; loop.quit(); });
            QTimer::singleShot(20000, &loop, &QEventLoop::quit);
            loop.exec();
            return result->error.code.isEmpty();
        };

        const QString firstDevice = QStringLiteral("AAAAAAAA-AAAA-4AAA-8AAA-AAAAAAAAAAAA");
        const QString secondDevice = QStringLiteral("bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbbb");
        pairedMutationId = RemoteMutationDto::newMutationId();
        HostRequestContext pairedContext{QStringLiteral("paired-session-1"), {}, 3,
            HostRequestContext::AuthenticationKind::PairedDevice, firstDevice};
        BrickSuiteProtocol::Message spoofedRequest = BrickSuiteProtocol::request(
            QStringLiteral("test.mutation"), {{QStringLiteral("workspaceId"), 1},
                {QStringLiteral("mutationId"), RemoteMutationDto::newMutationId()},
                {QStringLiteral("expected"), QJsonObject{}},
                {QStringLiteral("mutation"), QJsonObject{}},
                {QStringLiteral("clientIdentity"), QStringLiteral("PairedDevice:spoofed")}});
        spoofedRequest.protocolMinor = 3;
        dispatcher.dispatchAsync(spoofedRequest, true, pairedContext,
                                 [&](auto value) { response = value; });
        if (!check(response.error.code == QStringLiteral("INVALID_ARGUMENT"),
                   "client-supplied receipt identity is rejected")) return 1;
        if (!check(dispatchMutation(pairedContext, pairedMutationId, &response),
                   "Protocol 1.3 paired mutation succeeds")) return 1;

        pairedReconnectMutationId = RemoteMutationDto::newMutationId();
        pairedContext.sessionId = QStringLiteral("paired-session-2");
        if (!check(dispatchMutation(pairedContext, pairedReconnectMutationId, &response),
                   "same paired device mutation after reconnect succeeds")) return 1;

        otherDeviceMutationId = RemoteMutationDto::newMutationId();
        HostRequestContext otherDevice{QStringLiteral("paired-session-3"), {}, 3,
            HostRequestContext::AuthenticationKind::PairedDevice, secondDevice};
        if (!check(dispatchMutation(otherDevice, otherDeviceMutationId, &response),
                   "different paired device mutation succeeds")) return 1;

        legacyMutationId = RemoteMutationDto::newMutationId();
        HostRequestContext legacyContext{QStringLiteral("legacy-session"), {}, 2,
            HostRequestContext::AuthenticationKind::LegacySharedToken, {}};
        if (!check(dispatchMutation(legacyContext, legacyMutationId, &response),
                   "Protocol 1.2 legacy mutation succeeds")) return 1;

        if (!check(dispatchMutation(pairedContext, pairedMutationId, &response)
                       && response.payload.value(QStringLiteral("replayed")).toBool(),
                   "receipt replay remains independent of reconnecting session")) return 1;
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
               && query.value(0).toInt()==8, "domain rollback and committed mutations execute once")) return 1;
    query.prepare(QStringLiteral("SELECT COUNT(*) FROM remote_mutation_receipt WHERE mutation_id=:id"));
    query.bindValue(QStringLiteral(":id"), failedMutationId);
    if (!check(query.exec() && query.next() && query.value(0).toInt()==0,
               "failed mutation receipt rolled back")) return 1;
    query.prepare(QStringLiteral("SELECT client_identity FROM remote_mutation_receipt "
                                 "WHERE mutation_id=:id"));
    auto receiptIdentity = [&](const QString& mutationId) {
        query.bindValue(QStringLiteral(":id"), mutationId);
        return query.exec() && query.next() ? query.value(0).toString() : QString();
    };
    const QString firstIdentity = QStringLiteral("PairedDevice:aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa");
    if (!check(receiptIdentity(pairedMutationId) == firstIdentity,
               "Protocol 1.3 receipt uses normalized trusted paired-device identity")
        || !check(receiptIdentity(pairedReconnectMutationId) == firstIdentity,
                  "same paired device attribution is stable across sessions")
        || !check(receiptIdentity(otherDeviceMutationId)
                      == QStringLiteral("PairedDevice:bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbbb"),
                  "different paired device has distinct attribution")
        || !check(receiptIdentity(legacyMutationId) == QStringLiteral("LegacySharedToken"),
                  "Protocol 1.2 receipt uses explicit legacy attribution")) return 1;
    verify.close(); verify={}; QSqlDatabase::removeDatabase(verifyName);
    std::cout << "Remote mutation foundation tests passed.\n";
    return 0;
}
