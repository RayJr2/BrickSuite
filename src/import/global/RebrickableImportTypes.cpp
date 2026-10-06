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

