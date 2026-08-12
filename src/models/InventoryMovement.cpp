#include "InventoryMovement.h"

int InventoryMovement::id() const
{
    return m_id;
}

void InventoryMovement::setId(int id)
{
    m_id = id;
}

int InventoryMovement::workspaceId() const
{
    return m_workspaceId;
}

void InventoryMovement::setWorkspaceId(int workspaceId)
{
    m_workspaceId = workspaceId;
}

int InventoryMovement::inventoryRecordId() const
{
    return m_inventoryRecordId;
}

void InventoryMovement::setInventoryRecordId(int inventoryRecordId)
{
    m_inventoryRecordId = inventoryRecordId;
}

int InventoryMovement::partId() const
{
    return m_partId;
}

void InventoryMovement::setPartId(int partId)
{
    m_partId = partId;
}

int InventoryMovement::colorId() const
{
    return m_colorId;
}

void InventoryMovement::setColorId(int colorId)
{
    m_colorId = colorId;
}

QString InventoryMovement::movementType() const
{
    return m_movementType;
}

void InventoryMovement::setMovementType(const QString& movementType)
{
    m_movementType = movementType;
}

int InventoryMovement::quantityChange() const
{
    return m_quantityChange;
}

void InventoryMovement::setQuantityChange(int quantityChange)
{
    m_quantityChange = quantityChange;
}

int InventoryMovement::fromStorageLocationId() const
{
    return m_fromStorageLocationId;
}

void InventoryMovement::setFromStorageLocationId(int locationId)
{
    m_fromStorageLocationId = locationId;
}

int InventoryMovement::toStorageLocationId() const
{
    return m_toStorageLocationId;
}

void InventoryMovement::setToStorageLocationId(int locationId)
{
    m_toStorageLocationId = locationId;
}

QString InventoryMovement::condition() const
{
    return m_condition;
}

void InventoryMovement::setCondition(const QString& condition)
{
    m_condition = condition;
}

QString InventoryMovement::ownershipType() const
{
    return m_ownershipType;
}

void InventoryMovement::setOwnershipType(const QString& ownershipType)
{
    m_ownershipType = ownershipType;
}

QString InventoryMovement::referenceType() const
{
    return m_referenceType;
}

void InventoryMovement::setReferenceType(const QString& referenceType)
{
    m_referenceType = referenceType;
}

QString InventoryMovement::referenceId() const
{
    return m_referenceId;
}

void InventoryMovement::setReferenceId(const QString& referenceId)
{
    m_referenceId = referenceId;
}

QString InventoryMovement::notes() const
{
    return m_notes;
}

void InventoryMovement::setNotes(const QString& notes)
{
    m_notes = notes;
}

QDateTime InventoryMovement::createdUtc() const
{
    return m_createdUtc;
}

void InventoryMovement::setCreatedUtc(const QDateTime& createdUtc)
{
    m_createdUtc = createdUtc;
}