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

#include "Workspace.h"

int Workspace::id() const
{
    return m_id;
}

void Workspace::setId(int id)
{
    m_id = id;
}

QString Workspace::name() const
{
    return m_name;
}

void Workspace::setName(const QString& name)
{
    m_name = name;
}

QString Workspace::description() const
{
    return m_description;
}

void Workspace::setDescription(const QString& description)
{
    m_description = description;
}

QDateTime Workspace::createdUtc() const
{
    return m_createdUtc;
}

void Workspace::setCreatedUtc(const QDateTime& createdUtc)
{
    m_createdUtc = createdUtc;
}

QDateTime Workspace::modifiedUtc() const
{
    return m_modifiedUtc;
}

void Workspace::setModifiedUtc(const QDateTime& modifiedUtc)
{
    m_modifiedUtc = modifiedUtc;
}

bool Workspace::isActive() const
{
    return m_isActive;
}

void Workspace::setIsActive(bool isActive)
{
    m_isActive = isActive;
}