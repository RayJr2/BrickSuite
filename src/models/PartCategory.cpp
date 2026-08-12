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