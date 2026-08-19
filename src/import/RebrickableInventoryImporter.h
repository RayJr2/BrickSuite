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

#include "RebrickableInventoryImportPreview.h"
#include "InventoryCsvOperation.h"

#include <QString>
#include <QStringList>

class QSqlDatabase;

class RebrickableInventoryImporter
{
public:
    struct ImportOptions
    {
        int workspaceId = 0;
        int storageLocationId = 0;

        QString condition = "Used";
        QString ownershipType = "Owned";

        InventoryCsvOperation operation = InventoryCsvOperation::Append;
    };

    struct ImportResult
    {
        int rowsProcessed = 0;
        int rowsImported = 0;
        int rowsFailed = 0;

        int totalQuantityImported = 0;
    };

    explicit RebrickableInventoryImporter(QSqlDatabase& database);

    bool importOwnedParts(const QString& filePath,
                          const ImportOptions& options,
                          ImportResult& result);

    bool previewOwnedParts(const QString& filePath,
                           const ImportOptions& options,
                           RebrickableInventoryImportPreview& preview);

    bool importPreview(const RebrickableInventoryImportPreview& preview,
                       const ImportOptions& options,
                       ImportResult& result);

private:
    QStringList parseCsvLine(const QString& line) const;

    QSqlDatabase& m_database;
};