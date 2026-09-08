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
    QStringList details;
    details.append(QStringLiteral("%1 rows read").arg(counters.rowsRead));
    if (counters.inserted) details.append(QStringLiteral("%1 inserted").arg(counters.inserted));
    if (counters.updated) details.append(QStringLiteral("%1 updated").arg(counters.updated));
    if (counters.unchanged) details.append(QStringLiteral("%1 unchanged").arg(counters.unchanged));
    if (counters.reactivated) details.append(QStringLiteral("%1 reactivated").arg(counters.reactivated));
    if (counters.deactivated) details.append(QStringLiteral("%1 deactivated").arg(counters.deactivated));
    if (counters.selfReferencesIgnored)
        details.append(QStringLiteral("%1 self-reference ignored")
                           .arg(counters.selfReferencesIgnored));
    if (counters.replaced) details.append(QStringLiteral("%1 replaced").arg(counters.replaced));
    if (counters.preferredChanged)
        details.append(QStringLiteral("%1 preferred flags changed")
                           .arg(counters.preferredChanged));
    if (counters.setInventoryRows)
        details.append(QStringLiteral("%1 Set-inventory rows processed")
                           .arg(counters.setInventoryRows));
    if (counters.recognizedMinifigInventories)
        details.append(QStringLiteral("%1 Minifig inventories recognized")
                           .arg(counters.recognizedMinifigInventories));
    if (counters.minifigPartRows)
        details.append(QStringLiteral("%1 Minifig-owned Part rows processed")
                           .arg(counters.minifigPartRows));
    entry->message = QStringLiteral("%1 — %2.")
                         .arg(noChanges ? QStringLiteral("No changes")
                                        : QStringLiteral("Imported"),
                              details.join(QStringLiteral(", ")));
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
