#pragma once

#include "dto/RemoteReadDtos.h"
#include <QHash>

// A page-level decoration input: callers batch-load each distinct canonical
// identity once, then decorate rows without per-row repository queries.
struct LocalReferenceDecoration
{
    QHash<QString, QString> partNamesByNumber;
    QHash<int, QString> colorNamesByRebrickableId;

    void decorate(QList<RemoteReadDto::InventoryRow>& rows) const
    {
        for (auto& row : rows) {
            const auto part = partNamesByNumber.constFind(row.partNumber);
            if (part != partNamesByNumber.constEnd()) row.partNameFallback = part.value();
            const auto color = colorNamesByRebrickableId.constFind(row.rebrickableColorId);
            if (color != colorNamesByRebrickableId.constEnd()) row.colorNameFallback = color.value();
        }
    }
};
