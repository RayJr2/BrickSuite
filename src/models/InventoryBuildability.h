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

struct InventoryBuildabilityRequirement
{
    int partId = 0;
    int colorId = 0;
    int rebrickableColorId = -1;
    QString partNumber;
    QString partName;
    QString colorName;
    int required = 0;
    int looseAvailable = 0;
    int looseUsed = 0;
    int collectionUsed = 0;
    int missing = 0;
};

struct InventoryBuildabilitySource
{
    int collectionItemId = 0;
    QString label;
    QString state;
    int piecesUsed = 0;
};

struct InventoryBuildabilitySetResult
{
    int setCatalogId = 0;
    QString setNumber;
    QString name;
    int year = 0;
    int themeCatalogId = 0;
    int rebrickableThemeId = 0;
    QString themeName;
    QString imageUrl;
    int catalogPartCount = 0;
    int looseSatisfiedQuantity = 0;
    int totalQuantity = 0;
    int looseSatisfiedRequirements = 0;
    int totalRequirements = 0;
    int advisorySatisfiedQuantity = 0;
    int advisorySatisfiedRequirements = 0;
    int missingQuantity = 0;
    int collectionSourceCount = 0;
    QList<InventoryBuildabilityRequirement> requirements;
    QList<InventoryBuildabilitySource> sources;

    int loosePercent() const;
    int advisoryPercent() const;
    bool usesCollection() const { return collectionSourceCount > 0 || !sources.isEmpty(); }
};

struct InventoryBuildabilitySearch
{
    int workspaceId = 0;
    QString text;
    int minimumPercent = 75;
    int minimumSetParts = 25;
    int yearFrom = 0;
    int yearTo = 0;
    int themeCatalogId = 0;
    bool fullyBuildableOnly = false;
    bool includeCollection = true;
    bool fullyBuildableFirst = true;
    int maximumResults = 250;
    // Internal Host/local identity used only to constrain an exact detail evaluation.
    int exactSetCatalogId = 0;
};

struct InventoryBuildabilitySearchResult
{
    bool success = false;
    QString errorMessage;
    QList<InventoryBuildabilitySetResult> sets;
    bool capReached = false;
    int qualifyingCount = 0;
    int eligibleCollectionSources = 0;
    int dormantCollectionSources = 0;
    int candidateCountBeforeFilters = 0;
    int candidateCountAfterCatalogFilters = 0;
    int preciseEvaluationCount = 0;
};
