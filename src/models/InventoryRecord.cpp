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

#include "InventoryRecord.h"

int InventoryRecord::id() const
{
    return m_id;
}

void InventoryRecord::setId(int id)
{
    m_id = id;
}

int InventoryRecord::workspaceId() const
{
    return m_workspaceId;
}

void InventoryRecord::setWorkspaceId(int workspaceId)
{
    m_workspaceId = workspaceId;
}

int InventoryRecord::partId() const
{
    return m_partId;
}

void InventoryRecord::setPartId(int partId)
{
    m_partId = partId;
}

int InventoryRecord::colorId() const
{
    return m_colorId;
}

void InventoryRecord::setColorId(int colorId)
{
    m_colorId = colorId;
}

int InventoryRecord::storageLocationId() const
{
    return m_storageLocationId;
}

void InventoryRecord::setStorageLocationId(int storageLocationId)
{
    m_storageLocationId = storageLocationId;
}

QString InventoryRecord::condition() const
{
    return m_condition;
}

void InventoryRecord::setCondition(const QString& condition)
{
    m_condition = condition;
}

QString InventoryRecord::ownershipType() const
{
    return m_ownershipType;
}

void InventoryRecord::setOwnershipType(const QString& ownershipType)
{
    m_ownershipType = ownershipType;
}

int InventoryRecord::quantity() const
{
    return m_quantity;
}

void InventoryRecord::setQuantity(int quantity)
{
    m_quantity = quantity;
}

QDateTime InventoryRecord::createdUtc() const
{
    return m_createdUtc;
}

void InventoryRecord::setCreatedUtc(const QDateTime& createdUtc)
{
    m_createdUtc = createdUtc;
}

QDateTime InventoryRecord::modifiedUtc() const
{
    return m_modifiedUtc;
}

void InventoryRecord::setModifiedUtc(const QDateTime& modifiedUtc)
{
    m_modifiedUtc = modifiedUtc;
}