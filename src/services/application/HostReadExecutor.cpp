#include "HostReadExecutor.h"
#include "../../network/HostRequestContext.h"

#include "../../repositories/StorageLocationRepository.h"
#include "../../repositories/StorageLocationTypeRepository.h"
#include "../../repositories/PartRepository.h"
#include "../../repositories/ColorRepository.h"
#include "../../repositories/PartCategoryRepository.h"
#include "../../repositories/LostInventoryRepository.h"
#include "../../repositories/BuildRequirementRepository.h"
#include "../../repositories/BuildAllocationRepository.h"
#include "../../repositories/ManufacturerRepository.h"
#include "../../repositories/InventoryBuildabilityRepository.h"
#include "../../repositories/SetCatalogRepository.h"
#include "../parts/PartReferenceManifest.h"
#include "../builds/BuildRequirementAvailabilityService.h"
#include "../builds/BuildLifecycleService.h"

#include <QElapsedTimer>
#include <QHash>
#include <QDateTime>
#include <QPointer>
#include <QMutexLocker>
#include <QSqlError>
#include <QSqlQuery>
#include <QSet>
#include <QTimer>
#include <QStringList>
#include <QUuid>
#include <algorithm>

namespace {
RemoteBuildabilityDto::CompactResult portableBuildability(
    const InventoryBuildabilitySetResult& value)
{
    return {value.setNumber, value.name, value.year, value.rebrickableThemeId,
        value.themeName, value.imageUrl, value.catalogPartCount, value.totalQuantity,
        value.totalRequirements, value.looseSatisfiedQuantity,
        value.looseSatisfiedRequirements, value.advisorySatisfiedQuantity,
        value.advisorySatisfiedRequirements, value.missingQuantity,
        value.usesCollection(), int(value.sources.size())};
}

int activeThemeCatalogId(const QSqlDatabase& db, int externalId, bool* queryOk)
{
    if (queryOk) *queryOk = false;
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "SELECT tc.id FROM theme_external_identifier tei "
        "JOIN theme_catalog tc ON tc.id=tei.theme_catalog_id "
        "WHERE tei.provider='Rebrickable' AND tei.external_id=:id "
        "AND tei.is_active=1 AND tc.is_active=1"));
    query.bindValue(QStringLiteral(":id"), QString::number(externalId));
    if (!query.exec()) return 0;
    if (queryOk) *queryOk = true;
    return query.next() ? query.value(0).toInt() : 0;
}

bool activeWorkspaceExists(const QSqlDatabase& db, qint64 workspaceId, bool* queryOk)
{
    QSqlQuery query(db);
    query.prepare(QStringLiteral("SELECT 1 FROM workspace WHERE id=:id AND is_active=1"));
    query.bindValue(QStringLiteral(":id"), workspaceId);
    const bool executed = query.exec();
    if (queryOk) *queryOk = executed;
    return executed && query.next();
}
}

struct HostReadExecutor::PendingRead
{
    QString label;
    QString sessionId;
    Task task;
    QPointer<QObject> context;
    ErrorCallback failure;
    qint64 queuedAt = 0;
    bool queued = true;
};

class HostReadExecutor::Worker : public QObject
{
public:
    Worker(QString path, QString name, std::atomic_int& queued,
           std::atomic_int& active, HostReadExecutor* owner)
        : m_path(std::move(path)), m_name(std::move(name)), m_queued(queued),
          m_active(active), m_owner(owner) {}

    void initialize()
    {
        Q_ASSERT(QThread::currentThread() == thread());
        m_database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_name);
        m_database.setDatabaseName(m_path);
        m_database.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY;QSQLITE_BUSY_TIMEOUT=5000"));
        if (!m_database.open()) {
            m_error = QStringLiteral("Unable to open the Host read database: %1")
                          .arg(m_database.lastError().text());
            return;
        }
        QSqlQuery pragma(m_database);
        if (!pragma.exec(QStringLiteral("PRAGMA foreign_keys = ON"))
            || !pragma.exec(QStringLiteral("PRAGMA busy_timeout = 5000"))
            || !pragma.exec(QStringLiteral("PRAGMA query_only = ON"))) {
            m_error = QStringLiteral("Unable to configure the Host read database connection.");
            return;
        }
        m_services = createConnectionBoundReadApplicationServices(m_database);
    }

    void execute(const std::shared_ptr<PendingRead>& read)
    {
        Q_ASSERT(QThread::currentThread() == thread());
        if (!m_owner->beginRead(read)) return;
        const int remaining = m_queued.load();
        struct ActiveGuard {
            HostReadExecutor* owner;
            std::shared_ptr<PendingRead> read;
            ~ActiveGuard() { owner->finishRead(read); }
        } activeGuard{m_owner, read};
        if (!m_services) {
            deliverFailure(read->context, read->failure, m_error.isEmpty()
                ? QStringLiteral("The Host read executor is unavailable.") : m_error);
            return;
        }
        QElapsedTimer timer;
        timer.start();
        read->task(*m_services, m_database);
        qDebug().noquote() << "Host read" << read->label
                           << "queueWaitMs=" << qMax<qint64>(0, timerReference() - read->queuedAt)
                           << "executionMs=" << timer.elapsed()
                           << "queued=" << remaining;
    }

    void close()
    {
        Q_ASSERT(QThread::currentThread() == thread());
        m_services.reset();
        if (m_database.isValid()) m_database.close();
        m_database = QSqlDatabase();
        if (QSqlDatabase::contains(m_name))
            QSqlDatabase::removeDatabase(m_name);
    }

    static qint64 timerReference()
    {
        return QDateTime::currentMSecsSinceEpoch();
    }

    void notifyActivity()
    {
        QPointer<HostReadExecutor> owner(m_owner);
        QMetaObject::invokeMethod(m_owner, [owner] {
            if (!owner) return;
            emit owner->activityChanged();
            if (owner->isIdle()) emit owner->drained();
        }, Qt::QueuedConnection);
    }

private:
    QString m_path;
    QString m_name;
    QString m_error;
    QSqlDatabase m_database;
    std::unique_ptr<ApplicationServices> m_services;
    std::atomic_int& m_queued;
    std::atomic_int& m_active;
    HostReadExecutor* m_owner = nullptr;
};

