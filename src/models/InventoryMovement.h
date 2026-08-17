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

#include <QDateTime>
#include <QString>

class InventoryMovement
{
public:
    InventoryMovement() = default;

    int id() const;
    void setId(int id);

    int workspaceId() const;
    void setWorkspaceId(int workspaceId);

    int inventoryRecordId() const;
    void setInventoryRecordId(int inventoryRecordId);

    int partId() const;
    void setPartId(int partId);

    int colorId() const;
    void setColorId(int colorId);

    QString movementType() const;
    void setMovementType(const QString& movementType);

    int quantityChange() const;
    void setQuantityChange(int quantityChange);

    int fromStorageLocationId() const;
    void setFromStorageLocationId(int locationId);

    int toStorageLocationId() const;
    void setToStorageLocationId(int locationId);

    QString condition() const;
    void setCondition(const QString& condition);

    QString ownershipType() const;
    void setOwnershipType(const QString& ownershipType);

    QString referenceType() const;
    void setReferenceType(const QString& referenceType);

    QString referenceId() const;
    void setReferenceId(const QString& referenceId);

    QString notes() const;
    void setNotes(const QString& notes);

    QDateTime createdUtc() const;
    void setCreatedUtc(const QDateTime& createdUtc);

private:
    int m_id = 0;
    int m_workspaceId = 0;
    int m_inventoryRecordId = 0;
    int m_partId = 0;
    int m_colorId = 0;

    QString m_movementType;

    int m_quantityChange = 0;

    int m_fromStorageLocationId = 0;
    int m_toStorageLocationId = 0;

    QString m_condition;
    QString m_ownershipType;

    QString m_referenceType;
    QString m_referenceId;

    QString m_notes;

    QDateTime m_createdUtc;
};
