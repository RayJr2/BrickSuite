/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 *
 * This file is part of BrickSuite.
 *
 * BrickSuite is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, version 3 of the License.
 *
 * BrickSuite is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with BrickSuite. If not, see <https://www.gnu.org/licenses/>.
 */

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