HostReadExecutor::HostReadExecutor(const QString& databasePath, QObject* parent)
    : QObject(parent)
    , m_connectionName(QStringLiteral("BrickSuite_HostRead_%1")
          .arg(QUuid::createUuid().toString(QUuid::WithoutBraces)))
{
    m_thread.setObjectName(QStringLiteral("BrickSuite Host Read Worker"));
    m_worker = new Worker(databasePath, m_connectionName, m_queued, m_active, this);
    m_worker->moveToThread(&m_thread);
    connect(&m_thread, &QThread::started, m_worker, [worker = m_worker]() {
        worker->initialize();
    });
    m_thread.start();
}

HostReadExecutor::~HostReadExecutor() { shutdown(); }

QString HostReadExecutor::connectionName() const { return m_connectionName; }
bool HostReadExecutor::isAccepting() const { return m_accepting && m_thread.isRunning(); }
int HostReadExecutor::queuedReadCount() const { return m_queued.load(); }
int HostReadExecutor::activeReadCount() const { return m_active.load(); }
bool HostReadExecutor::isIdle() const
{ return queuedReadCount() == 0 && activeReadCount() == 0; }
void HostReadExecutor::stopAccepting() { m_accepting = false; }
void HostReadExecutor::startAccepting()
{ if (m_thread.isRunning() && m_worker) m_accepting = true; }

void HostReadExecutor::closeConnectionAsync(
    std::function<void(bool, const QString&)> completion)
{
    stopAccepting();
    if (!isIdle() || !m_worker || !m_thread.isRunning()) {
        const QString error = !isIdle()
            ? QStringLiteral("Host read operations have not drained.") : QString();
        QTimer::singleShot(0, this, [this, completion = std::move(completion), error] {
            emit connectionClosed(error.isEmpty(), error);
            if (completion) completion(error.isEmpty(), error);
        });
        return;
    }
    QPointer<HostReadExecutor> guard(this);
    QMetaObject::invokeMethod(m_worker, [this, guard, completion = std::move(completion)]() mutable {
        m_worker->close();
        QMetaObject::invokeMethod(this, [this, guard, completion = std::move(completion)]() mutable {
            if (!guard) return;
            emit connectionClosed(true, {});
            if (completion) completion(true, {});
        }, Qt::QueuedConnection);
    }, Qt::QueuedConnection);
}

void HostReadExecutor::deliverFailure(QObject* context, const ErrorCallback& failure,
                                      const QString& message)
{
    if (!failure || !context) return;
    QPointer<QObject> guard(context);
    QMetaObject::invokeMethod(context, [guard, failure, message]() {
        if (guard) failure(message);
    }, Qt::QueuedConnection);
}

void HostReadExecutor::enqueue(const QString& label, Task task, QObject* context,
                               ErrorCallback failure)
{
    if (!context || !isAccepting()) {
        deliverFailure(context, failure, QStringLiteral("The Host read executor is shutting down."));
        return;
    }
    int expected = m_queued.load();
    do {
        if (expected + m_active.load() >= MaximumQueuedReads) {
            deliverFailure(context, failure, QStringLiteral("The Host read queue is full."));
            return;
        }
    } while (!m_queued.compare_exchange_weak(expected, expected + 1));
    emit activityChanged();
    auto read = std::make_shared<PendingRead>();
    read->label = label;
    read->task = std::move(task);
    read->context = context;
    read->failure = std::move(failure);
    read->queuedAt = QDateTime::currentMSecsSinceEpoch();
    if (const auto* request = HostRequestContext::current())
        read->sessionId = request->sessionId;
    {
        QMutexLocker lock(&m_pendingMutex);
        m_pendingReads.append(read);
    }
    QMetaObject::invokeMethod(m_worker,
        [worker = m_worker, read] { worker->execute(read); }, Qt::QueuedConnection);
}

bool HostReadExecutor::beginRead(const std::shared_ptr<PendingRead>& read)
{
    QMutexLocker lock(&m_pendingMutex);
    if (!read->queued) return false;
    read->queued = false;
    m_pendingReads.removeOne(read);
    --m_queued;
    ++m_active;
    lock.unlock();
    emit activityChanged();
    return true;
}

void HostReadExecutor::finishRead(const std::shared_ptr<PendingRead>& read)
{
    Q_UNUSED(read);
    --m_active;
    emit activityChanged();
    if (isIdle()) emit drained();
}

int HostReadExecutor::cancelQueuedReadsForSession(const QString& sessionId)
{
    if (sessionId.isEmpty()) return 0;
    int cancelled = 0;
    QMutexLocker lock(&m_pendingMutex);
    for (auto it = m_pendingReads.begin(); it != m_pendingReads.end();) {
        const auto& read = *it;
        if (read->queued && read->sessionId == sessionId) {
            read->queued = false;
            read->task = {};
            read->failure = {};
            read->context.clear();
            it = m_pendingReads.erase(it);
            --m_queued;
            ++cancelled;
        } else ++it;
    }
    m_cancelled += cancelled;
    lock.unlock();
    if (cancelled) {
        qInfo() << "BrickSuite Host cancelled queued reads for disconnected session:" << cancelled;
        emit activityChanged();
        if (isIdle()) emit drained();
    }
    return cancelled;
}

int HostReadExecutor::cancelAllQueuedReads()
{
    int cancelled = 0;
    QMutexLocker lock(&m_pendingMutex);
    for (const auto& read : std::as_const(m_pendingReads)) {
        if (!read->queued) continue;
        read->queued = false;
        read->task = {};
        read->failure = {};
        read->context.clear();
        --m_queued;
        ++cancelled;
    }
    m_pendingReads.clear();
    m_cancelled += cancelled;
    lock.unlock();
    if (cancelled) emit activityChanged();
    if (isIdle()) emit drained();
    return cancelled;
}

#ifdef BRICKSUITE_TESTING
void HostReadExecutor::enqueueForTesting(const QString& sessionId, const QString& label,
                                         std::function<void()> task, QObject* context,
                                         ErrorCallback failure)
{
    HostRequestContext request;
    request.sessionId = sessionId;
    HostRequestContextScope scope(request);
    enqueue(label, [task = std::move(task)](ApplicationServices&, const QSqlDatabase&) {
        task();
    }, context, std::move(failure));
}
#endif

