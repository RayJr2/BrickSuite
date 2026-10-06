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
