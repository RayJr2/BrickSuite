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

#include "BuildAllocation.h"

int BuildAllocation::id() const
{
    return m_id;
}

void BuildAllocation::setId(int id)
{
    m_id = id;
}

int BuildAllocation::buildId() const
{
    return m_buildId;
}

void BuildAllocation::setBuildId(int buildId)
{
    m_buildId = buildId;
}

int BuildAllocation::inventoryRecordId() const
{
    return m_inventoryRecordId;
}

void BuildAllocation::setInventoryRecordId(int inventoryRecordId)
{
    m_inventoryRecordId = inventoryRecordId;
}

int BuildAllocation::partId() const
{
    return m_partId;
}

void BuildAllocation::setPartId(int partId)
{
    m_partId = partId;
}

int BuildAllocation::colorId() const
{
    return m_colorId;
}

void BuildAllocation::setColorId(int colorId)
{
    m_colorId = colorId;
}

int BuildAllocation::storageLocationId() const
{
    return m_storageLocationId;
}

void BuildAllocation::setStorageLocationId(int storageLocationId)
{
    m_storageLocationId = storageLocationId;
}

int BuildAllocation::quantityAllocated() const
{
    return m_quantityAllocated;
}

void BuildAllocation::setQuantityAllocated(int quantityAllocated)
{
    m_quantityAllocated = quantityAllocated;
}

QDateTime BuildAllocation::createdUtc() const
{
    return m_createdUtc;
}

void BuildAllocation::setCreatedUtc(const QDateTime& createdUtc)
{
    m_createdUtc = createdUtc;
}

QDateTime BuildAllocation::modifiedUtc() const
{
    return m_modifiedUtc;
}

void BuildAllocation::setModifiedUtc(const QDateTime& modifiedUtc)
{
    m_modifiedUtc = modifiedUtc;
}