void HostReadExecutor::shutdown()
{
    m_accepting = false;
    const int cancelled = cancelAllQueuedReads();
    QElapsedTimer timer;
    timer.start();
    qInfo() << "Host read executor shutdown started; queued" << queuedReadCount()
            << "active" << activeReadCount() << "cancelled" << cancelled;
    if (m_thread.isRunning()) {
        QMetaObject::invokeMethod(m_worker, [worker = m_worker]() { worker->close(); },
                                  Qt::BlockingQueuedConnection);
        m_thread.quit();
        if (!m_thread.wait(30000)) {
            qWarning() << "Host read executor shutdown exceeded 30000 ms; waiting safely for the active read.";
            m_thread.wait();
        }
    }
    qInfo() << "Host read executor stopped cleanly in" << timer.elapsed() << "ms.";
    delete m_worker;
    m_worker = nullptr;
}

#define HOST_READ_METHOD_BODY(label, expression, completionType) \
    QPointer<QObject> guard(context); \
    enqueue(QStringLiteral(label), [=, completion = std::move(completion)] \
        (ApplicationServices& services, const QSqlDatabase& database) mutable { \
        Q_UNUSED(database); auto result = (expression); \
        if (guard) QMetaObject::invokeMethod(guard, [guard, completion, result = std::move(result)]() mutable { \
            if (guard) completion(result); }, Qt::QueuedConnection); \
    }, context, std::move(failure))

void HostReadExecutor::listWorkspaces(QObject* context,
    std::function<void(const QList<Workspace>&)> completion, ErrorCallback failure)
{ HOST_READ_METHOD_BODY("workspace.list", services.workspaces().list(), QList<Workspace>); }
void HostReadExecutor::listManufacturerNames(QObject* context,
    std::function<void(const QStringList&)> completion, ErrorCallback failure)
{
    QPointer<QObject> guard(context);
    enqueue(QStringLiteral("manufacturers.list"),
        [guard, completion=std::move(completion)](ApplicationServices&, const QSqlDatabase& database) mutable {
            QStringList result;
            for (const Manufacturer& manufacturer : ManufacturerRepository(database).getAll(true))
                result.append(manufacturer.name());
            if (guard) QMetaObject::invokeMethod(guard, [guard, completion, result=std::move(result)]() mutable {
                if (guard) completion(result);
            }, Qt::QueuedConnection);
        }, context, std::move(failure));
}
void HostReadExecutor::getWorkspace(int id, QObject* context,
    std::function<void(const std::optional<Workspace>&)> completion, ErrorCallback failure)
{ HOST_READ_METHOD_BODY("workspace.get", services.workspaces().get(id), std::optional<Workspace>); }
void HostReadExecutor::listStorage(int workspaceId, QObject* context,
    std::function<void(const QList<StorageLocation>&)> completion, ErrorCallback failure)
{ HOST_READ_METHOD_BODY("storage.list", StorageLocationRepository(database).getByWorkspace(workspaceId), QList<StorageLocation>); }
void HostReadExecutor::listStoragePortable(int workspaceId, bool includeInactive, QObject* context,
    std::function<void(const std::optional<QList<RemoteReadDto::StorageSummary>>&)> completion,
    ErrorCallback failure)
{
    QPointer<QObject> guard(context);
    enqueue(QStringLiteral("storage.list"), [=, completion=std::move(completion)]
        (ApplicationServices& services, const QSqlDatabase& database) mutable {
        if (!services.workspaces().exists(workspaceId)) {
            if (guard) QMetaObject::invokeMethod(guard, [guard,completion]() { if(guard) completion(std::nullopt); }, Qt::QueuedConnection);
            return;
        }
        StorageLocationRepository locations(database); StorageLocationTypeRepository types(database);
        auto rows = locations.getByWorkspaceIncludingInactive(workspaceId);
        if (!includeInactive)
            rows.erase(std::remove_if(rows.begin(), rows.end(),
                [](const StorageLocation& row) { return !row.isActive(); }), rows.end());
        QHash<int,QString> typeNames; for (const auto& type : types.getAll()) typeNames.insert(type.id(),type.name());
        QHash<int,StorageLocation> byId; for(const auto& row:rows) byId.insert(row.id(),row);
        QList<RemoteReadDto::StorageSummary> result;
        for(const auto& row:rows){QStringList names;QSet<int>seen;int id=row.id();while(id>0&&byId.contains(id)&&!seen.contains(id)){seen.insert(id);names.prepend(byId[id].name());id=byId[id].parentLocationId();}
            result.append({row.id(),row.parentLocationId(),row.name(),names.join(QStringLiteral(" / ")),typeNames.value(row.locationTypeId()),row.sortOrder(),row.isActive(),row.allowsInventory(),row.allowsCollection()});}
        if(guard)QMetaObject::invokeMethod(guard,[guard,completion,result=std::move(result)]()mutable{if(guard)completion(result);},Qt::QueuedConnection);
    }, context, std::move(failure));
}
void HostReadExecutor::getStoragePortable(int workspaceId,int storageId,QObject* context,
    std::function<void(const std::optional<RemoteReadDto::StorageDetail>&)> completion,ErrorCallback failure)
{
    QPointer<QObject> guard(context);
    enqueue(QStringLiteral("storage.get"),[=,completion=std::move(completion)](ApplicationServices&,const QSqlDatabase& database) mutable {
        StorageLocationRepository locations(database); StorageLocationTypeRepository types(database);
        std::optional<StorageLocation> row; std::optional<RemoteReadDto::StorageDetail> result;
        if(locations.tryGetById(storageId,row)&&row&&row->workspaceId()==workspaceId){
            const auto type=types.getById(row->locationTypeId()); QStringList names; QSet<int> seen; int current=row->id(); bool valid=true;
            while(current>0){if(seen.contains(current)||seen.size()>=10000){valid=false;break;}seen.insert(current);std::optional<StorageLocation> part;if(!locations.tryGetById(current,part)||!part||part->workspaceId()!=workspaceId){valid=false;break;}names.prepend(part->name());current=part->parentLocationId();}
            if(valid&&type){RemoteReadDto::StorageDetail v;v.storageId=row->id();v.parentStorageId=row->parentLocationId();v.name=row->name();v.displayPath=names.join(QStringLiteral(" / "));v.typeName=type->name();v.sortOrder=row->sortOrder();v.active=row->isActive();v.allowsInventory=row->allowsInventory();v.allowsCollection=row->allowsCollection();v.workspaceId=workspaceId;v.storageTypeId=row->locationTypeId();v.description=row->description();v.createdUtc=row->createdUtc();v.modifiedUtc=row->modifiedUtc();result=v;}
        }
        if(guard)QMetaObject::invokeMethod(guard,[guard,completion,result=std::move(result)]()mutable{if(guard)completion(result);},Qt::QueuedConnection);
    },context,std::move(failure));
}
void HostReadExecutor::listStorageTypesPortable(QObject* context,
    std::function<void(const QList<RemoteReadDto::StorageType>&)> completion,ErrorCallback failure)
{
    QPointer<QObject> guard(context);
    enqueue(QStringLiteral("storage.types.list"),[=,completion=std::move(completion)](ApplicationServices&,const QSqlDatabase& database) mutable {
        QList<RemoteReadDto::StorageType> result; for(const auto&type:StorageLocationTypeRepository(database).getAll())if(type.isActive())result.append({type.id(),type.name(),type.description(),true});
        if(guard)QMetaObject::invokeMethod(guard,[guard,completion,result=std::move(result)]()mutable{if(guard)completion(result);},Qt::QueuedConnection);
    },context,std::move(failure));
}
void HostReadExecutor::searchInventory(const InventorySearchCriteria& criteria, QObject* context,
    std::function<void(const InventoryApplicationService::Page&)> completion, ErrorCallback failure)
{ HOST_READ_METHOD_BODY("inventory.search", services.inventory().search(criteria), InventoryApplicationService::Page); }
void HostReadExecutor::getInventory(int id, QObject* context,
    std::function<void(const std::optional<InventoryRecord>&)> completion, ErrorCallback failure)
{ HOST_READ_METHOD_BODY("inventory.get", services.inventory().get(id), std::optional<InventoryRecord>); }
void HostReadExecutor::inventoryHistory(int workspaceId, int partId, int colorId, QObject* context,
    std::function<void(const QList<InventoryHistoryResult>&)> completion, ErrorCallback failure)
{ HOST_READ_METHOD_BODY("inventory.history", services.inventory().history(workspaceId, partId, colorId), QList<InventoryHistoryResult>); }
void HostReadExecutor::listBuilds(int workspaceId, bool archived, QObject* context,
    std::function<void(const QList<Build>&)> completion, ErrorCallback failure)
{ HOST_READ_METHOD_BODY("builds.list", services.builds().list(workspaceId, archived), QList<Build>); }
void HostReadExecutor::getBuild(int workspaceId, int id, QObject* context,
    std::function<void(const std::optional<Build>&)> completion, ErrorCallback failure)
{
    QPointer<QObject> guard(context);
    enqueue(QStringLiteral("builds.get"), [=, completion = std::move(completion)]
        (ApplicationServices& services, const QSqlDatabase&) mutable {
        auto result = services.builds().get(id);
        if (result && result->workspaceId() != workspaceId)
            result.reset();
        if (guard) QMetaObject::invokeMethod(guard,
            [guard, completion, result = std::move(result)]() mutable {
                if (guard) completion(result);
            }, Qt::QueuedConnection);
    }, context, std::move(failure));
}
void HostReadExecutor::buildRequirements(int id, QObject* context,
    std::function<void(const QList<BuildRequirement>&)> completion, ErrorCallback failure)
{ HOST_READ_METHOD_BODY("builds.requirements", services.builds().requirements(id), QList<BuildRequirement>); }
void HostReadExecutor::missingParts(int workspaceId, int buildId, QObject* context,
    std::function<void(const QList<MissingPartsService::MissingPart>&)> completion, ErrorCallback failure)
{ HOST_READ_METHOD_BODY("builds.missingParts", services.builds().missingParts(workspaceId, buildId), QList<MissingPartsService::MissingPart>); }
void HostReadExecutor::pullingView(int buildId, QObject* context,
    std::function<void(const BuildPullingService::PullingView&)> completion, ErrorCallback failure)
{ HOST_READ_METHOD_BODY("builds.pulling", services.builds().pullingView(buildId), BuildPullingService::PullingView); }
void HostReadExecutor::searchCollection(const CollectionSearchCriteria& criteria, QObject* context,
    std::function<void(const CollectionApplicationService::Page&)> completion, ErrorCallback failure)
{ HOST_READ_METHOD_BODY("collection.search", services.collection().search(criteria), CollectionApplicationService::Page); }
void HostReadExecutor::getCollection(int id, QObject* context,
    std::function<void(const std::optional<CollectionSearchResult>&)> completion, ErrorCallback failure)
{ HOST_READ_METHOD_BODY("collection.get", services.collection().getDisplay(id), std::optional<CollectionSearchResult>); }

