#include "HostWriteExecutor.h"

#include "../../repositories/RemoteMutationReceiptRepository.h"

#include <QDateTime>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QMetaObject>
#include <QPointer>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

namespace {
bool isBusyError(const QString& text)
{
    return text.contains(QStringLiteral("locked"), Qt::CaseInsensitive)
           || text.contains(QStringLiteral("busy"), Qt::CaseInsensitive);
}
}

class HostWriteExecutor::Worker : public QObject
{
public:
    Worker(QString path, QString name, std::atomic_int& queued)
        : m_path(std::move(path)), m_name(std::move(name)), m_queued(queued) {}

    void initialize()
    {
        m_database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_name);
        m_database.setDatabaseName(m_path);
        m_database.setConnectOptions(QStringLiteral("QSQLITE_BUSY_TIMEOUT=5000"));
        if (!m_database.open()) { m_error = QStringLiteral("Host write database unavailable."); return; }
        QSqlQuery pragma(m_database);
        if (!pragma.exec(QStringLiteral("PRAGMA foreign_keys=ON"))
            || !pragma.exec(QStringLiteral("PRAGMA busy_timeout=5000")))
            m_error = QStringLiteral("Host write database configuration failed.");
        if (m_error.isEmpty()) {
            RemoteMutationReceiptRepository receipts(m_database);
            receipts.removeCommittedBefore(QDateTime::currentDateTimeUtc().addDays(-ReceiptRetentionDays),
                                           ReceiptCleanupBatch);
        }
    }

    void execute(RemoteMutationDto::RequestContext context, QString hash, Mutation mutation,
                 QObject* callbackContext, Completion completion, Failure failure,
                 Publisher publisher)
    {
        struct CountGuard { std::atomic_int& value; ~CountGuard(){ --value; } } guard{m_queued};
        const QPointer<QObject> guarded(callbackContext);
        auto fail = [guarded, failure](RemoteMutationDto::Error error) {
            if (!failure || !guarded) return;
            QMetaObject::invokeMethod(guarded, [guarded, failure, error] {
                if (guarded) failure(error);
            }, Qt::QueuedConnection);
        };
        if (!m_error.isEmpty()) {
            fail({QStringLiteral("INTERNAL_ERROR"),
                  QStringLiteral("The Host mutation service is unavailable."), true});
            return;
        }
        QElapsedTimer timer; timer.start();
        if (!m_database.transaction()) {
            fail({QStringLiteral("BUSY"),
                  QStringLiteral("The Host database is busy. Try again."), true});
            return;
        }
        RemoteMutationReceiptRepository receipts(m_database);
        QString repositoryError;
        const auto receipt = receipts.find(context.mutationId, &repositoryError);
        if (!repositoryError.isEmpty()) {
            m_database.rollback();
            fail({isBusyError(repositoryError) ? QStringLiteral("BUSY")
                                               : QStringLiteral("INTERNAL_ERROR"),
                  isBusyError(repositoryError)
                      ? QStringLiteral("The Host database is busy. Try again.")
                      : QStringLiteral("The Host could not verify mutation replay state."), true});
            return;
        }
        if (receipt) {
            m_database.rollback();
            if (receipt->operation != context.operation || receipt->workspaceId != context.workspaceId
                || receipt->requestHash != hash) {
                fail({QStringLiteral("IDEMPOTENCY_CONFLICT"),
                      QStringLiteral("The mutation ID was already used for different data."), false});
                return;
            }
            RemoteMutationDto::Result result;
            RemoteMutationDto::Error parseError;
            if (!RemoteMutationDto::resultFromJson(
                    QJsonDocument::fromJson(receipt->resultJson.toUtf8()).object(), &result, &parseError)) {
                fail(parseError); return;
            }
            result.replayed = true;
            if (completion && guarded)
                QMetaObject::invokeMethod(guarded, [guarded, completion, result] {
                    if (guarded) completion(result);
                }, Qt::QueuedConnection);
            return;
        }

        QSqlQuery workspace(m_database);
        workspace.prepare(QStringLiteral("SELECT is_active FROM workspace WHERE id=:id"));
        workspace.bindValue(QStringLiteral(":id"), context.workspaceId);
        if (!workspace.exec()) {
            const bool busy = isBusyError(workspace.lastError().text());
            m_database.rollback();
            fail({busy ? QStringLiteral("BUSY") : QStringLiteral("INTERNAL_ERROR"),
                  busy ? QStringLiteral("The Host database is busy. Try again.")
                       : QStringLiteral("The Host could not validate the Workspace."), true});
            return;
        }
        if (!workspace.next()) {
            m_database.rollback();
            fail({QStringLiteral("WORKSPACE_NOT_FOUND"),
                  QStringLiteral("The selected Workspace no longer exists."), false});
            return;
        }
        if (!workspace.value(0).toBool()) {
            m_database.rollback();
            fail({QStringLiteral("WORKSPACE_INACTIVE"),
                  QStringLiteral("The selected Workspace is inactive."), false});
            return;
        }

        const MutationOutcome outcome = mutation(m_database);
        if (!outcome.success) {
            m_database.rollback();
            RemoteMutationDto::Error error = outcome.error;
            if (error.code.isEmpty())
                error = {QStringLiteral("INTERNAL_ERROR"),
                         QStringLiteral("The Host could not complete the mutation."), false};
            fail(error); return;
        }
        RemoteMutationDto::Result result;
        result.mutationId = context.mutationId;
        result.operation = context.operation;
        result.committedUtc = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
        result.authoritative = outcome.authoritative;
        RemoteMutationReceipt stored{result.mutationId, result.operation, context.workspaceId, hash,
            QStringLiteral("SUCCESS"),
            QString::fromUtf8(QJsonDocument(RemoteMutationDto::resultToJson(result)).toJson(QJsonDocument::Compact)),
            QDateTime::fromString(result.committedUtc, Qt::ISODateWithMs), context.clientIdentity.left(128)};
        const bool inserted = receipts.insert(stored, &repositoryError);
        const bool committed = inserted && m_database.commit();
        if (!committed) {
            const bool busy = isBusyError(repositoryError)
                              || isBusyError(m_database.lastError().text());
            m_database.rollback();
            fail({busy ? QStringLiteral("BUSY") : QStringLiteral("INTERNAL_ERROR"),
                  busy ? QStringLiteral("The Host database is busy. Try again.")
                       : QStringLiteral("The Host could not commit the mutation."), true});
            return;
        }
        qInfo() << "Remote mutation committed" << context.operation << context.workspaceId
                << context.mutationId.left(8);
        qDebug() << "Remote mutation commit timing" << context.operation
                 << timer.elapsed() << "ms";
        if (completion && guarded) {
            const auto workflow=outcome.publicationWorkflow;
            const auto scope=outcome.publicationScope;
            QMetaObject::invokeMethod(guarded,
                [guarded, completion, result, publisher, workflow, scope] {
                    if (!guarded) return;
                    // Complete the correlated protocol response synchronously on
                    // the application thread before publishing any invalidation.
                    // A refresh caused by that invalidation must never delay or
                    // supersede the authoritative response for its own mutation.
                    completion(result);
                    if (publisher) publisher(workflow,scope);
                }, Qt::QueuedConnection);
        } else if (publisher) {
            // The originating UI/transport context may have gone away after the
            // commit. Other connected clients must still be invalidated.
            publisher(outcome.publicationWorkflow,outcome.publicationScope);
        }
    }

    void close()
    {
        if (m_database.isValid()) m_database.close();
        m_database = {};
        QSqlDatabase::removeDatabase(m_name);
    }

