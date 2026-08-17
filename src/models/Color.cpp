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

#include "Color.h"

int Color::id() const
{
    return m_id;
}

void Color::setId(int id)
{
    m_id = id;
}

QString Color::name() const
{
    return m_name;
}

void Color::setName(const QString& name)
{
    m_name = name;
}

QString Color::rgb() const
{
    return m_rgb;
}

void Color::setRgb(const QString& rgb)
{
    m_rgb = rgb;
}

bool Color::isTransparent() const
{
    return m_isTransparent;
}

void Color::setIsTransparent(bool isTransparent)
{
    m_isTransparent = isTransparent;
}

int Color::rebrickableId() const
{
    return m_rebrickableId;
}

void Color::setRebrickableId(int rebrickableId)
{
    m_rebrickableId = rebrickableId;
}

QDateTime Color::createdUtc() const
{
    return m_createdUtc;
}

void Color::setCreatedUtc(const QDateTime& createdUtc)
{
    m_createdUtc = createdUtc;
}

QDateTime Color::modifiedUtc() const
{
    return m_modifiedUtc;
}

void Color::setModifiedUtc(const QDateTime& modifiedUtc)
{
    m_modifiedUtc = modifiedUtc;
}