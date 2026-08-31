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

#include "InventoryImportTypes.h"

#include <QString>
#include <QStringList>

class QSqlDatabase;

class RebrickableInventoryImporter
{
public:

    explicit RebrickableInventoryImporter(QSqlDatabase& database);

    bool importOwnedParts(const QString& filePath,
                          const InventoryImportOptions& options,
                          InventoryImportResult& result);

    bool previewOwnedParts(const QString& filePath,
                           const InventoryImportOptions& options,
                           InventoryImportPreview& preview);

    bool importPreview(const InventoryImportPreview& preview,
                       const InventoryImportOptions& options,
                       InventoryImportResult& result);

private:
    QStringList parseCsvLine(const QString& line) const;

    QSqlDatabase& m_database;
};