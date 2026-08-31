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

#include "InventoryImportTypes.h"

#include <QString>
#include <QStringList>

class QSqlDatabase;

class BrickOwlInventoryImporter
{
public:
    explicit BrickOwlInventoryImporter(QSqlDatabase& database);

    bool previewOrder(const QString& filePath,
                      const InventoryImportOptions& options,
                      InventoryImportPreview& preview);

    bool importPreview(const InventoryImportPreview& preview,
                       const InventoryImportOptions& options,
                       InventoryImportResult& result);

private:
    QStringList parseCsvLine(const QString& line) const;
    QStringList extractPartCandidates(const QString& name) const;

    bool resolvePart(const QStringList& candidates,
                     InventoryImportPreviewRow& row) const;

    bool resolveColor(const QString& brickOwlColorName,
                      InventoryImportPreviewRow& row) const;

    QSqlDatabase& m_database;
};
