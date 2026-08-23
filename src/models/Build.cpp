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

#include "Build.h"

int Build::id() const
{
    return m_id;
}

void Build::setId(int id)
{
    m_id = id;
}

int Build::workspaceId() const
{
    return m_workspaceId;
}

void Build::setWorkspaceId(int workspaceId)
{
    m_workspaceId = workspaceId;
}

QString Build::buildType() const
{
    return m_buildType;
}

void Build::setBuildType(const QString& buildType)
{
    m_buildType = buildType.trimmed();
}

QString Build::name() const
{
    return m_name;
}

void Build::setName(const QString& name)
{
    m_name = name.trimmed();
}

QString Build::setNumber() const
{
    return m_setNumber;
}

void Build::setSetNumber(const QString& setNumber)
{
    m_setNumber = setNumber.trimmed();
}

QString Build::inventoryMode() const
{
    return m_inventoryMode;
}

void Build::setInventoryMode(const QString& inventoryMode)
{
    m_inventoryMode = inventoryMode.trimmed();
}

int Build::manufacturerId() const
{
    return m_manufacturerId;
}

void Build::setManufacturerId(int manufacturerId)
{
    m_manufacturerId = manufacturerId;
}

QString Build::status() const
{
    return m_status;
}

void Build::setStatus(const QString& status)
{
    m_status = status.trimmed();
}

bool Build::isActive() const
{
    return m_isActive;
}

void Build::setIsActive(bool active)
{
    m_isActive = active;
}

QString Build::notes() const
{
    return m_notes;
}

void Build::setNotes(const QString& notes)
{
    m_notes = notes.trimmed();
}

QDateTime Build::createdUtc() const
{
    return m_createdUtc;
}

void Build::setCreatedUtc(const QDateTime& createdUtc)
{
    m_createdUtc = createdUtc;
}

QDateTime Build::modifiedUtc() const
{
    return m_modifiedUtc;
}

void Build::setModifiedUtc(const QDateTime& modifiedUtc)
{
    m_modifiedUtc = modifiedUtc;
}