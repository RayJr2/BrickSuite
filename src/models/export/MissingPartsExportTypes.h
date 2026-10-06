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
#include <QtGlobal>

enum class MissingPartsExportField
{
    Build,
    BuildReference,
    PartNumber,
    PartName,
    Category,
    Color,
    QuantityMissing,
    Manufacturer,
    LegoElementId,
    RebrickablePartId,
    RebrickableColorId,
    BrickLinkPartId,
    BrickLinkColorId,
    Required,
    Pulled,
    Remaining,
    Available
};

struct MissingPartsExportFieldDescriptor
{
    MissingPartsExportField field;
    QString id;
    QString displayName;
    QString csvHeader;
    bool defaultEnabled = false;
    int defaultOrder = 0;
    bool ambiguous = false;
    bool numeric = false;
};

struct MissingPartsExportRow
{
    QString buildName;
    QString buildReference;
    int partId = 0;
    int colorId = 0;
    QString partNumber;
    QString partName;
    QString category;
    QString colorName;
    int missing = 0;
    QString manufacturer;
    QStringList legoElementIds;
    QStringList pickABrickElementCandidates;
    QString rebrickablePartId;
    int rebrickableColorId = -1;
    QStringList brickLinkPartIds;
    QString brickLinkColorId;
    int required = 0;
    int pulled = 0;
    int remaining = 0;
    int available = 0;
};

struct MissingPartsExportConfiguration
{
    QStringList fieldOrder;
    QSet<QString> enabledFields;
};

struct MissingPartsExportProjection
{
    QList<MissingPartsExportFieldDescriptor> fields;
    QStringList headers;
    QList<QStringList> rows;
};

struct PickABrickExportSourceRow
{
    MissingPartsExportRow source;
    bool included = true;
    QStringList elementCandidates;
    QString selectedElementId;
    QString overridePartNumber;
    QString overridePartName;
    enum class OverrideState { None, Resolving, Ready, PartNotFound, NoElement, Unavailable, Failed };
    OverrideState overrideState = OverrideState::None;
    quint64 resolutionGeneration = 0;

    bool hasPartOverride() const { return !overridePartNumber.isEmpty(); }
};

struct PickABrickPartResolution
{
    bool serviceAvailable = true;
    bool partFound = false;
    QString partNumber;
    QString partName;
    QStringList elementCandidates;
};

struct PickABrickExportRow
{
    QString elementId;
    qint64 quantity = 0;
};

struct PickABrickExportProjection
{
    QList<PickABrickExportRow> rows;
    int includedSourceRows = 0;
    int unresolvedIncludedRows = 0;
    int excludedSourceRows = 0;
    qint64 includedPieces = 0;
    qint64 excludedPieces = 0;
    QString error;

    bool ready() const
    {
        return error.isEmpty() && includedSourceRows > 0 && unresolvedIncludedRows == 0;
    }
};
