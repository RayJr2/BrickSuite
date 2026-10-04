#include "CatalogSetPartOutService.h"
#include "CompositionToInventoryService.h"
#include "../application/HostOperationalGate.h"
#include "../application/HostStorageMutationService.h"
#include "../../repositories/EffectiveSetCompositionRepository.h"
#include "../../repositories/SetCatalogRepository.h"
#include "../../repositories/WorkspaceRepository.h"
#include "../../repositories/ManufacturerRepository.h"
#include "../../repositories/StorageLocationRepository.h"
#include "../../repositories/StorageLocationTypeRepository.h"
#include "../../repositories/RemoteMutationReceiptRepository.h"
#include "../../settings/UserSettings.h"
#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>
#include <limits>
#include <map>

namespace {
const QString operation = QStringLiteral("catalogSet.partOut.local");
QString hash(const QJsonObject& object)
{ return QString::fromLatin1(QCryptographicHash::hash(QJsonDocument(object).toJson(QJsonDocument::Compact), QCryptographicHash::Sha256).toHex()); }
QJsonObject requestData(const CatalogSetPartOutService::Request& r)
{
    return {{"workspace",r.workspaceId},{"set",r.setCatalogId},{"copies",r.copies},{"spares",r.includeSpares},
        {"condition",r.condition},{"createStorage",r.createStorage},{"storage",r.storageId},
        {"type",r.storageTypeId},{"parent",r.parentStorageId},{"name",r.storageName.trimmed()}};
}
QString executionBlocker()
{
    if (UserSettings::instance().sharedDataSource() != SharedDataSource::ThisComputer)
        return QStringLiteral("Parting out a catalog Set is currently available on the Host/local database only.");
    if (!HostOperationalGate::localWritesAllowed())
        return QStringLiteral("Host Maintenance prevents operational changes.");
    return {};
}
QString storagePath(QSqlDatabase db, int workspace, int id)
{
    QSet<int> visited; QStringList names;
    while (id > 0) {
        if (visited.contains(id) || visited.size() >= 10000) return {};
        visited.insert(id);
        const auto item = StorageLocationRepository(db).getById(id);
        if (!item || item->workspaceId() != workspace || !item->isActive()) return {};
        names.prepend(item->name()); id = item->parentLocationId();
    }
    return names.join(QStringLiteral(" / "));
}
QList<CompositionToInventoryService::Row> inventoryRows(const CatalogSetPartOutService::Plan& plan,
    const CatalogSetPartOutService::Request& request, int storage)
{
    QList<CompositionToInventoryService::Row> rows;
    for (const auto& row : plan.rows)
        if (!row.spare || request.includeSpares) rows.append({row.partId,row.colorId,storage,row.total});
    return rows;
}
CompositionToInventoryService::Context inventoryContext(const CatalogSetPartOutService::Plan& plan,
    const CatalogSetPartOutService::Request& request)
{
    CompositionToInventoryService::Context context;
    context.workspaceId=request.workspaceId; context.manufacturerId=plan.manufacturerId; context.condition=request.condition;
    context.referenceType=QStringLiteral("SetCatalog"); context.referenceId=QString::number(request.setCatalogId);
    context.notes=QStringLiteral("Added from Set %1 - %2; copies: %3; include spares: %4; operation: %5; composition: %6.")
        .arg(plan.setNumber,plan.setName).arg(request.copies).arg(request.includeSpares?"yes":"no",request.operationId,plan.source);
    return context;
}
template<class T, class F> T onConnection(const QString& path, F work)
{
    const QString name=QStringLiteral("catalog-part-out-")+QUuid::createUuid().toString(QUuid::WithoutBraces);
    T result;
    {
        QSqlDatabase db=QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),name);
        db.setDatabaseName(path); db.setConnectOptions(QStringLiteral("QSQLITE_BUSY_TIMEOUT=5000"));
        if (!db.open()) result.message=QStringLiteral("Unable to open the part-out database connection: ")+db.lastError().text();
        else {
            QSqlQuery pragma(db);
            if (!pragma.exec("PRAGMA foreign_keys=ON")) result.message=QStringLiteral("Unable to enable database integrity checks.");
            else result=work(CatalogSetPartOutService(db));
        }
        db.close();
    }
    QSqlDatabase::removeDatabase(name);
    return result;
}
}

