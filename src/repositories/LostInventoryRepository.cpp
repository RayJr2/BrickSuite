#include "LostInventoryRepository.h"

#include "../database/DatabaseManager.h"

#include <QDateTime>
#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>

namespace {

LostInventoryItem itemFromQuery(const QSqlQuery& query)
{
    LostInventoryItem item;

    item.workspaceId = query.value("workspace_id").toInt();

    item.partId = query.value("part_id").toInt();

    item.colorId = query.value("color_id").toInt();

    item.partNumber = query.value("part_number").toString();

    item.partName = query.value("part_name").toString();

    item.colorName = query.value("color_name").toString();

    item.outstandingQuantity = query.value("outstanding_quantity").toInt();

    if (!query.value("last_storage_location_id").isNull()) {
        item.lastStorageLocationId = query.value("last_storage_location_id").toInt();
    }

    item.condition = query.value("condition").toString();

    item.ownershipType = query.value("ownership_type").toString();

    item.lastLostUtc = QDateTime::fromString(query.value("last_lost_utc").toString(),
                                             Qt::ISODateWithMs);

    return item;
}

QString baseSql()
{
    return R"(
        SELECT
            totals.workspace_id,
            totals.part_id,
            totals.color_id,

            p.part_number,
            p.name AS part_name,

            c.name AS color_name,

            totals.outstanding_quantity,

            last_lost.from_storage_location_id
                AS last_storage_location_id,

            last_lost.condition,
            last_lost.ownership_type,

            last_lost.created_utc
                AS last_lost_utc

        FROM
        (
            SELECT
                workspace_id,
                part_id,
                color_id,

                SUM(
                    CASE
                        WHEN movement_type = 'Lost'
                            THEN -quantity_change

                        WHEN movement_type = 'Found'
                            THEN -quantity_change

                        ELSE 0
                    END
                ) AS outstanding_quantity

            FROM inventory_movement

            WHERE movement_type IN
                ('Lost', 'Found')

            GROUP BY
                workspace_id,
                part_id,
                color_id

            HAVING
                SUM(
                    CASE
                        WHEN movement_type = 'Lost'
                            THEN -quantity_change

                        WHEN movement_type = 'Found'
                            THEN -quantity_change

                        ELSE 0
                    END
                ) > 0
        ) totals

        INNER JOIN part p
            ON p.id = totals.part_id

        INNER JOIN color c
            ON c.id = totals.color_id

        LEFT JOIN inventory_movement last_lost
            ON last_lost.id =
            (
                SELECT im2.id

                FROM inventory_movement im2

                WHERE
                    im2.workspace_id =
                        totals.workspace_id

                    AND im2.part_id =
                        totals.part_id

                    AND im2.color_id =
                        totals.color_id

                    AND im2.movement_type =
                        'Lost'

                ORDER BY
                    im2.created_utc DESC,
                    im2.id DESC

                LIMIT 1
            )
    )";
}

} // namespace

QList<LostInventoryItem> LostInventoryRepository::getOutstanding(int workspaceId) const
{
    QList<LostInventoryItem> results;

    if (workspaceId <= 0)
        return results;

    QSqlDatabase database = DatabaseManager::instance().database();

    QSqlQuery query(database);

    const QString sql = baseSql() +
                        R"(
            WHERE totals.workspace_id =
                :workspace_id

            ORDER BY
                p.part_number,
                c.name
        )";

    query.prepare(sql);

    query.bindValue(":workspace_id", workspaceId);

    if (!query.exec()) {
        qCritical() << "Unable to retrieve outstanding "
                       "lost inventory:"
                    << query.lastError().text();

        return results;
    }

    while (query.next()) {
        results.append(itemFromQuery(query));
    }

    return results;
}

std::optional<LostInventoryItem> LostInventoryRepository::getOutstandingForPartColor(
    int workspaceId, int partId, int colorId) const
{
    if (workspaceId <= 0 || partId <= 0 || colorId <= 0) {
        return std::nullopt;
    }

    QSqlDatabase database = DatabaseManager::instance().database();

    QSqlQuery query(database);

    const QString sql = baseSql() +
                        R"(
            WHERE totals.workspace_id =
                :workspace_id

              AND totals.part_id =
                :part_id

              AND totals.color_id =
                :color_id

            LIMIT 1
        )";

    query.prepare(sql);

    query.bindValue(":workspace_id", workspaceId);

    query.bindValue(":part_id", partId);

    query.bindValue(":color_id", colorId);

    if (!query.exec()) {
        qCritical() << "Unable to retrieve lost inventory "
                       "for Part/Color:"
                    << query.lastError().text();

        return std::nullopt;
    }

    if (!query.next())
        return std::nullopt;

    return itemFromQuery(query);
}