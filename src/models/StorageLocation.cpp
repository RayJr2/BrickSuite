#include "StorageLocation.h"

int StorageLocation::id() const
{
    return m_id;
}

void StorageLocation::setId(int id)
{
    m_id = id;
}

int StorageLocation::workspaceId() const
{
    return m_workspaceId;
}

void StorageLocation::setWorkspaceId(int workspaceId)
{
    m_workspaceId = workspaceId;
}

int StorageLocation::parentLocationId() const
{
    return m_parentLocationId;
}

void StorageLocation::setParentLocationId(int parentLocationId)
{
    m_parentLocationId = parentLocationId;
}

int StorageLocation::locationTypeId() const
{
    return m_locationTypeId;
}

void StorageLocation::setLocationTypeId(int locationTypeId)
{
    m_locationTypeId = locationTypeId;
}

QString StorageLocation::name() const
{
    return m_name;
}

void StorageLocation::setName(const QString& name)
{
    m_name = name;
}

QString StorageLocation::description() const
{
    return m_description;
}

void StorageLocation::setDescription(const QString& description)
{
    m_description = description;
}

int StorageLocation::sortOrder() const
{
    return m_sortOrder;
}

void StorageLocation::setSortOrder(int sortOrder)
{
    m_sortOrder = sortOrder;
}

bool StorageLocation::isActive() const
{
    return m_isActive;
}

void StorageLocation::setIsActive(bool isActive)
{
    m_isActive = isActive;
}

QDateTime StorageLocation::createdUtc() const
{
    return m_createdUtc;
}

void StorageLocation::setCreatedUtc(const QDateTime& createdUtc)
{
    m_createdUtc = createdUtc;
}

QDateTime StorageLocation::modifiedUtc() const
{
    return m_modifiedUtc;
}

void StorageLocation::setModifiedUtc(const QDateTime& modifiedUtc)
{
    m_modifiedUtc = modifiedUtc;
}