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