CatalogSetPartOutService::Plan CatalogSetPartOutService::preview(const Request& r) const
{
    Plan plan;
    auto fail=[&](const QString& message) { plan.message=message; return plan; };
    const QString blocked=executionBlocker(); if (!blocked.isEmpty()) return fail(blocked);
    const auto workspace=WorkspaceRepository(m_database).getById(r.workspaceId);
    if (!workspace || !workspace->isActive()) return fail("Select an active Workspace.");
    if (r.copies<1 || r.copies>MaximumCopies || (r.condition!="Used" && r.condition!="New"))
        return fail("Select a positive copy count and a valid Condition.");
    const auto set=SetCatalogRepository(m_database).getById(r.setCatalogId);
    if (!set) return fail("The catalog Set is unavailable.");
    plan.setNumber=set->setNumber(); plan.setName=set->name();
    plan.manufacturerId=ManufacturerRepository(m_database).legoManufacturerId();
    if (plan.manufacturerId<=0) return fail("The LEGO manufacturer identity is unavailable.");
    const auto composition=EffectiveSetCompositionRepository(m_database).forSet(r.setCatalogId,true);
    if (!composition.success) return fail("Unable to read effective Set composition: "+composition.message);
    const bool revision=composition.source==EffectiveSetCompositionSource::PreferredRebrickableRevision;
    plan.source=revision?QString("Rebrickable revision %1 (version %2)").arg(composition.revisionId).arg(composition.revisionVersion)
                        :QStringLiteral("Catalog Parts List fallback");
    QSqlQuery nested(m_database);
    nested.prepare("SELECT 1 FROM set_inventory_revision r WHERE r.set_catalog_id=? AND r.is_active=1 AND "
        "(EXISTS(SELECT 1 FROM set_inventory_minifig m WHERE m.set_inventory_revision_id=r.id) OR "
        "EXISTS(SELECT 1 FROM set_inventory_contained_set s WHERE s.set_inventory_revision_id=r.id)) LIMIT 1");
    nested.addBindValue(r.setCatalogId);
    if (!nested.exec()) return fail("Unable to verify nested composition coverage.");
    if (nested.next()) return fail("BrickSuite cannot safely part out this Set because its catalog composition contains nested Minifig or Set relationships whose Part coverage cannot yet be verified.");

    // Check raw rows independently of the effective reader's INNER JOINs.
    QSqlQuery raw(m_database);
    raw.prepare(revision
        ? "SELECT x.quantity,p.id,p.is_active,c.id,typeof(x.quantity) FROM set_inventory_part x LEFT JOIN part p ON p.id=x.part_id LEFT JOIN color c ON c.id=x.color_id WHERE x.set_inventory_revision_id=?"
        : "SELECT x.quantity_required,p.id,p.is_active,c.id,typeof(x.quantity_required) FROM set_catalog_part x LEFT JOIN part p ON p.id=x.part_id LEFT JOIN color c ON c.id=x.color_id WHERE x.set_catalog_id=?");
    raw.addBindValue(revision?composition.revisionId:r.setCatalogId);
    if (!raw.exec()) return fail("Unable to verify all stored composition rows.");
    int rawCount=0;
    while (raw.next()) {
        ++rawCount;
        if (raw.value(1).isNull() || !raw.value(2).toBool() || raw.value(3).isNull())
            return fail("A stored composition row contains an unresolved or inactive Part, or unresolved Color. No rows may be omitted.");
        if (raw.value(4).toString()!="integer" || raw.value(0).toLongLong()<=0
            || raw.value(0).toLongLong()>std::numeric_limits<int>::max()/qint64(r.copies))
            return fail("A stored composition quantity is invalid or overflows after multiplying copies.");
    }
    if (rawCount==0) return fail("This Set has no effective Part composition. Get or import its Catalog Parts List first.");
    if (rawCount!=composition.parts.size()) return fail("The effective composition omits stored rows; part-out is blocked.");
    std::map<std::pair<int,int>,qint64> totals;
    QJsonArray fingerprintRows;
    for (const auto& part:composition.parts) {
        const qint64 total=part.quantity*qint64(r.copies);
        plan.rows.append({part.partId,part.colorId,part.partNumber,part.partName,part.colorName,part.quantity,total,part.spare});
        qint64& category=part.spare?plan.sparePieces:plan.requiredPieces;
        if (category>std::numeric_limits<qint64>::max()-total) return fail("The piece total overflows.");
        category+=total;
        if (!part.spare || r.includeSpares) {
            auto& sum=totals[{part.partId,part.colorId}];
            if (sum>std::numeric_limits<int>::max()-total) return fail("Combined required/spare quantities overflow an Inventory row.");
            sum+=total;
        }
        fingerprintRows.append(QJsonArray{part.id,part.partId,part.colorId,QString::number(part.quantity),part.spare,part.partNumber,part.partName,part.colorName});
    }
    if (r.includeSpares && plan.requiredPieces>std::numeric_limits<qint64>::max()-plan.sparePieces) return fail("The combined piece total overflows.");
    plan.totalPieces=plan.requiredPieces+(r.includeSpares?plan.sparePieces:0);
    if (!plan.totalPieces) return fail("No pieces are selected for Inventory.");
    StorageLocationRepository storage(m_database);
    if (r.createStorage) {
        if (r.storageName.trimmed().isEmpty() || r.storageName.trimmed().size()>HostStorageMutationService::MaximumNameLength)
            return fail("Enter a Storage name of 1 to 200 characters.");
        const auto type=StorageLocationTypeRepository(m_database).getById(r.storageTypeId);
        if (!type || !type->isActive()) return fail("Select an active Storage type.");
        if (r.parentStorageId<0) return fail("The parent Storage location is invalid.");
        if (r.parentStorageId>0) {
            const QString path=storagePath(m_database,r.workspaceId,r.parentStorageId);
            if (path.isEmpty()) return fail("The parent must be active, in this Workspace, and have a valid hierarchy.");
            if (storage.hasInventoryChecked(r.parentStorageId)!=StorageLocationRepository::CheckResult::No
                || storage.hasCollectionChecked(r.parentStorageId)!=StorageLocationRepository::CheckResult::No)
                return fail("The parent contains Inventory or Collection contents, or its contents could not be verified. Choose an empty container parent.");
            plan.destination=path+" / ";
        }
        plan.destination+=r.storageName.trimmed();
        QSqlQuery duplicates(m_database);
        duplicates.prepare("SELECT 1 FROM storage_location WHERE workspace_id=? AND COALESCE(parent_location_id,0)=? AND name=? COLLATE NOCASE LIMIT 1");
        duplicates.addBindValue(r.workspaceId); duplicates.addBindValue(r.parentStorageId); duplicates.addBindValue(r.storageName.trimmed());
        if (!duplicates.exec()) return fail("Unable to check destination names.");
        if (duplicates.next()) plan.warnings<<"A sibling Storage location already has this name. A separate new location will be created.";
    } else {
        plan.destination=storagePath(m_database,r.workspaceId,r.storageId);
        if (plan.destination.isEmpty() || !storage.isValidInventoryDestination(r.workspaceId,r.storageId))
            return fail("Select an active Inventory-capable leaf destination in this Workspace.");
        const auto check=CompositionToInventoryService(m_database).validate(inventoryContext(plan,r),inventoryRows(plan,r,r.storageId));
        if (!check.success) return fail(check.message);
    }
    QSqlQuery owned(m_database);
    owned.prepare("SELECT COUNT(*) FROM collection_item WHERE workspace_id=? AND set_catalog_id=? AND item_type='Set' AND is_active=1 AND state IN ('Assembled','Sealed')");
    owned.addBindValue(r.workspaceId); owned.addBindValue(r.setCatalogId);
    if (!owned.exec() || !owned.next()) return fail("Unable to check existing Collection copies.");
    const int collectionCopies=owned.value(0).toInt();
    if (collectionCopies>0) plan.warnings<<"BrickSuite already has intact copies of this Set in My Collection. Continue only for an additional physical copy; use Collection Disassemble for an already-recorded copy.";
    QJsonObject fingerprint=requestData(r);
    fingerprint.insert("rows",fingerprintRows); fingerprint.insert("source",plan.source);
    fingerprint.insert("setNumber",plan.setNumber); fingerprint.insert("setName",plan.setName);
    fingerprint.insert("collectionCopies",collectionCopies); fingerprint.insert("warnings",QJsonArray::fromStringList(plan.warnings));
    fingerprint.insert("destination",plan.destination); fingerprint.insert("manufacturer",plan.manufacturerId);
    plan.fingerprint=hash(fingerprint); plan.success=true;
    return plan;
}