void HostReadExecutor::effectivePartReference(const PartReferenceManifest& manifest, QObject* context,
    std::function<void(const QList<PartReferenceEntry>&, const QString&)> completion,
    ErrorCallback failure)
{
    QPointer<QObject> guard(context);
    enqueue(QStringLiteral("partReference.customizations"),
        [guard, completion = std::move(completion), manifest]
        (ApplicationServices& services, const QSqlDatabase&) mutable {
            QString error;
            auto result = services.partReferenceCustomizations().effectiveEntries(manifest, &error);
            if (guard) QMetaObject::invokeMethod(guard,
                [guard, completion, result = std::move(result), error]() mutable {
                    if (guard) completion(result, error);
                }, Qt::QueuedConnection);
        }, context, std::move(failure));
}

#undef HOST_READ_METHOD_BODY

namespace {
QString locationPath(const QSqlDatabase& db, int id)
{
    StorageLocationRepository locations(db); QStringList names; int guard = 0;
    while (id > 0 && guard++ < 100) { const auto value = locations.getById(id); if (!value) break;
        names.prepend(value->name()); id = value->parentLocationId(); }
    return names.join(QStringLiteral(" / "));
}

QString manufacturerName(const QSqlDatabase& db, int id)
{
    QSqlQuery query(db);
    query.prepare(QStringLiteral("SELECT name FROM manufacturer WHERE id = :id"));
    query.bindValue(QStringLiteral(":id"), id);
    return query.exec() && query.next() ? query.value(0).toString() : QString();
}
}

