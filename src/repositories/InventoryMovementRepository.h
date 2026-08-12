#pragma once

#include "../models/InventoryHistoryResult.h"
#include "../models/InventoryMovement.h"

#include <QList>

class QSqlQuery;

class InventoryMovementRepository
{
public:
    bool create(InventoryMovement& movement);

    QList<InventoryMovement> getByInventoryRecord(int inventoryRecordId) const;

    QList<InventoryMovement> getByPart(int workspaceId, int partId) const;

    QList<InventoryHistoryResult> getHistoryForPartColor(int workspaceId,
                                                         int partId,
                                                         int colorId) const;

private:
    InventoryMovement movementFromQuery(const QSqlQuery& query) const;
};
