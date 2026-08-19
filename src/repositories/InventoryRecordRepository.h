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

    bool updateOrMerge(InventoryRecord& record);

    bool remove(int inventoryRecordId);

    bool moveInventory(int inventoryRecordId, int destinationStorageLocationId, int quantityToMove);

    int totalQuantityForPartColor(int workspaceId, int partId, int colorId) const;

    QList<InventoryRecord> getByPartColor(int workspaceId, int partId, int colorId) const;

    bool markLost(int inventoryRecordId, int quantityLost, const QString& notes = QString());

    bool markFound(int workspaceId,
                   int partId,
                   int colorId,
                   int quantityFound,
                   int destinationStorageLocationId,
                   const QString& condition,
                   const QString& ownershipType,
                   const QString& notes = QString());

private:
    InventoryRecord inventoryRecordFromQuery(const QSqlQuery& query) const;
};
