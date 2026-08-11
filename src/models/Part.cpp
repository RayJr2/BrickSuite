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