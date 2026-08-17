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

#include "Part.h"

int Part::id() const
{
    return m_id;
}

void Part::setId(int id)
{
    m_id = id;
}

QString Part::partNumber() const
{
    return m_partNumber;
}

void Part::setPartNumber(const QString& partNumber)
{
    m_partNumber = partNumber;
}

QString Part::name() const
{
    return m_name;
}

void Part::setName(const QString& name)
{
    m_name = name;
}

int Part::partCategoryId() const
{
    return m_partCategoryId;
}

void Part::setPartCategoryId(int partCategoryId)
{
    m_partCategoryId = partCategoryId;
}

QString Part::rebrickablePartId() const
{
    return m_rebrickablePartId;
}

void Part::setRebrickablePartId(const QString& rebrickablePartId)
{
    m_rebrickablePartId = rebrickablePartId;
}

bool Part::isActive() const
{
    return m_isActive;
}

void Part::setIsActive(bool isActive)
{
    m_isActive = isActive;
}

QDateTime Part::createdUtc() const
{
    return m_createdUtc;
}

QString Part::material() const
{
    return m_material;
}

void Part::setMaterial(const QString& material)
{
    m_material = material;
}

void Part::setCreatedUtc(const QDateTime& createdUtc)
{
    m_createdUtc = createdUtc;
}

QDateTime Part::modifiedUtc() const
{
    return m_modifiedUtc;
}

void Part::setModifiedUtc(const QDateTime& modifiedUtc)
{
    m_modifiedUtc = modifiedUtc;
}