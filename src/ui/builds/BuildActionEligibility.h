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
