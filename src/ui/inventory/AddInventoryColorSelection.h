#pragma once

#include "../../models/Color.h"

#include <QList>

namespace AddInventoryColorSelection
{
inline QList<Color> visibleColors(const QList<Color>& colors,
                                  const QList<int>& knownRebrickableColorIds,
                                  bool showAllColors)
{
    if (showAllColors)
        return colors;

    QList<Color> visible;
    for (const Color& color : colors) {
        if (knownRebrickableColorIds.contains(color.rebrickableId()))
            visible.append(color);
    }
    return visible;
}

inline int retainedColorId(const QList<Color>& visibleColors, int preferredColorId)
{
    if (preferredColorId <= 0)
        return 0;

    for (const Color& color : visibleColors) {
        if (color.id() == preferredColorId)
            return preferredColorId;
    }
    return 0;
}
}
