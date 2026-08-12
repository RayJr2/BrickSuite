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