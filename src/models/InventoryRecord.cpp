#include "InventoryRecord.h"

int InventoryRecord::id() const
{
    return m_id;
}

void InventoryRecord::setId(int id)
{
    m_id = id;
}

int InventoryRecord::workspaceId() const
{
    return m_workspaceId;
}

void InventoryRecord::setWorkspaceId(int workspaceId)
{
    m_workspaceId = workspaceId;
}

int InventoryRecord::partId() const
{
    return m_partId;
}

void InventoryRecord::setPartId(int partId)
{
    m_partId = partId;
}

int InventoryRecord::colorId() const
{
    return m_colorId;
}

void InventoryRecord::setColorId(int colorId)
{
    m_colorId = colorId;
}

int InventoryRecord::storageLocationId() const
{
    return m_storageLocationId;
}

void InventoryRecord::setStorageLocationId(int storageLocationId)
{
    m_storageLocationId = storageLocationId;
}

QString InventoryRecord::condition() const
{
    return m_condition;
}

void InventoryRecord::setCondition(const QString& condition)
{
    m_condition = condition;
}

QString InventoryRecord::ownershipType() const
{
    return m_ownershipType;
}

void InventoryRecord::setOwnershipType(const QString& ownershipType)
{
    m_ownershipType = ownershipType;
}

int InventoryRecord::quantity() const
{
    return m_quantity;
}

void InventoryRecord::setQuantity(int quantity)
{
    m_quantity = quantity;
}

QDateTime InventoryRecord::createdUtc() const
{
    return m_createdUtc;
}

void InventoryRecord::setCreatedUtc(const QDateTime& createdUtc)
{
    m_createdUtc = createdUtc;
}

QDateTime InventoryRecord::modifiedUtc() const
{
    return m_modifiedUtc;
}

void InventoryRecord::setModifiedUtc(const QDateTime& modifiedUtc)
{
    m_modifiedUtc = modifiedUtc;
}