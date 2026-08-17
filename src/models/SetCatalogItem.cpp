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

#include "SetCatalogItem.h"

int SetCatalogItem::id() const
{
    return m_id;
}

void SetCatalogItem::setId(int id)
{
    m_id = id;
}

QString SetCatalogItem::setNumber() const
{
    return m_setNumber;
}

void SetCatalogItem::setSetNumber(const QString& setNumber)
{
    m_setNumber = setNumber.trimmed();
}

QString SetCatalogItem::name() const
{
    return m_name;
}

void SetCatalogItem::setName(const QString& name)
{
    m_name = name.trimmed();
}

int SetCatalogItem::year() const
{
    return m_year;
}

void SetCatalogItem::setYear(int year)
{
    m_year = year;
}

int SetCatalogItem::themeId() const
{
    return m_themeId;
}

void SetCatalogItem::setThemeId(int themeId)
{
    m_themeId = themeId;
}

int SetCatalogItem::numberOfParts() const
{
    return m_numberOfParts;
}

void SetCatalogItem::setNumberOfParts(int numberOfParts)
{
    m_numberOfParts = numberOfParts;
}

QString SetCatalogItem::imageUrl() const
{
    return m_imageUrl;
}

void SetCatalogItem::setImageUrl(const QString& imageUrl)
{
    m_imageUrl = imageUrl.trimmed();
}

QDateTime SetCatalogItem::createdUtc() const
{
    return m_createdUtc;
}

void SetCatalogItem::setCreatedUtc(const QDateTime& createdUtc)
{
    m_createdUtc = createdUtc;
}

QDateTime SetCatalogItem::modifiedUtc() const
{
    return m_modifiedUtc;
}

void SetCatalogItem::setModifiedUtc(const QDateTime& modifiedUtc)
{
    m_modifiedUtc = modifiedUtc;
}