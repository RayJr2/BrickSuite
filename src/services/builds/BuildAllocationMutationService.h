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

#pragma once

#include "../../models/BuildAllocation.h"

#include <QList>
#include <QSqlDatabase>
#include <QString>
#include <functional>

class BuildAllocationMutationService
{
public:
    enum class Error { None, InvalidInput, NotFound, InvalidState, DatabaseFailure };
    struct Result {
        bool success = false;
        Error error = Error::None;
        QString message;
        QList<BuildAllocation> allocations;
        QList<int> affectedRequirementIds;
        QList<int> affectedInventoryIds;
        int piecesAdded = 0;
        int allocationsCreated = 0;
        int allocationsUpdated = 0;
        int preferredPiecesAdded = 0;
        bool changed = false;
    };
    BuildAllocationMutationService();
    explicit BuildAllocationMutationService(const QSqlDatabase& database);
    Result replaceForRequirement(int requirementId, const QList<BuildAllocation>& allocations) const;
    Result replaceForRequirementInCurrentTransaction(int requirementId,
                                                      const QList<BuildAllocation>& allocations) const;
    Result allocateAvailable(int buildId, int preferredStorageLocationId = 0) const;
    Result allocateAvailableInCurrentTransaction(int buildId,
                                                 int preferredStorageLocationId = 0) const;
private:
    QSqlDatabase database() const;
    Result inTransaction(const std::function<Result()>& operation) const;
    QString m_connectionName;
};
