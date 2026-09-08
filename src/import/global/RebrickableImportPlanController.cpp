#include "RebrickableImportPlanController.h"

#include "RebrickableDatasetRegistry.h"

#include <QSet>

namespace {
RebrickableImportPlanEntry* findEntry(RebrickableImportPlan& plan, RebrickableDatasetId dataset)
{
    for (auto& entry : plan.entries)
        if (entry.dataset == dataset)
            return &entry;
    return nullptr;
}

void blockDependents(RebrickableImportPlan& plan, RebrickableDatasetId failed)
{
    QSet<RebrickableDatasetId> unavailable{failed};
    bool changed = true;
    while (changed) {
        changed = false;
        for (auto& entry : plan.entries) {
            if (unavailable.contains(entry.dataset))
                continue;
            const auto* descriptor = RebrickableDatasetRegistry::descriptor(entry.dataset);
            if (!descriptor)
                continue;
            for (const auto dependency : descriptor->hardDependencies) {
                if (!unavailable.contains(dependency))
                    continue;
                unavailable.insert(entry.dataset);
                if (entry.status == RebrickableImportStatus::Ready
                    || entry.status == RebrickableImportStatus::Queued) {
                    entry.status = RebrickableImportStatus::BlockedByDependency;
                    const auto* failedDescriptor =
                        RebrickableDatasetRegistry::descriptor(dependency);
                    entry.message = QStringLiteral("Blocked because %1 did not complete.")
                                        .arg(failedDescriptor ? failedDescriptor->displayName
                                                              : QStringLiteral("a dependency"));
                }
                changed = true;
                break;
            }
        }
    }
}
}

bool RebrickableImportPlanController::beginDataset(RebrickableImportPlan& plan,
                                                    RebrickableDatasetId dataset)
{
    auto* entry = findEntry(plan, dataset);
    if (!entry || (entry->status != RebrickableImportStatus::Ready
                   && entry->status != RebrickableImportStatus::Queued))
        return false;
    entry->status = RebrickableImportStatus::Importing;
    entry->message = QStringLiteral("Importing in one dataset transaction.");
    return true;
}

void RebrickableImportPlanController::completeDataset(
    RebrickableImportPlan& plan, RebrickableDatasetId dataset,
    const RebrickableImportCounters& counters, qint64 elapsedMilliseconds, bool noChanges)
{
    auto* entry = findEntry(plan, dataset);
    if (!entry)
        return;
    entry->counters = counters;
    entry->elapsedMilliseconds = elapsedMilliseconds;
    entry->status = noChanges ? RebrickableImportStatus::NoChanges
                              : RebrickableImportStatus::Imported;
    entry->message = noChanges ? QStringLiteral("Import completed with no changes.")
                               : QStringLiteral("Import transaction committed.");
}

void RebrickableImportPlanController::failDataset(RebrickableImportPlan& plan,
                                                  RebrickableDatasetId dataset,
                                                  const QString& message,
                                                  bool cancelled)
{
    auto* entry = findEntry(plan, dataset);
    if (!entry)
        return;
    entry->status = cancelled ? RebrickableImportStatus::Cancelled
                              : RebrickableImportStatus::Failed;
    entry->message = message;
    blockDependents(plan, dataset);
}

