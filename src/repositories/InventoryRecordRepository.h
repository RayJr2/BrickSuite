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

#include "RepositoryConnection.h"

#include "../models/InventoryRecord.h"
#include "../models/InventorySearchCriteria.h"
#include "../models/InventorySearchResult.h"

#include <QList>
#include <optional>

class QSqlQuery;


class InventoryRecordRepository : protected RepositoryConnection
{
public:
    struct AddResult {
        int inventoryRecordId = 0;
        int resultingQuantity = 0;
        bool created = false;
        bool merged = false;
    };

    struct UpdateResult {
        int sourceRecordId = 0;
        int survivingRecordId = 0;
        int resultingQuantity = 0;
        int storageLocationId = 0;
        bool merged = false;
    };

    struct MoveResult {
        int sourceRecordId = 0;
        int destinationRecordId = 0;
        int movedQuantity = 0;
        int resultingSourceQuantity = 0;
        int resultingDestinationQuantity = 0;
        bool destinationCreated = false;
    };

    struct FoundResult {
        int inventoryRecordId = 0;
        int quantityRestored = 0;
        int destinationStorageLocationId = 0;
        int outstandingLostQuantity = 0;
        bool created = false;
        bool merged = false;
    };

    InventoryRecordRepository() = default;
    explicit InventoryRecordRepository(const QSqlDatabase& database)
        : RepositoryConnection(database) {}

    bool create(InventoryRecord& record);

    QList<InventoryRecord> getByWorkspace(int workspaceId) const;

    QList<InventoryRecord> getByStorageLocation(int workspaceId, int storageLocationId) const;

    std::optional<InventoryRecord> getById(int id) const;
    bool tryGetById(int id, std::optional<InventoryRecord>& record) const;

    bool updateQuantity(int inventoryRecordId, int quantity);

    bool setQuantityWithMovement(int inventoryRecordId,
                                 int newQuantity,
                                 const QString& movementType,
                                 const QString& referenceType = QString(),
                                 const QString& referenceId = QString(),
                                 const QString& notes = QString(),
                                 bool manageTransaction = true);

    QList<InventorySearchResult> search(const InventorySearchCriteria& criteria) const;

    int count(const InventorySearchCriteria& criteria) const;

    bool addOrIncreaseQuantity(InventoryRecord& record,
                               const QString& movementType = QString(),
                               const QString& referenceType = QString(),
                               const QString& referenceId = QString(),
                               const QString& notes = QString(),
                               bool manageTransaction = true);
    bool addOrIncreaseQuantityInCurrentTransaction(
        InventoryRecord& record, const QString& movementType = QString(),
        const QString& referenceType = QString(), const QString& referenceId = QString(),
        const QString& notes = QString(), AddResult* result = nullptr);

    bool updateOrMerge(InventoryRecord& record);
    bool updateOrMergeInCurrentTransaction(InventoryRecord& record,
                                           UpdateResult* result = nullptr);

    bool remove(int inventoryRecordId);

    bool correctEntry(int inventoryRecordId,
                      int replacementPartId,
                      int quantityToCorrect,
                      const QString& notes = QString());
    bool correctEntryInCurrentTransaction(
        int inventoryRecordId, int replacementPartId, int quantityToCorrect,
        const QString& notes = QString());

    bool removeEntry(int inventoryRecordId,
                     int quantityToRemove,
                     const QString& notes = QString(),
                     QString* errorMessage = nullptr);
    bool removeEntryInCurrentTransaction(int inventoryRecordId, int quantityToRemove,
                                         const QString& notes = QString(),
                                         QString* errorMessage = nullptr);

    bool moveInventory(int inventoryRecordId, int destinationStorageLocationId, int quantityToMove);
    bool moveInventoryInCurrentTransaction(int inventoryRecordId, int destinationStorageLocationId,
                                           int quantityToMove, MoveResult* result = nullptr);

    int totalQuantityForPartColor(int workspaceId, int partId, int colorId) const;

    QList<InventoryRecord> getByPartColor(int workspaceId, int partId, int colorId) const;

    bool markLost(int inventoryRecordId, int quantityLost, const QString& notes = QString());
    bool markLostInCurrentTransaction(
        int inventoryRecordId, int quantityLost, const QString& notes = QString());

    bool markFound(int workspaceId,
                   int partId,
                   int colorId,
                   int quantityFound,
                   int destinationStorageLocationId,
                   const QString& condition,
                   const QString& ownershipType,
                   const QString& notes = QString());
    bool markFoundInCurrentTransaction(
        int workspaceId, int partId, int colorId, int quantityFound,
        int destinationStorageLocationId, const QString& condition,
        const QString& ownershipType, const QString& notes = QString(),
        FoundResult* result = nullptr);

private:
    InventoryRecord inventoryRecordFromQuery(const QSqlQuery& query) const;
};
