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

#include <QString>

namespace BuildActionEligibility {

inline bool supportsStockFulfillment(bool active,
                                     const QString& inventoryMode,
                                     const QString& status)
{
    return active
           && inventoryMode == QStringLiteral("Stock")
           && (status == QStringLiteral("Planned")
               || status == QStringLiteral("Pulling"));
}

inline bool canSubmitRequirement(bool workflowEligible,
                                 bool partResolvable,
                                 bool colorValid,
                                 bool quantityValid)
{
    return workflowEligible && partResolvable && colorValid && quantityValid;
}

struct RemoteRequirementActions
{
    bool canAdd = false;
    bool canEdit = false;
    bool canRemove = false;
    bool canSetAllocations = false;
    bool canAllocateAvailable = false;
};

inline RemoteRequirementActions remoteRequirementActions(
    bool sessionCurrent, bool buildEligible, bool requestPending,
    bool addCapability, bool editCapability, bool removeCapability,
    bool setAllocationsCapability, bool allocateAvailableCapability,
    bool requirementSpare = false, bool requirementHasPulledOrReleased = false,
    bool requirementHasAllocation = false)
{
    const bool writable = sessionCurrent && buildEligible && !requestPending;
    return {
        writable && addCapability,
        writable && editCapability,
        writable && removeCapability && !requirementHasPulledOrReleased
            && !requirementHasAllocation,
        writable && setAllocationsCapability && !requirementSpare
            && !requirementHasPulledOrReleased,
        writable && allocateAvailableCapability
    };
}

} // namespace BuildActionEligibility
