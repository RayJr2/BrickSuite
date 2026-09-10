#include "HostPullingMutationService.h"

#include "../builds/BuildPullingService.h"
#include "../../models/Build.h"
#include "../../models/BuildAllocation.h"
#include "../../models/BuildRequirement.h"
#include "../../models/InventoryRecord.h"
#include "../../repositories/BuildRepository.h"
#include "../../repositories/BuildAllocationRepository.h"
#include "../../repositories/BuildRequirementRepository.h"
#include "../../repositories/InventoryRecordRepository.h"

#include <QJsonArray>
#include <QSet>
#include <limits>

namespace {
HostWriteExecutor::MutationOutcome conflict(const QString& message)
{
    HostWriteExecutor::MutationOutcome result;
    result.error = {QStringLiteral("CONFLICT"), message, false};
    result.error.conflict = QJsonObject{{QStringLiteral("refreshRequired"), true}};
    return result;
}
}

HostWriteExecutor::Mutation HostPullingMutationService::createMutation(
    const RemoteMutationDto::Metadata& metadata, RemoteMutationDto::Error* error)
{
    RemotePullingMutationDto::Request request;
    if (!RemotePullingMutationDto::fromMetadata(metadata, &request, error)) return {};

    return [request](const QSqlDatabase& database) {
        BuildRepository builds(database);
        BuildAllocationRepository allocations(database);
        BuildRequirementRepository requirements(database);
        InventoryRecordRepository inventory(database);

        const auto build = builds.getById(int(request.buildId));
        if (!build || build->workspaceId() != request.workspaceId)
            return conflict(QStringLiteral("The Build is not available in the selected Workspace."));
        if (!build->isActive() || build->inventoryMode() != QStringLiteral("Stock")
            || (build->status() != QStringLiteral("Planned")
                && build->status() != QStringLiteral("Pulling")))
            return conflict(QStringLiteral("The Build is no longer eligible for Pulling."));
        if (!request.expectedBuildStatus.isEmpty()
            && request.expectedBuildStatus != build->status())
            return conflict(QStringLiteral("The Build status changed on the Host."));

        QHash<int, int> expectedRequirementPulled;
        QHash<int, qint64> collectiveDelta;
        QList<BuildPullingService::PullRequest> pulls;
        QJsonArray affectedAllocations;
        for (const auto& row : request.rows) {
            const auto allocation = allocations.getById(int(row.allocationId));
            if (!allocation || allocation->buildId() != build->id())
                return conflict(QStringLiteral("A Build allocation changed or belongs to another Build."));
            const auto requirement = requirements.getById(allocation->buildRequirementId());
            const auto record = inventory.getById(allocation->inventoryRecordId());
            if (!requirement || requirement->buildId() != build->id() || requirement->isSpare()
                || !record || record->workspaceId() != request.workspaceId
                || requirement->effectivePartId() != allocation->partId()
                || requirement->effectiveColorId() != allocation->colorId()
                || record->partId() != allocation->partId()
                || record->colorId() != allocation->colorId()
                || record->storageLocationId() != allocation->storageLocationId())
                return conflict(QStringLiteral("A Pulling source or requirement changed on the Host."));
            if (allocation->quantityAllocated() != row.expectedAllocatedQuantity
                || record->quantity() != row.expectedInventoryQuantity)
                return conflict(QStringLiteral("Allocation or Inventory quantities changed on the Host."));
            const int requirementId = requirement->id();
            if (expectedRequirementPulled.contains(requirementId)
                && expectedRequirementPulled.value(requirementId)
                       != row.expectedRequirementPulledQuantity)
                return conflict(QStringLiteral("Submitted rows disagree about requirement state."));
            expectedRequirementPulled.insert(requirementId, row.expectedRequirementPulledQuantity);
            if (requirement->quantityPulled() != row.expectedRequirementPulledQuantity)
                return conflict(QStringLiteral("Requirement Pulling progress changed on the Host."));
            if (row.quantity > allocation->quantityAllocated()
                || row.quantity > record->quantity())
                return conflict(QStringLiteral("The requested quantity is no longer available."));
            collectiveDelta[requirementId] += row.quantity;
            if (collectiveDelta.value(requirementId)
                > qint64(requirement->quantityRequired()) - requirement->quantityPulled())
                return conflict(QStringLiteral("The requested quantity exceeds the remaining requirement."));
            pulls.append({int(row.allocationId), row.quantity});
            affectedAllocations.append(row.allocationId);
        }

        BuildPullingService pulling(database);
        const auto pulled = pulling.recordPullsInCurrentTransaction(pulls);
        if (!pulled.success) {
            HostWriteExecutor::MutationOutcome failed;
            failed.error = {QStringLiteral("CONFLICT"), pulled.message, false};
            return failed;
        }

        QJsonObject totals;
        for (auto it = expectedRequirementPulled.cbegin(); it != expectedRequirementPulled.cend(); ++it) {
            const auto requirement = requirements.getById(it.key());
            if (requirement) totals.insert(QString::number(it.key()), requirement->quantityPulled());
        }
        HostWriteExecutor::MutationOutcome outcome;
        outcome.success = true;
        outcome.authoritative = QJsonObject{{"buildId", request.buildId},
            {"submittedRows", pulled.rowsPulled}, {"piecesRecorded", pulled.piecesPulled},
            {"allocationIds", affectedAllocations}, {"requirementPulledTotals", totals},
            {"buildStatus", build->status()}};
        outcome.publicationWorkflow = HostMutationPublicationService::Workflow::Pulling;
        outcome.publicationScope.workspaceId = request.workspaceId;
        outcome.publicationScope.buildId = request.buildId;
        return outcome;
    };
}
