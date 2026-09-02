/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 *
 * This file is part of BrickSuite.
 */

#include "MinifigCatalogItem.h"

int MinifigCatalogItem::id() const
{
    return m_id;
}

void MinifigCatalogItem::setId(int id)
{
    m_id = id;
}

QString MinifigCatalogItem::name() const
{
    return m_name;
}

void MinifigCatalogItem::setName(const QString& name)
{
    m_name = name.trimmed();
}

int MinifigCatalogItem::numberOfParts() const
{
    return m_numberOfParts;
}

void MinifigCatalogItem::setNumberOfParts(int numberOfParts)
{
    m_numberOfParts = numberOfParts;
}

QString MinifigCatalogItem::imageUrl() const
{
    return m_imageUrl;
}

void MinifigCatalogItem::setImageUrl(const QString& imageUrl)
{
    m_imageUrl = imageUrl.trimmed();
}

bool MinifigCatalogItem::isActive() const
{
    return m_isActive;
}

void MinifigCatalogItem::setIsActive(bool isActive)
{
    m_isActive = isActive;
}

QDateTime MinifigCatalogItem::createdUtc() const
{
    return m_createdUtc;
}

void MinifigCatalogItem::setCreatedUtc(const QDateTime& createdUtc)
{
    m_createdUtc = createdUtc;
}

QDateTime MinifigCatalogItem::modifiedUtc() const
{
    return m_modifiedUtc;
}

void MinifigCatalogItem::setModifiedUtc(const QDateTime& modifiedUtc)
{
    m_modifiedUtc = modifiedUtc;
}
