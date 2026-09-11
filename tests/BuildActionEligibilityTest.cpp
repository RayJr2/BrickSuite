#include "../src/ui/builds/BuildActionEligibility.h"

#include <QCoreApplication>
#include <QDebug>

namespace {

bool check(bool condition, const char* description)
{
    if (!condition)
        qCritical() << "FAILED:" << description;

    return condition;
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication application(argc, argv);
    bool ok = true;

    ok &= check(BuildActionEligibility::supportsStockFulfillment(
                    true, QStringLiteral("Stock"), QStringLiteral("Planned")),
                "active planned Build from Stock is eligible");
    ok &= check(BuildActionEligibility::supportsStockFulfillment(
                    true, QStringLiteral("Stock"), QStringLiteral("Pulling")),
                "active pulling Build from Stock is eligible");
    ok &= check(!BuildActionEligibility::supportsStockFulfillment(
                    true, QStringLiteral("Stock"), QStringLiteral("Complete")),
                "completed Build from Stock is ineligible");
    ok &= check(!BuildActionEligibility::supportsStockFulfillment(
                    true, QStringLiteral("Stock"), QStringLiteral("Disassembled")),
                "disassembled Build from Stock is ineligible");
    ok &= check(!BuildActionEligibility::supportsStockFulfillment(
                    true, QStringLiteral("Stock"), QStringLiteral("Cancelled")),
                "cancelled Build from Stock is ineligible");
    ok &= check(!BuildActionEligibility::supportsStockFulfillment(
                    true, QStringLiteral("CompleteSet"), QStringLiteral("Planned")),
                "planned Complete Set is ineligible");
    ok &= check(!BuildActionEligibility::supportsStockFulfillment(
                    true, QStringLiteral("CompleteSet"), QStringLiteral("Complete")),
                "completed Complete Set is ineligible");
    ok &= check(!BuildActionEligibility::supportsStockFulfillment(
                    false, QStringLiteral("Stock"), QStringLiteral("Planned")),
                "inactive Build from Stock is ineligible");

    ok &= check(!BuildActionEligibility::canSubmitRequirement(true, false, true, true),
                "empty or unresolved Part disables Add Requirement");
    ok &= check(BuildActionEligibility::canSubmitRequirement(true, true, true, true),
                "valid Part, Color, quantity, and workflow enable Add Requirement");
    ok &= check(!BuildActionEligibility::canSubmitRequirement(true, true, false, true),
                "invalid Color disables Add Requirement");
    ok &= check(!BuildActionEligibility::canSubmitRequirement(true, true, true, false),
                "invalid quantity disables Add Requirement");
    ok &= check(!BuildActionEligibility::canSubmitRequirement(false, true, true, true),
                "capability, pending request, or lifecycle denial disables Add Requirement");

    const bool screenshotPartResolvable = true;
    const bool screenshotAquaLocalColorIdValid = true;
    const bool screenshotQuantityValid = true;
    ok &= check(BuildActionEligibility::canSubmitRequirement(
                    true, screenshotPartResolvable,
                    screenshotAquaLocalColorIdValid, screenshotQuantityValid),
                "local Planned Stock Build accepts resolved descriptive Part selection");
    const auto remoteScreenshotWorkflow =
        BuildActionEligibility::remoteRequirementActions(
            true, true, false, true, false, false, false, false);
    ok &= check(BuildActionEligibility::canSubmitRequirement(
                    remoteScreenshotWorkflow.canAdd, screenshotPartResolvable,
                    screenshotAquaLocalColorIdValid, screenshotQuantityValid),
                "remote Planned Stock Build with Add capability accepts resolved selection");
    const auto remoteMissingCapability =
        BuildActionEligibility::remoteRequirementActions(
            true, true, false, false, false, false, false, false);
    ok &= check(!BuildActionEligibility::canSubmitRequirement(
                    remoteMissingCapability.canAdd, screenshotPartResolvable,
                    screenshotAquaLocalColorIdValid, screenshotQuantityValid),
                "remote selection remains disabled without Add capability");
    const auto remotePending =
        BuildActionEligibility::remoteRequirementActions(
            true, true, true, true, false, false, false, false);
    ok &= check(!BuildActionEligibility::canSubmitRequirement(
                    remotePending.canAdd, screenshotPartResolvable,
                    screenshotAquaLocalColorIdValid, screenshotQuantityValid),
                "remote selection remains disabled while Add is pending");

    const auto full = BuildActionEligibility::remoteRequirementActions(
        true, true, false, true, true, true, true, true);
    ok &= check(full.canAdd && full.canEdit && full.canRemove
                    && full.canSetAllocations && full.canAllocateAvailable,
                "full G2 capabilities enable every eligible action");
    const auto noAdd = BuildActionEligibility::remoteRequirementActions(
        true, true, false, false, true, true, true, true);
    ok &= check(!noAdd.canAdd && noAdd.canEdit && noAdd.canRemove
                    && noAdd.canSetAllocations && noAdd.canAllocateAvailable,
                "missing Add disables only Add");
    const auto noEdit = BuildActionEligibility::remoteRequirementActions(
        true, true, false, true, false, true, true, true);
    ok &= check(noEdit.canAdd && !noEdit.canEdit && noEdit.canRemove
                    && noEdit.canSetAllocations && noEdit.canAllocateAvailable,
                "missing Edit disables only Edit");
    const auto noRemove = BuildActionEligibility::remoteRequirementActions(
        true, true, false, true, true, false, true, true);
    ok &= check(noRemove.canAdd && noRemove.canEdit && !noRemove.canRemove
                    && noRemove.canSetAllocations && noRemove.canAllocateAvailable,
                "missing Remove disables only Remove");
    const auto noSetAllocations = BuildActionEligibility::remoteRequirementActions(
        true, true, false, true, true, true, false, true);
    ok &= check(noSetAllocations.canAdd && noSetAllocations.canEdit
                    && noSetAllocations.canRemove && !noSetAllocations.canSetAllocations
                    && noSetAllocations.canAllocateAvailable,
                "missing allocation-set disables only Manage Allocation");
    const auto noAllocateAvailable = BuildActionEligibility::remoteRequirementActions(
        true, true, false, true, true, true, true, false);
    ok &= check(noAllocateAvailable.canAdd && noAllocateAvailable.canEdit
                    && noAllocateAvailable.canRemove
                    && noAllocateAvailable.canSetAllocations
                    && !noAllocateAvailable.canAllocateAvailable,
                "missing Allocate Available disables only that action");
    const auto noWrites = BuildActionEligibility::remoteRequirementActions(
        true, true, false, false, false, false, false, false);
    ok &= check(!noWrites.canAdd && !noWrites.canEdit && !noWrites.canRemove
                    && !noWrites.canSetAllocations && !noWrites.canAllocateAvailable,
                "older Host remains read-only");
    const auto stale = BuildActionEligibility::remoteRequirementActions(
        false, true, false, true, true, true, true, true);
    ok &= check(!stale.canAdd && !stale.canEdit && !stale.canRemove
                    && !stale.canSetAllocations && !stale.canAllocateAvailable,
                "obsolete session disables every remote action");
    const auto invalidLifecycle = BuildActionEligibility::remoteRequirementActions(
        true, false, false, true, true, true, true, true);
    ok &= check(!invalidLifecycle.canAdd && !invalidLifecycle.canEdit
                    && !invalidLifecycle.canRemove
                    && !invalidLifecycle.canSetAllocations
                    && !invalidLifecycle.canAllocateAvailable,
                "ineligible Build lifecycle disables every remote action");
    const auto allocated = BuildActionEligibility::remoteRequirementActions(
        true, true, false, true, true, true, true, true, false, false, true);
    ok &= check(allocated.canEdit && !allocated.canRemove && allocated.canSetAllocations,
                "allocated requirement can be managed but not removed");
    const auto pulled = BuildActionEligibility::remoteRequirementActions(
        true, true, false, true, true, true, true, true, false, true, true);
    ok &= check(pulled.canEdit && !pulled.canRemove && !pulled.canSetAllocations,
                "pulled or released requirement preserves committed allocation state");

    return ok ? 0 : 1;
}
