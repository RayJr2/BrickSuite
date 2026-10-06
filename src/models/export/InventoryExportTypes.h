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
#include <QSet>
#include <QString>
#include <QStringList>

struct InventoryExportRow
{
    qint64 inventoryRecordId = 0;
    qint64 storageLocationId = 0;
    int partId = 0;
    int colorId = 0;
    int manufacturerId = 0;
    QString partNumber;
    QString partName;
    QString category;
    QString color;
    int quantity = 0;
    QString storagePath;
    QString manufacturer;
    QString condition;
    QString ownership;
    QStringList legoElementIds;
    QString rebrickablePartId;
    int rebrickableColorId = -1;
    QStringList brickLinkPartIds;
    QString brickLinkColorId;
};

struct InventoryExportFieldDescriptor
{
    QString id;
    QString label;
    QString header;
    bool defaultEnabled = false;
    int defaultOrder = 0;
    bool numeric = false;
};

struct InventoryExportConfiguration
{
    QStringList fieldOrder;
    QSet<QString> enabledFields;
};

struct InventoryExportProjection
{
    QList<InventoryExportFieldDescriptor> fields;
    QStringList headers;
    QList<QStringList> rows;
};