void HostReadExecutor::getInventoryPortable(int workspaceId, int id, QObject* context,
    std::function<void(const std::optional<RemoteReadDto::InventoryDetail>&)> completion,
    ErrorCallback failure)
{
    QPointer<QObject> guard(context);
    enqueue(QStringLiteral("inventory.get"), [=, completion=std::move(completion)](ApplicationServices& services,const QSqlDatabase& db) mutable {
        const auto record=services.inventory().get(id); std::optional<RemoteReadDto::InventoryDetail> result;
        if(record && record->workspaceId() == workspaceId){RemoteReadDto::InventoryDetail d;d.inventoryRecordId=record->id();d.workspaceId=record->workspaceId();d.quantity=record->quantity();d.storageId=record->storageLocationId();d.storagePath=locationPath(db,d.storageId);d.manufacturerDisplay=manufacturerName(db,record->manufacturerId());d.condition=record->condition();d.ownershipType=record->ownershipType();d.createdUtc=record->createdUtc();d.modifiedUtc=record->modifiedUtc();QSqlQuery aq(db);aq.prepare("SELECT COALESCE(SUM(quantity_allocated),0) FROM build_allocation WHERE inventory_record_id=?");aq.addBindValue(record->id());if(aq.exec()&&aq.next())d.allocatedQuantity=aq.value(0).toInt();const auto p=PartRepository(db).getById(record->partId());const auto c=ColorRepository(db).getById(record->colorId());if(p){d.partNumber=p->partNumber();d.partNameFallback=p->name();}if(c){d.rebrickableColorId=c->rebrickableId();d.colorNameFallback=c->name();}result=d;}
        if(guard)QMetaObject::invokeMethod(guard,[guard,completion,result=std::move(result)]()mutable{if(guard)completion(result);},Qt::QueuedConnection);
    },context,std::move(failure));
}

void HostReadExecutor::listBuildsPortable(int workspaceId, bool archived,
    const RemoteReadDto::PageRequest& page, QObject* context,
    std::function<void(const RemoteReadDto::Page<RemoteReadDto::BuildSummary>&)> completion,
    ErrorCallback failure)
{
    QPointer<QObject> guard(context);
    enqueue(QStringLiteral("builds.list"), [=, completion=std::move(completion)]
        (ApplicationServices& services, const QSqlDatabase& db) mutable {
        RemoteReadDto::Page<RemoteReadDto::BuildSummary> result;
        result.page=page.page; result.pageSize=page.pageSize;
        const auto builds=services.builds().list(workspaceId, archived);
        result.totalRows=builds.size();
        const int begin=(page.page-1)*page.pageSize;
        for (int i=begin;i<qMin(begin+page.pageSize,builds.size());++i) {
            const Build& build=builds.at(i);
            RemoteReadDto::BuildSummary value;
            value.buildId = build.id(); value.workspaceId = build.workspaceId();
            value.buildType = build.buildType(); value.name = build.name();
            value.setNumber = build.setNumber();
            if (build.buildType() == QStringLiteral("Minifig"))
                value.minifigNumber = build.sourceReference();
            value.inventoryMode = build.inventoryMode();
            if (build.inventoryMode() == QStringLiteral("CompleteSet"))
                value.manufacturerDisplay = manufacturerName(db, build.manufacturerId());
            value.status = build.status(); value.notes = build.notes(); value.active = build.isActive();
            value.createdUtc = build.createdUtc(); value.modifiedUtc = build.modifiedUtc();
            result.rows.append(value);
        }
        if (guard) QMetaObject::invokeMethod(guard,
            [guard, completion, result=std::move(result)]() mutable {
                if (guard) completion(result);
            }, Qt::QueuedConnection);
    }, context, std::move(failure));
}

void HostReadExecutor::getBuildPortable(int workspaceId, int buildId, QObject* context,
    std::function<void(const std::optional<RemoteReadDto::BuildDetail>&)> completion,
    ErrorCallback failure)
{
    QPointer<QObject> guard(context);
    enqueue(QStringLiteral("builds.get"), [=, completion=std::move(completion)]
        (ApplicationServices& services, const QSqlDatabase& db) mutable {
        std::optional<RemoteReadDto::BuildDetail> result;
        const auto build = services.builds().get(buildId);
        if (build && build->workspaceId() == workspaceId) {
            RemoteReadDto::BuildDetail value;
            value.buildId = build->id(); value.workspaceId = build->workspaceId();
            value.buildType = build->buildType(); value.name = build->name();
            value.setNumber = build->setNumber();
            if (build->buildType() == QStringLiteral("Minifig"))
                value.minifigNumber = build->sourceReference();
            value.inventoryMode = build->inventoryMode();
            if (build->inventoryMode() == QStringLiteral("CompleteSet"))
                value.manufacturerDisplay = manufacturerName(db, build->manufacturerId());
            value.status = build->status(); value.notes = build->notes(); value.active = build->isActive();
            value.createdUtc = build->createdUtc(); value.modifiedUtc = build->modifiedUtc();
            result = value;
        }
        if (guard) QMetaObject::invokeMethod(guard,
            [guard, completion, result=std::move(result)]() mutable {
                if (guard) completion(result);
            }, Qt::QueuedConnection);
    }, context, std::move(failure));
}

void HostReadExecutor::buildCancellationReturnsPortable(int workspaceId, int buildId,
    QObject* context,
    std::function<void(const std::optional<QList<RemoteReadDto::BuildCancellationReturnRow>>&)> completion,
    ErrorCallback failure)
{
    QPointer<QObject> guard(context);
    enqueue(QStringLiteral("builds.cancelReturns"), [=, completion=std::move(completion)]
        (ApplicationServices& services, const QSqlDatabase& db) mutable {
        std::optional<QList<RemoteReadDto::BuildCancellationReturnRow>> result;
        const auto build=services.builds().get(buildId);
        if(build&&build->workspaceId()==workspaceId){
            QList<RemoteReadDto::BuildCancellationReturnRow> rows;
            BuildAllocationRepository allocations(db); ManufacturerRepository manufacturers(db);
            for(const auto& requirement:BuildRequirementRepository(db).getByBuild(buildId)){
                if(requirement.quantityPulled()<=0)continue;
                const auto part=PartRepository(db).getById(requirement.partId());
                const auto color=ColorRepository(db).getById(requirement.colorId());
                int total=0;
                for(const auto& provenance:allocations.pulledManufacturerProvenance(
                        buildId,requirement.partId(),requirement.colorId())){
                    const auto manufacturer=manufacturers.getById(provenance.manufacturerId);
                    if(!manufacturer)continue;
                    RemoteReadDto::BuildCancellationReturnRow row;
                    row.requirementId=requirement.id();row.partNumber=part?part->partNumber():QString::number(requirement.partId());
                    row.partNameFallback=part?part->name():QString();row.colorNameFallback=color?color->name():QString::number(requirement.colorId());
                    row.manufacturerDisplay=manufacturer->name();row.quantityPulled=provenance.quantityPulled;row.spare=requirement.isSpare();
                    total+=row.quantityPulled;rows.append(row);
                }
                if(total!=requirement.quantityPulled()){ result=std::nullopt; goto done; }
            }
            result=rows;
        }
done:
        if(guard)QMetaObject::invokeMethod(guard,[guard,completion,result=std::move(result)]()mutable{if(guard)completion(result);},Qt::QueuedConnection);
    },context,std::move(failure));
}

