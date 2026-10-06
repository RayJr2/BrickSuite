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

struct ProcurementItem
{
    int partId = 0;
    int colorId = 0;

    QString partNumber;
    QString partName;
    QString colorName;

    int quantityNeeded = 0;

    QString resolvedItemId;
    QString resolvedItemStatus;
    bool resolvedItemReady = false;

    QString resolvedColorId;
    QString resolvedColorStatus;
    bool resolvedColorReady = false;

    // Session-only preview overrides. These do not update provider mappings.
    QString itemOverride;
    bool itemOverrideActive = false;
    bool rememberItemOverride = false;

    QString colorOverrideId;
    QString colorOverrideName;

    QString effectiveItemId() const
    {
        return itemOverrideActive
                   ? itemOverride.trimmed()
                   : resolvedItemId.trimmed();
    }

    QString effectiveColorId() const
    {
        return colorOverrideId.trimmed().isEmpty()
                   ? resolvedColorId.trimmed()
                   : colorOverrideId.trimmed();
    }

    bool itemReady() const
    {
        if (itemOverrideActive)
            return !itemOverride.trimmed().isEmpty();

        return resolvedItemReady
               && !resolvedItemId.trimmed().isEmpty();
    }

    bool colorReady() const
    {
        return !effectiveColorId().isEmpty()
               && (resolvedColorReady || !colorOverrideId.trimmed().isEmpty());
    }

    bool ready() const
    {
        return quantityNeeded > 0 && itemReady() && colorReady();
    }
};

struct ProcurementDraft
{
    int workspaceId = 0;
    int buildId = 0;

    QString buildName;
    QString buildType;
    QString buildNumber;

    QList<ProcurementItem> items;

    int totalMissingPieces() const
    {
        int total = 0;

        for (const ProcurementItem& item : items)
            total += item.quantityNeeded;

        return total;
    }

    int readyRows() const
    {
        int total = 0;

        for (const ProcurementItem& item : items) {
            if (item.ready())
                ++total;
        }

        return total;
    }

    int reviewRows() const
    {
        return items.size() - readyRows();
    }
};
