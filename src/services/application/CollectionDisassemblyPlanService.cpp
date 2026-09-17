#include "CollectionDisassemblyPlanService.h"

#include "../builds/BuildLifecycleService.h"
#include "../collection/CollectionDisassemblyService.h"
#include "../../repositories/ColorRepository.h"
#include "../../repositories/CollectionRepository.h"
#include "../../repositories/ManufacturerRepository.h"
#include "../../repositories/PartRepository.h"

#include <QCryptographicHash>

namespace {
CollectionDisassemblyPlanService::Result failure(const QString& code, const QString& message)
{
    CollectionDisassemblyPlanService::Result result;
    result.errorCode = code;
    result.message = message;
    return result;
}

QString planHash(const RemoteReadDto::CollectionDisassemblyPlan& plan)
{
    QByteArray value = plan.authority.toUtf8() + '\0'
        + QByteArray::number(plan.collectionItemId) + '\0'
        + QByteArray::number(plan.sourceBuildId) + '\0'
        + plan.modifiedUtc.toUTC().toString(Qt::ISODateWithMs).toUtf8();
    for (const auto& row : plan.rows) {
        value += '\0' + QByteArray::number(row.rowIndex) + '\0'
            + QByteArray::number(row.requirementId) + '\0'
            + row.partNumber.toUtf8() + '\0'
            + QByteArray::number(row.rebrickableColorId) + '\0'
            + row.manufacturerDisplay.toUtf8() + '\0'
            + QByteArray::number(row.quantity) + '\0'
            + QByteArray::number(row.spare);
    }
    return QString::fromLatin1(QCryptographicHash::hash(value, QCryptographicHash::Sha256).toHex());
}
}

CollectionDisassemblyPlanService::CollectionDisassemblyPlanService(
    const QSqlDatabase& database)
    : m_connectionName(database.connectionName())
{}

CollectionDisassemblyPlanService::Result CollectionDisassemblyPlanService::preview(
    int workspaceId, int collectionItemId) const
{
    const QSqlDatabase db = QSqlDatabase::database(m_connectionName, false);
    const auto display = CollectionRepository(db).displayById(collectionItemId);
    if (!display || display->item.workspaceId != workspaceId)
        return failure(QStringLiteral("NOT_FOUND"),
                       QStringLiteral("The Collection item was not found in this Workspace."));

    Result result;
    auto& plan = result.plan;
    const auto& item = display->item;
    plan.collectionItemId = item.id;
    plan.workspaceId = item.workspaceId;
    plan.sourceBuildId = item.sourceBuildId;
    plan.type = collectionItemTypeToString(item.type);
    plan.reference = display->displayReference;
    plan.name = display->displayName;
    plan.state = collectionItemStateToString(item.state);
    plan.condition = collectionItemConditionToString(item.condition);
    plan.completeness = collectionItemCompletenessToString(item.completeness);
    plan.storageId = item.storageLocationId;
    plan.nickname = item.nickname;
    plan.notes = item.notes;
    plan.active = item.isActive;
    plan.allowPartsSource = item.allowPartsSource;
    plan.modifiedUtc = item.modifiedUtc;

    int rowIndex = 0;
    if (item.sourceBuildId > 0) {
        const auto source = BuildLifecycleService(db)
            .linkedCollectionDisassemblyReturnPlan(collectionItemId);
        if (!source.success)
            return failure(QStringLiteral("CONFLICT"), source.message);
        plan.authority = QStringLiteral("Build");
        plan.inventoryMode = source.build.inventoryMode();
        ManufacturerRepository manufacturers(db);
        for (const auto& value : source.rows) {
            const auto part = PartRepository(db).getById(value.partId);
            const auto color = ColorRepository(db).getById(value.colorId);
            const auto manufacturer = manufacturers.getById(value.manufacturerId);
            if (!part || !color || !manufacturer)
                return failure(QStringLiteral("CONFLICT"),
                    QStringLiteral("The Build return plan contains an unavailable identity."));
            plan.rows.append({++rowIndex, value.requirementId, part->partNumber(), part->name(),
                color->rebrickableId(), color->name(), manufacturer->name(), value.quantity,
                value.spare});
        }
    } else {
        const auto source = CollectionDisassemblyService(db).preview(collectionItemId);
        if (!source.success)
            return failure(source.error == CollectionDisassemblyService::Error::NotFound
                    ? QStringLiteral("NOT_FOUND") : QStringLiteral("CONFLICT"), source.message);
        plan.authority = QStringLiteral("Catalog");
        plan.inventoryMode = QStringLiteral("CompleteSet");
        plan.excludedSparePieces = source.excludedSparePieces;
        const auto lego = ManufacturerRepository(db).getById(
            ManufacturerRepository(db).legoManufacturerId());
        if (!lego)
            return failure(QStringLiteral("CONFLICT"),
                           QStringLiteral("The LEGO manufacturer identity is unavailable."));
        for (const auto& value : source.rows) {
            const auto color = ColorRepository(db).getById(value.colorId);
            if (!color)
                return failure(QStringLiteral("CONFLICT"),
                               QStringLiteral("The catalog return plan contains an unavailable Color."));
            plan.rows.append({++rowIndex, 0, value.partNumber, value.partName,
                color->rebrickableId(), value.colorName, lego->name(), value.quantity, false});
        }
    }
    plan.planId = planHash(plan);
    result.success = true;
    return result;
}