void HostReadExecutor::buildDisassemblyReturnsPortable(int workspaceId, int buildId,
    QObject* context,
    std::function<void(const std::optional<QList<RemoteReadDto::BuildCancellationReturnRow>>&)> completion,
    ErrorCallback failure)
{
    QPointer<QObject> guard(context);
    enqueue(QStringLiteral("builds.disassemblyReturns"), [=, completion=std::move(completion)]
        (ApplicationServices& services, const QSqlDatabase& db) mutable {
        std::optional<QList<RemoteReadDto::BuildCancellationReturnRow>> result;
        const auto plan=BuildLifecycleService(db).disassemblyReturnPlan(buildId);
        if(plan.success&&plan.build.workspaceId()==workspaceId){
            QList<RemoteReadDto::BuildCancellationReturnRow> rows;
            ManufacturerRepository manufacturers(db);
            for(const auto& source:plan.rows){
                const auto part=PartRepository(db).getById(source.partId);
                const auto color=ColorRepository(db).getById(source.colorId);
                const auto manufacturer=manufacturers.getById(source.manufacturerId);
                if(!part||!color||!manufacturer){result=std::nullopt;goto done_disassembly;}
                rows.append({source.requirementId,part->partNumber(),part->name(),color->name(),manufacturer->name(),source.quantity,source.spare});
            }
            result=rows;
        }
done_disassembly:
        if(guard)QMetaObject::invokeMethod(guard,[guard,completion,result=std::move(result)]()mutable{if(guard)completion(result);},Qt::QueuedConnection);
    },context,std::move(failure));
}

void HostReadExecutor::searchInventoryPortable(const RemoteReadDto::InventorySearchRequest&r,QObject*context,std::function<void(const InventoryApplicationService::Page&)>completion,ErrorCallback failure)
{
 QPointer<QObject>guard(context);enqueue(QStringLiteral("inventory.search"),[=,completion=std::move(completion)](ApplicationServices&s,const QSqlDatabase&db)mutable{InventorySearchCriteria c;c.workspaceId=int(r.workspaceId);c.searchText=r.text;c.storageLocationId=int(r.storageId);c.limit=r.paging.pageSize;c.offset=(r.paging.page-1)*r.paging.pageSize;if(r.rebrickableCategoryId>=0){const auto category=PartCategoryRepository(db).getByRebrickableId(r.rebrickableCategoryId);if(!category){InventoryApplicationService::Page empty;if(guard)QMetaObject::invokeMethod(guard,[guard,completion,empty]()mutable{if(guard)completion(empty);},Qt::QueuedConnection);return;}c.categoryId=category->id();}if(r.rebrickableColorId>=0){const auto color=ColorRepository(db).getByRebrickableId(r.rebrickableColorId);if(!color){InventoryApplicationService::Page empty;if(guard)QMetaObject::invokeMethod(guard,[guard,completion,empty]()mutable{if(guard)completion(empty);},Qt::QueuedConnection);return;}c.colorId=color->id();}auto out=s.inventory().search(c);if(guard)QMetaObject::invokeMethod(guard,[guard,completion,out=std::move(out)]()mutable{if(guard)completion(out);},Qt::QueuedConnection);},context,std::move(failure));
}

void HostReadExecutor::inventoryHistoryPortable(int workspaceId,const QString& partNumber,int rbColor,QObject* context,std::function<void(const QList<RemoteReadDto::InventoryHistoryRow>&)> completion,ErrorCallback failure)
{
    QPointer<QObject> guard(context); enqueue(QStringLiteral("inventory.history"),[=,completion=std::move(completion)](ApplicationServices& services,const QSqlDatabase& db)mutable{QList<RemoteReadDto::InventoryHistoryRow> out;const auto p=PartRepository(db).getByPartNumber(partNumber);const auto c=ColorRepository(db).getByRebrickableId(rbColor);if(p&&c){for(const auto&x:services.inventory().history(workspaceId,p->id(),c->id()))out.append({x.movementId,x.movementType,x.quantityChange,x.fromStorageLocationId,locationPath(db,x.fromStorageLocationId),x.toStorageLocationId,locationPath(db,x.toStorageLocationId),x.condition,x.ownershipType,x.referenceType,x.referenceId,x.notes,x.createdUtc});}if(guard)QMetaObject::invokeMethod(guard,[guard,completion,out=std::move(out)]()mutable{if(guard)completion(out);},Qt::QueuedConnection);},context,std::move(failure));
}

void HostReadExecutor::listLostInventoryPortable(int workspaceId, QObject* context,
    std::function<void(const QList<RemoteReadDto::LostInventoryRow>&)> completion,
    ErrorCallback failure)
{
    QPointer<QObject> guard(context);
    enqueue(QStringLiteral("inventory.lost.list"),
        [=, completion=std::move(completion)](ApplicationServices&, const QSqlDatabase& db) mutable {
            QList<RemoteReadDto::LostInventoryRow> out;
            const QList<StorageLocation> locations = StorageLocationRepository(db).getByWorkspace(workspaceId);
            QHash<int, StorageLocation> byId;
            for (const auto& location : locations) byId.insert(location.id(), location);
            auto pathFor = [&byId](int id) {
                QStringList names; int guardCount = 0;
                while (id > 0 && byId.contains(id) && guardCount++ < 100) {
                    const auto location = byId.value(id); names.prepend(location.name());
                    id = location.parentLocationId();
                }
                return names.join(QStringLiteral(" / "));
            };
            for (const auto& item : LostInventoryRepository(db).getOutstanding(workspaceId)) {
                const auto color = ColorRepository(db).getById(item.colorId);
                if (!color || color->rebrickableId() < 0) continue;
                out.append({item.partNumber, item.partName, color->rebrickableId(), item.colorName,
                    item.outstandingQuantity, item.lastStorageLocationId,
                    pathFor(item.lastStorageLocationId), item.condition,
                    item.ownershipType, item.lastLostUtc});
            }
            if (guard) QMetaObject::invokeMethod(guard,
                [guard, completion, out=std::move(out)]() mutable { if (guard) completion(out); },
                Qt::QueuedConnection);
        }, context, std::move(failure));
}

