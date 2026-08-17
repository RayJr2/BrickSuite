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

#include "PartCategory.h"

int PartCategory::id() const
{
    return m_id;
}

void PartCategory::setId(int id)
{
    m_id = id;
}

QString PartCategory::name() const
{
    return m_name;
}

void PartCategory::setName(const QString& name)
{
    m_name = name;
}

int PartCategory::rebrickableId() const
{
    return m_rebrickableId;
}

void PartCategory::setRebrickableId(int rebrickableId)
{
    m_rebrickableId = rebrickableId;
}

QDateTime PartCategory::createdUtc() const
{
    return m_createdUtc;
}

void PartCategory::setCreatedUtc(const QDateTime& createdUtc)
{
    m_createdUtc = createdUtc;
}

QDateTime PartCategory::modifiedUtc() const
{
    return m_modifiedUtc;
}

void PartCategory::setModifiedUtc(const QDateTime& modifiedUtc)
{
    m_modifiedUtc = modifiedUtc;
}