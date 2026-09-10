#include "BuildRequirementAvailabilityService.h"

#include "../../database/DatabaseManager.h"
#include "../../models/BuildRequirement.h"
#include "../../repositories/BuildAllocationRepository.h"
#include "../../repositories/InventoryRecordRepository.h"

#include <QSqlDatabase>
#include <QThread>

BuildRequirementAvailabilityService::BuildRequirementAvailabilityService()
    : BuildRequirementAvailabilityService(DatabaseManager::instance().database()) {}

BuildRequirementAvailabilityService::BuildRequirementAvailabilityService(
    const QSqlDatabase& database)
    : m_connectionName(database.connectionName()), m_ownerThread(QThread::currentThread())
{
    Q_ASSERT(database.isValid());
}

QSqlDatabase BuildRequirementAvailabilityService::serviceDatabase() const
{
    Q_ASSERT(QThread::currentThread() == m_ownerThread);
    return QSqlDatabase::database(m_connectionName, false);
}

BuildRequirementAvailabilityService::Projection
BuildRequirementAvailabilityService::project(int workspaceId,
                                               const BuildRequirement& requirement) const
{
    InventoryRecordRepository inventory(serviceDatabase());
    BuildAllocationRepository allocations(serviceDatabase());
    Projection result;
    result.owned = inventory.totalQuantityForPartColor(
        workspaceId, requirement.effectivePartId(), requirement.effectiveColorId());
    const int totalAllocated = allocations.totalAllocatedForPartColor(
        workspaceId, requirement.effectivePartId(), requirement.effectiveColorId());
    result.thisRequirementAllocated = allocations.totalAllocatedForRequirement(requirement.id());
    result.otherAllocated = qMax(totalAllocated - result.thisRequirementAllocated, 0);
    result.available = qMax(result.owned - totalAllocated, 0);
    const int remaining = qMax(requirement.quantityRequired() - requirement.quantityPulled(), 0);
    result.missing = requirement.isSpare()
        ? 0 : qMax(remaining - result.thisRequirementAllocated - result.available, 0);
    return result;
}