void HostReadExecutor::searchBuildabilityPortable(
    const RemoteBuildabilityDto::SearchRequest& request, QObject* context,
    std::function<void(const std::optional<RemoteBuildabilityDto::SearchResponse>&)> completion,
    ErrorCallback failure)
{
    QPointer<QObject> guard(context);
    enqueue(QStringLiteral("buildability.inventory.search"),
        [=, completion=std::move(completion)](ApplicationServices&, const QSqlDatabase& db) mutable {
            bool workspaceQueryOk = false;
            const bool workspaceExists = activeWorkspaceExists(
                db, request.workspaceId, &workspaceQueryOk);
            if (!workspaceQueryOk) {
                deliverFailure(guard, failure, QStringLiteral("Unable to validate the Workspace."));
                return;
            }
            if (!workspaceExists) {
                if (guard) QMetaObject::invokeMethod(guard,
                    [guard, completion] { if (guard) completion(std::nullopt); },
                    Qt::QueuedConnection);
                return;
            }
            InventoryBuildabilitySearch local;
            local.workspaceId = int(request.workspaceId);
            local.text = request.text;
            local.minimumPercent = request.minimumPercent;
            local.minimumSetParts = request.minimumSetParts;
            local.yearFrom = request.yearFrom;
            local.yearTo = request.yearTo;
            local.fullyBuildableOnly = request.fullyBuildableOnly;
            local.includeCollection = request.includeCollection;
            local.fullyBuildableFirst = request.fullyBuildableFirst;
            local.maximumResults = request.maximumResults;
            if (request.rebrickableThemeId > 0) {
                bool queryOk = false;
                local.themeCatalogId = activeThemeCatalogId(
                    db, request.rebrickableThemeId, &queryOk);
                if (!queryOk) {
                    deliverFailure(guard, failure,
                        QStringLiteral("Unable to resolve the Rebrickable Theme identity."));
                    return;
                }
                if (local.themeCatalogId <= 0) {
                    if (guard) QMetaObject::invokeMethod(guard,
                        [guard, completion] { if (guard) completion(std::nullopt); },
                        Qt::QueuedConnection);
                    return;
                }
            }
            const auto result = InventoryBuildabilityRepository(db).search(local);
            if (!result.success) {
                deliverFailure(guard, failure, result.errorMessage);
                return;
            }
            RemoteBuildabilityDto::SearchResponse out;
            out.qualifyingCount = result.qualifyingCount;
            out.capReached = result.capReached;
            out.eligibleCollectionSourceCount = result.eligibleCollectionSources;
            out.dormantCollectionSourceCount = result.dormantCollectionSources;
            for (const auto& item : result.sets) out.rows.append(portableBuildability(item));
            if (out.rows.size() > request.maximumResults)
                out.rows = out.rows.mid(0, request.maximumResults);
            out.returnedCount = out.rows.size();
            out.capReached = out.qualifyingCount > out.returnedCount;
            if (guard) QMetaObject::invokeMethod(guard,
                [guard, completion, out=std::move(out)]() mutable {
                    if (guard) completion(out);
                }, Qt::QueuedConnection);
        }, context, std::move(failure));
}

void HostReadExecutor::buildabilityDetailsPortable(
    const RemoteBuildabilityDto::DetailsRequest& request, QObject* context,
    std::function<void(const std::optional<RemoteBuildabilityDto::DetailsResponse>&)> completion,
    ErrorCallback failure)
{
    QPointer<QObject> guard(context);
    enqueue(QStringLiteral("buildability.inventory.details"),
        [=, completion=std::move(completion)](ApplicationServices&, const QSqlDatabase& db) mutable {
            bool workspaceQueryOk = false;
            const bool workspaceExists = activeWorkspaceExists(
                db, request.workspaceId, &workspaceQueryOk);
            if (!workspaceQueryOk) {
                deliverFailure(guard, failure, QStringLiteral("Unable to validate the Workspace."));
                return;
            }
            if (!workspaceExists) {
                if (guard) QMetaObject::invokeMethod(guard,
                    [guard, completion] { if (guard) completion(std::nullopt); },
                    Qt::QueuedConnection);
                return;
            }
            bool lookupOk = false;
            const auto matches = SetCatalogRepository(db).getExactMatchesBySetNumber(
                request.setNumber, &lookupOk);
            if (!lookupOk) {
                deliverFailure(guard, failure, QStringLiteral("Unable to resolve the Set identity."));
                return;
            }
            if (matches.size() != 1) {
                if (guard) QMetaObject::invokeMethod(guard,
                    [guard, completion] { if (guard) completion(std::nullopt); },
                    Qt::QueuedConnection);
                return;
            }
            InventoryBuildabilitySearch local;
            local.workspaceId = int(request.workspaceId);
            local.minimumPercent = 0;
            local.minimumSetParts = 1;
            local.includeCollection = request.includeCollection;
            local.maximumResults = 50;
            local.exactSetCatalogId = matches.first().id();
            const auto evaluated = InventoryBuildabilityRepository(db).search(local);
            if (!evaluated.success) {
                deliverFailure(guard, failure, evaluated.errorMessage);
                return;
            }
            if (evaluated.sets.isEmpty()) {
                if (guard) QMetaObject::invokeMethod(guard,
                    [guard, completion] { if (guard) completion(std::nullopt); },
                    Qt::QueuedConnection);
                return;
            }
            const auto& item = evaluated.sets.first();
            RemoteBuildabilityDto::DetailsResponse out;
            out.candidate = portableBuildability(item);
            out.page = request.paging.page;
            out.pageSize = request.paging.pageSize;
            out.totalCount = item.requirements.size();
            const int begin = qMin(out.totalCount, (out.page - 1) * out.pageSize);
            const int end = qMin(out.totalCount, begin + out.pageSize);
            for (int i = begin; i < end; ++i) {
                const auto& row = item.requirements.at(i);
                out.requirements.append({row.partNumber, row.partName,
                    row.rebrickableColorId, row.colorName, row.required,
                    row.looseAvailable, row.looseUsed, row.collectionUsed, row.missing});
            }
            out.returnedCount = out.requirements.size();
            for (const auto& source : item.sources)
                out.sources.append({source.collectionItemId, source.label,
                    source.state, source.piecesUsed});
            if (guard) QMetaObject::invokeMethod(guard,
                [guard, completion, out=std::move(out)]() mutable {
                    if (guard) completion(out);
                }, Qt::QueuedConnection);
        }, context, std::move(failure));
}

