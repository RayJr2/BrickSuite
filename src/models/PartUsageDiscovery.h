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

struct PartUsageCriterion
{
    int partId = 0;
    QString partNumber;
    QString partName;
    int colorId = 0; // Zero means Any Color.
    QString colorName;
    int quantity = 1;
};

enum class PartUsageMatchMode { All, Any };

struct PartUsageSearch
{
    QList<PartUsageCriterion> criteria;
    QString text;
    PartUsageMatchMode matchMode = PartUsageMatchMode::All;
    int maximumResults = 250;
};

struct PartUsageSetResult
{
    int setCatalogId = 0;
    QString setNumber;
    QString name;
    int year = 0;
    int themeId = 0;
    QString themeName;
    QString imageUrl;
    int catalogPartCount = 0;
    int matchedCriteria = 0;
    int totalCriteria = 0;
};

struct PartUsageSearchResult
{
    bool success = false;
    QString errorMessage;
    QList<PartUsageSetResult> sets;
    bool capReached = false;
    int qualifyingCount = 0;
};
