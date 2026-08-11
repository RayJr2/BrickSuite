#pragma once

#include "../models/InventoryRecord.h"

#include <QList>
#include <optional>

class QSqlQuery;

class InventoryRecordRepository
{
public:
    bool create(InventoryRecord& record);

    QList<InventoryRecord> getByWorkspace(int workspaceId) const;

    QList<InventoryRecord> getByStorageLocation(int workspaceId, int storageLocationId) const;

    std::optional<InventoryRecord> getById(int id) const;

    bool updateQuantity(int inventoryRecordId, int quantity);

private:
    InventoryRecord inventoryRecordFromQuery(const QSqlQuery& query) const;
};