private:
    QString m_path, m_name, m_error;
    QSqlDatabase m_database;
    std::atomic_int& m_queued;
};

HostWriteExecutor::HostWriteExecutor(const QString& path, Publisher publisher, QObject* parent)
    : QObject(parent), m_publisher(std::move(publisher)),
      m_connectionName(QStringLiteral("host-write-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces)))
{
    m_worker = new Worker(path, m_connectionName, m_queued);
    m_worker->moveToThread(&m_thread);
    m_thread.start();
    QMetaObject::invokeMethod(m_worker, [this]{ m_worker->initialize(); }, Qt::BlockingQueuedConnection);
}

HostWriteExecutor::~HostWriteExecutor() { shutdown(); }
bool HostWriteExecutor::isAccepting() const { return m_accepting && m_thread.isRunning(); }
int HostWriteExecutor::queuedMutationCount() const { return m_queued.load(); }
QString HostWriteExecutor::connectionName() const { return m_connectionName; }

void HostWriteExecutor::enqueue(const RemoteMutationDto::RequestContext& context,
                                const QString& hash, Mutation mutation, QObject* callbackContext,
                                Completion completion, Failure failure)
{
    if (!isAccepting()) {
        if (failure && callbackContext)
            QMetaObject::invokeMethod(callbackContext, [failure]{ failure({QStringLiteral("BUSY"),
                QStringLiteral("The Host mutation service is stopping."), true}); }, Qt::QueuedConnection);
        return;
    }
    if (m_queued.fetch_add(1) >= MaximumQueuedMutations) {
        --m_queued;
        if (failure && callbackContext)
            QMetaObject::invokeMethod(callbackContext, [failure]{ failure({QStringLiteral("BUSY"),
                QStringLiteral("The Host mutation queue is full. Try again."), true}); }, Qt::QueuedConnection);
        return;
    }
    QMetaObject::invokeMethod(m_worker, [=, mutation=std::move(mutation)]() mutable {
        m_worker->execute(context, hash, std::move(mutation), callbackContext,
                          completion, failure, m_publisher);
    }, Qt::QueuedConnection);
}

void HostWriteExecutor::shutdown()
{
    if (!m_accepting.exchange(false)) return;
    if (m_worker && m_thread.isRunning())
        QMetaObject::invokeMethod(m_worker, [this]{ m_worker->close(); }, Qt::BlockingQueuedConnection);
    m_thread.quit(); m_thread.wait(); delete m_worker; m_worker = nullptr;
}
