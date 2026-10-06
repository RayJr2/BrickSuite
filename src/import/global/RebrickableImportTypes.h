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

#include <QElapsedTimer>
#include <QString>
#include <QStringList>
#include <QVector>

enum class RebrickableDatasetId {
    Themes,
    Colors,
    PartCategories,
    Parts,
    PartRelationships,
    Sets,
    Minifigs,
    Elements,
    Inventories,
    InventoryParts,
    InventoryMinifigs,
    InventorySets
};

enum class RebrickableImportSourceType { None, Csv, Zip };

enum class RebrickableImportStatus {
    Missing,
    Ambiguous,
    Invalid,
    Ready,
    BlockedByDependency,
    NotImplemented,
    Queued,
    Importing,
    Imported,
    NoChanges,
    Failed,
    Skipped,
    Cancelled
};

enum class RebrickableValidationSeverity { Warning, UnresolvedReference, RowFailure, Fatal };

struct RebrickableValidationIssue {
    RebrickableValidationSeverity severity = RebrickableValidationSeverity::Warning;
    QString message;
    qint64 row = -1;
};

struct RebrickableImportCounters {
    qint64 rowsRead = 0;
    qint64 inserted = 0;
    qint64 updated = 0;
    qint64 unchanged = 0;
    qint64 skipped = 0;
    qint64 deactivated = 0;
    qint64 unresolved = 0;
    qint64 reactivated = 0;
    qint64 selfReferencesIgnored = 0;
    qint64 replaced = 0;
    qint64 preferredChanged = 0;
    qint64 setInventoryRows = 0;
    qint64 recognizedMinifigInventories = 0;
    qint64 minifigPartRows = 0;
};

struct RebrickableImportPlanEntry {
    RebrickableDatasetId dataset = RebrickableDatasetId::Themes;
    QString displayName;
    QString sourcePath;
    qint64 sourceSize = -1;
    qint64 sourceModifiedMilliseconds = -1;
    QStringList conflictingSourcePaths;
    RebrickableImportSourceType sourceType = RebrickableImportSourceType::None;
    RebrickableImportStatus status = RebrickableImportStatus::Missing;
    QString message;
    qint64 rowTotalHint = -1;
    qint64 elapsedMilliseconds = 0;
    QVector<RebrickableValidationIssue> issues;
    RebrickableImportCounters counters;
};

struct RebrickableImportPlan {
    QString sourceDirectory;
    QVector<RebrickableImportPlanEntry> entries;
};

struct RebrickableImportProgress {
    RebrickableDatasetId dataset = RebrickableDatasetId::Themes;
    int datasetIndex = 0;
    int datasetTotal = 0;
    qint64 currentRow = -1;
    qint64 totalRows = -1;
    QString phase;
    QString message;
    qint64 elapsedMilliseconds = 0;
    RebrickableImportCounters counters;
};

QString rebrickableImportStatusText(RebrickableImportStatus status);
