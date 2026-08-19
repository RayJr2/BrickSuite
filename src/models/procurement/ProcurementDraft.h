/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
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
    QString colorOverrideId;
    QString colorOverrideName;

    QString effectiveItemId() const
    {
        return itemOverride.trimmed().isEmpty()
                   ? resolvedItemId.trimmed()
                   : itemOverride.trimmed();
    }

    QString effectiveColorId() const
    {
        return colorOverrideId.trimmed().isEmpty()
                   ? resolvedColorId.trimmed()
                   : colorOverrideId.trimmed();
    }

    bool itemReady() const
    {
        return !effectiveItemId().isEmpty()
               && (resolvedItemReady || !itemOverride.trimmed().isEmpty());
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
