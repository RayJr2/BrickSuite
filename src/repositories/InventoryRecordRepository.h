#pragma once

#include "../models/InventoryRecord.h"
#include "../models/InventorySearchCriteria.h"
#include "../models/InventorySearchResult.h"

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

    QList<InventorySearchResult> search(const InventorySearchCriteria& criteria) const;

    bool addOrIncreaseQuantity(InventoryRecord& record,
                               const QString& movementType = QString(),
                               const QString& referenceType = QString(),
                               const QString& referenceId = QString(),
                               const QString& notes = QString(),
                               bool manageTransaction = true);

    bool updateOrMerge(InventoryRecord& record);

    bool remove(int inventoryRecordId);

    bool moveInventory(int inventoryRecordId, int destinationStorageLocationId, int quantityToMove);

private:
    InventoryRecord inventoryRecordFromQuery(const QSqlQuery& query) const;
};
