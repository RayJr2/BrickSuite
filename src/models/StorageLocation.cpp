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

#include "StorageLocation.h"

int StorageLocation::id() const
{
    return m_id;
}

void StorageLocation::setId(int id)
{
    m_id = id;
}

int StorageLocation::workspaceId() const
{
    return m_workspaceId;
}

void StorageLocation::setWorkspaceId(int workspaceId)
{
    m_workspaceId = workspaceId;
}

int StorageLocation::parentLocationId() const
{
    return m_parentLocationId;
}

void StorageLocation::setParentLocationId(int parentLocationId)
{
    m_parentLocationId = parentLocationId;
}

int StorageLocation::locationTypeId() const
{
    return m_locationTypeId;
}

void StorageLocation::setLocationTypeId(int locationTypeId)
{
    m_locationTypeId = locationTypeId;
}

QString StorageLocation::name() const
{
    return m_name;
}

void StorageLocation::setName(const QString& name)
{
    m_name = name;
}

QString StorageLocation::description() const
{
    return m_description;
}

void StorageLocation::setDescription(const QString& description)
{
    m_description = description;
}

int StorageLocation::sortOrder() const
{
    return m_sortOrder;
}

void StorageLocation::setSortOrder(int sortOrder)
{
    m_sortOrder = sortOrder;
}

bool StorageLocation::isActive() const
{
    return m_isActive;
}

void StorageLocation::setIsActive(bool isActive)
{
    m_isActive = isActive;
}

bool StorageLocation::allowsInventory() const
{
    return m_allowsInventory;
}

void StorageLocation::setAllowsInventory(bool allowsInventory)
{
    m_allowsInventory = allowsInventory;
}

bool StorageLocation::allowsCollection() const
{
    return m_allowsCollection;
}

void StorageLocation::setAllowsCollection(bool allowsCollection)
{
    m_allowsCollection = allowsCollection;
}

QDateTime StorageLocation::createdUtc() const
{
    return m_createdUtc;
}

void StorageLocation::setCreatedUtc(const QDateTime& createdUtc)
{
    m_createdUtc = createdUtc;
}

QDateTime StorageLocation::modifiedUtc() const
{
    return m_modifiedUtc;
}

void StorageLocation::setModifiedUtc(const QDateTime& modifiedUtc)
{
    m_modifiedUtc = modifiedUtc;
}
