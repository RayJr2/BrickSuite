#include "RebrickableImportTypes.h"

QString rebrickableImportStatusText(RebrickableImportStatus status)
{
    switch (status) {
    case RebrickableImportStatus::Missing: return QStringLiteral("Missing");
    case RebrickableImportStatus::Ambiguous: return QStringLiteral("Ambiguous");
    case RebrickableImportStatus::Invalid: return QStringLiteral("Invalid");
    case RebrickableImportStatus::Ready: return QStringLiteral("Ready");
    case RebrickableImportStatus::BlockedByDependency: return QStringLiteral("Blocked by dependency");
    case RebrickableImportStatus::NotImplemented: return QStringLiteral("Not implemented");
    case RebrickableImportStatus::Queued: return QStringLiteral("Queued");
    case RebrickableImportStatus::Importing: return QStringLiteral("Importing");
    case RebrickableImportStatus::Imported: return QStringLiteral("Imported");
    case RebrickableImportStatus::NoChanges: return QStringLiteral("No changes");
    case RebrickableImportStatus::Failed: return QStringLiteral("Failed");
    case RebrickableImportStatus::Skipped: return QStringLiteral("Skipped");
    case RebrickableImportStatus::Cancelled: return QStringLiteral("Cancelled");
    }
    return QStringLiteral("Unknown");
}