CatalogSetPartOutService::Result CatalogSetPartOutService::execute(const Request& r, const QString& fingerprint) const
{
    Result result; result.workspaceId=r.workspaceId;
    auto fail=[&](const QString& message) { result.message=message; return result; };
    const QString blocked=executionBlocker(); if (!blocked.isEmpty()) return fail(blocked);
    if (QUuid(r.operationId).isNull() || fingerprint.isEmpty()) return fail("A preview and operation identity are required.");
    QSqlDatabase db=m_database;
    if (!db.transaction()) return fail("Unable to begin part-out transaction: "+db.lastError().text());
    auto rollback=[&](const QString& message) { db.rollback(); return fail(message); };
    QJsonObject request=requestData(r); request.insert("preview",fingerprint);
    const QString requestHash=hash(request);
    RemoteMutationReceiptRepository receipts(db); QString receiptError;
    const auto receipt=receipts.find(r.operationId,&receiptError);
    if (!receiptError.isEmpty()) return rollback("Unable to verify part-out receipt: "+receiptError);
    if (receipt) {
        if (receipt->operation!=operation || receipt->workspaceId!=r.workspaceId || receipt->requestHash!=requestHash)
            return rollback("This operation identity was already used for a different request.");
        const auto saved=QJsonDocument::fromJson(receipt->resultJson.toUtf8()).object();
        if (!saved.value("storageId").toInt() || saved.value("pieces").toString().toLongLong()<=0)
            return rollback("The saved part-out receipt is unreadable; no Inventory was added.");
        db.rollback(); result.success=true; result.replayed=true;
        result.storageId=saved.value("storageId").toInt(); result.storageCreated=saved.value("created").toBool();
        result.totalPieces=saved.value("pieces").toString().toLongLong(); result.destination=saved.value("destination").toString();
        result.message=saved.value("message").toString(); return result;
    }
    const Plan plan=preview(r);
    if (!plan.success) return rollback(plan.message);
    if (plan.fingerprint!=fingerprint) return rollback("The Set composition or preview context changed. Review a fresh preview before confirming.");
    int destination=r.storageId;
    if (r.createStorage) {
        const auto created=HostStorageMutationService(db).add({r.workspaceId,r.parentStorageId,r.storageTypeId,r.storageName.trimmed(),{},true,false});
        if (!created.success) return rollback(created.error.message);
        destination=created.location.id();
    }
    const auto added=CompositionToInventoryService(db).addInCurrentTransaction(inventoryContext(plan,r),inventoryRows(plan,r,destination));
    if (!added.success) return rollback(added.message);
    result.storageId=destination; result.storageCreated=r.createStorage; result.totalPieces=added.totalPieces;
    result.destination=plan.destination;
    result.message=QString("Added %1 pieces from %2 - %3 to Inventory.\nStorage: %4")
        .arg(result.totalPieces).arg(plan.setNumber,plan.setName,plan.destination);
    const QJsonObject saved{{"storageId",destination},{"created",r.createStorage},{"pieces",QString::number(result.totalPieces)},
                           {"destination",result.destination},{"message",result.message}};
    if (!receipts.insert({r.operationId,operation,r.workspaceId,requestHash,"SUCCESS",
            QString::fromUtf8(QJsonDocument(saved).toJson(QJsonDocument::Compact)),QDateTime::currentDateTimeUtc(),"local"},&receiptError))
        return rollback("Unable to persist part-out receipt: "+receiptError);
    if (!db.commit()) return rollback("Unable to commit part-out: "+db.lastError().text());
    result.success=true; return result;
}

CatalogSetPartOutService::Plan CatalogSetPartOutService::previewFile(const QString& path,const Request& r)
{ return onConnection<Plan>(path,[&](const auto& service){
    QSqlDatabase db=service.m_database;
    if (!db.transaction()) { Plan plan; plan.message="Unable to begin composition preview."; return plan; }
    const Plan plan=service.preview(r); db.rollback(); return plan;
}); }
CatalogSetPartOutService::Result CatalogSetPartOutService::executeFile(const QString& path,const Request& r,const QString& fingerprint)
{ return onConnection<Result>(path,[&](const auto& service){return service.execute(r,fingerprint);}); }