void HostReadExecutor::buildRequirementsPortable(int workspaceId,int buildId,const RemoteReadDto::PageRequest&page,QObject*context,std::function<void(const RemoteReadDto::Page<RemoteReadDto::BuildRequirement>&)>completion,ErrorCallback failure)
{
 QPointer<QObject>guard(context);enqueue(QStringLiteral("builds.requirements"),[=,completion=std::move(completion)](ApplicationServices&s,const QSqlDatabase&db)mutable{
  RemoteReadDto::Page<RemoteReadDto::BuildRequirement> out;out.page=page.page;out.pageSize=page.pageSize;
  const auto build=s.builds().get(buildId);if(!build||build->workspaceId()!=workspaceId){out.resourceFound=false;if(guard)QMetaObject::invokeMethod(guard,[guard,completion,out]()mutable{if(guard)completion(out);},Qt::QueuedConnection);return;}
  const auto all=s.builds().requirements(buildId);BuildRequirementAvailabilityService availability(db);out.totalRows=all.size();const int begin=(page.page-1)*page.pageSize;
  for(int i=begin;i<qMin(begin+page.pageSize,all.size());++i){const auto&x=all[i];RemoteReadDto::BuildRequirement d;d.requirementId=x.id();d.buildId=x.buildId();d.quantityRequired=x.quantityRequired();d.quantityPulled=x.quantityPulled();d.quantityReleased=x.quantityReleased();d.modifiedUtc=x.modifiedUtc();d.spare=x.isSpare();const auto stock=availability.project(workspaceId,x);d.owned=stock.owned;d.thisRequirementAllocated=stock.thisRequirementAllocated;d.otherAllocated=stock.otherAllocated;d.available=stock.available;d.missing=stock.missing;const auto p=PartRepository(db).getById(x.partId());const auto c=ColorRepository(db).getById(x.colorId());const auto sp=PartRepository(db).getById(x.substitutePartId());const auto sc=ColorRepository(db).getById(x.substituteColorId());if(p){d.partNumber=p->partNumber();d.partNameFallback=p->name();}if(c){d.rebrickableColorId=c->rebrickableId();d.colorNameFallback=c->name();}if(sp)d.substitutePartNumber=sp->partNumber();if(sc)d.substituteRebrickableColorId=sc->rebrickableId();out.rows.append(d);}
  if(guard)QMetaObject::invokeMethod(guard,[guard,completion,out=std::move(out)]()mutable{if(guard)completion(out);},Qt::QueuedConnection);
 },context,std::move(failure));
}

void HostReadExecutor::missingPartsPortable(int workspaceId,int buildId,const RemoteReadDto::PageRequest&page,QObject*context,std::function<void(const RemoteReadDto::Page<RemoteReadDto::MissingPart>&)>completion,ErrorCallback failure)
{
 QPointer<QObject>guard(context);enqueue(QStringLiteral("builds.missingParts"),[=,completion=std::move(completion)](ApplicationServices&s,const QSqlDatabase&db)mutable{RemoteReadDto::Page<RemoteReadDto::MissingPart>out;out.page=page.page;out.pageSize=page.pageSize;const auto build=s.builds().get(buildId);if(!build||build->workspaceId()!=workspaceId){out.resourceFound=false;if(guard)QMetaObject::invokeMethod(guard,[guard,completion,out]()mutable{if(guard)completion(out);},Qt::QueuedConnection);return;}const auto all=s.builds().missingParts(workspaceId,buildId);out.totalRows=all.size();const int begin=(page.page-1)*page.pageSize;for(int i=begin;i<qMin(begin+page.pageSize,all.size());++i){const auto&x=all[i];RemoteReadDto::MissingPart d;d.partNumber=x.partNumber;d.partNameFallback=x.partName;d.colorNameFallback=x.colorName;d.required=x.required;d.pulled=x.pulled;d.remaining=x.remaining;d.owned=x.owned;d.thisBuildAllocated=x.thisBuildAllocated;d.otherBuildsAllocated=x.otherBuildsAllocated;d.available=x.available;d.missing=x.missing;const auto color=ColorRepository(db).getById(x.colorId);if(color)d.rebrickableColorId=color->rebrickableId();out.rows.append(d);}if(guard)QMetaObject::invokeMethod(guard,[guard,completion,out=std::move(out)]()mutable{if(guard)completion(out);},Qt::QueuedConnection);},context,std::move(failure));
}

void HostReadExecutor::pullingPortable(int workspaceId,int buildId,const RemoteReadDto::PageRequest&page,QObject*context,std::function<void(const RemoteReadDto::Page<RemoteReadDto::PullingRow>&)>completion,ErrorCallback failure)
{
QPointer<QObject>guard(context);enqueue(QStringLiteral("builds.pulling"),[=,completion=std::move(completion)](ApplicationServices&s,const QSqlDatabase&db)mutable{RemoteReadDto::Page<RemoteReadDto::PullingRow>out;out.page=page.page;out.pageSize=page.pageSize;const auto build=s.builds().get(buildId);if(!build||build->workspaceId()!=workspaceId){out.resourceFound=false;if(guard)QMetaObject::invokeMethod(guard,[guard,completion,out]()mutable{if(guard)completion(out);},Qt::QueuedConnection);return;}const auto view=s.builds().pullingView(buildId);out.totalRows=view.items.size();const int begin=(page.page-1)*page.pageSize;for(int i=begin;i<qMin(begin+page.pageSize,view.items.size());++i){const auto&x=view.items[i];RemoteReadDto::PullingRow d;d.requirementId=x.buildRequirementId;d.allocationId=x.allocationId;d.inventoryRecordId=x.inventoryRecordId;d.storageId=x.storageLocationId;d.storagePath=x.storagePath;d.partNumber=x.partNumber;d.partNameFallback=x.partName;d.colorNameFallback=x.colorName;d.quantityRequired=x.quantityRequired;d.quantityPulled=x.quantityPulledForRequirement;d.quantityAllocated=x.quantityAllocatedHere;d.inventoryQuantity=x.inventoryQuantity;d.substitution=x.isSubstitution;const auto allocation=BuildAllocationRepository(db).getById(x.allocationId);if(allocation)d.allocationModifiedUtc=allocation->modifiedUtc();const auto color=ColorRepository(db).getById(x.colorId);if(color)d.rebrickableColorId=color->rebrickableId();out.rows.append(d);}if(guard)QMetaObject::invokeMethod(guard,[guard,completion,out=std::move(out)]()mutable{if(guard)completion(out);},Qt::QueuedConnection);},context,std::move(failure));
}
