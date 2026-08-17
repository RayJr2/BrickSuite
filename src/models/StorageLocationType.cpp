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

#include "StorageLocationType.h"

int StorageLocationType::id() const
{
    return m_id;
}

void StorageLocationType::setId(int id)
{
    m_id = id;
}

QString StorageLocationType::name() const
{
    return m_name;
}

void StorageLocationType::setName(const QString& name)
{
    m_name = name;
}

QString StorageLocationType::description() const
{
    return m_description;
}

void StorageLocationType::setDescription(const QString& description)
{
    m_description = description;
}

bool StorageLocationType::isSystem() const
{
    return m_isSystem;
}

void StorageLocationType::setIsSystem(bool isSystem)
{
    m_isSystem = isSystem;
}

bool StorageLocationType::isActive() const
{
    return m_isActive;
}

void StorageLocationType::setIsActive(bool isActive)
{
    m_isActive = isActive;
}

int StorageLocationType::sortOrder() const
{
    return m_sortOrder;
}

void StorageLocationType::setSortOrder(int sortOrder)
{
    m_sortOrder = sortOrder;
}