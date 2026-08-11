#include "StorageLocationType.h"

int StorageLocationType::id() const
{
    return m_id;
}

void StorageLocationType::setId(int id)
{
    m_id = id;
}

QString StorageLocationType::name() const
{
    return m_name;
}

void StorageLocationType::setName(const QString& name)
{
    m_name = name;
}

QString StorageLocationType::description() const
{
    return m_description;
}

void StorageLocationType::setDescription(const QString& description)
{
    m_description = description;
}

bool StorageLocationType::isSystem() const
{
    return m_isSystem;
}

void StorageLocationType::setIsSystem(bool isSystem)
{
    m_isSystem = isSystem;
}

bool StorageLocationType::isActive() const
{
    return m_isActive;
}

void StorageLocationType::setIsActive(bool isActive)
{
    m_isActive = isActive;
}

int StorageLocationType::sortOrder() const
{
    return m_sortOrder;
}

void StorageLocationType::setSortOrder(int sortOrder)
{
    m_sortOrder = sortOrder;
}