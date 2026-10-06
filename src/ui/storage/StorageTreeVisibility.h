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

#include <QHash>
#include <QList>
#include <QSet>

namespace StorageTreeVisibility {

struct Row
{
    qint64 id = 0;
    qint64 parentId = 0;
    bool active = true;
};

inline QSet<qint64> visibleIds(const QList<Row>& rows, bool showInactive)
{
    QHash<qint64, Row> byId;
    for (const Row& row : rows) byId.insert(row.id, row);

    QSet<qint64> visible;
    for (const Row& row : rows) {
        bool display = showInactive || row.active;
        qint64 parentId = row.parentId;
        QSet<qint64> visited;
        while (display && parentId > 0) {
            if (visited.contains(parentId) || !byId.contains(parentId)) {
                display = false;
                break;
            }
            visited.insert(parentId);
            const Row parent = byId.value(parentId);
            if (!showInactive && !parent.active) display = false;
            parentId = parent.parentId;
        }
        if (display) visible.insert(row.id);
    }
    return visible;
}

} // namespace StorageTreeVisibility
