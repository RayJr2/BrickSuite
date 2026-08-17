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

#include <QList>
#include <QString>

struct RebrickableInventoryImportPreviewRow
{
    QString partNumber;
    QString partName;

    int partId = 0;

    int rebrickableColorId = 0;
    int colorId = 0;
    QString colorName;

    int csvQuantity = 0;
    int currentQuantity = 0;
    int resultingQuantity = 0;

    QString status;
    QString errorMessage;
};

struct RebrickableInventoryImportPreview
{
    QString sourceFilePath;
    QString sourceFileName;

    int rowsProcessed = 0;
    int validRows = 0;
    int failedRows = 0;

    int totalCsvQuantity = 0;

    QList<RebrickableInventoryImportPreviewRow> rows;
};
