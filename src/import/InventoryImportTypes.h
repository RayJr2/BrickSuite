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
 */

#pragma once

#include "InventoryCsvOperation.h"

#include <QList>
#include <QString>

enum class InventoryImportSource
{
    Unknown = 0,
    RebrickableCsv,
    BrickOwlOrderCsv
};

inline QString inventoryImportSourceName(InventoryImportSource source)
{
    switch (source) {
    case InventoryImportSource::RebrickableCsv:
        return QStringLiteral("Rebrickable CSV");
    case InventoryImportSource::BrickOwlOrderCsv:
        return QStringLiteral("BrickOwl Order CSV");
    case InventoryImportSource::Unknown:
    default:
        return QStringLiteral("Unknown");
    }
}

struct InventoryImportOptions
{
    int workspaceId = 0;
    int storageLocationId = 0;

    QString condition = QStringLiteral("Used");
    QString ownershipType = QStringLiteral("Owned");

    InventoryCsvOperation operation = InventoryCsvOperation::Append;
};

struct InventoryImportResult
{
    int rowsProcessed = 0;
    int rowsImported = 0;
    int rowsFailed = 0;

    int totalQuantityImported = 0;
};

struct InventoryImportPreviewRow
{
    // Provider/source data. Parsers fill these fields before/while resolving
    // the row into BrickSuite canonical part/color identities.
    QString sourcePartNumber;
    QString sourceColorIdentifier;
    QString sourceColorName;
    QString sourceCondition;
    QString sourceOrderId;
    QString sourceLotId;
    QString sourceBoid;

    int sourceOrderedQuantity = 0;
    int sourceRefundedQuantity = 0;

    // Canonical BrickSuite catalog identity used by preview/commit.
    QString partNumber;
    QString partName;
    int partId = 0;

    int colorId = 0;
    QString colorName;

    // BrickSuite currently seeds its canonical colors from Rebrickable.
    // Retain the resolved Rebrickable color ID as export metadata so the
    // existing Rebrickable Compare/diff workflow remains provider-neutral
    // at the preview layer while its writer can still export RB CSV.
    int rebrickableColorId = 0;

    int inventoryRecordId = 0;

    bool presentInSource = false;
    bool presentInBrickSuite = false;

    int sourceQuantity = 0;
    int currentQuantity = 0;
    int resultingQuantity = 0;
    int difference = 0;

    QString status;
    QString errorMessage;
};

struct InventoryImportPreview
{
    InventoryImportSource source = InventoryImportSource::Unknown;

    QString sourceFilePath;
    QString sourceFileName;
    QString sourcePartListName;
    QString storageDisplayName;

    InventoryCsvOperation operation = InventoryCsvOperation::Append;

    int rowsProcessed = 0;
    int validRows = 0;
    int failedRows = 0;

    int totalSourceQuantity = 0;

    QList<InventoryImportPreviewRow> rows;
};
