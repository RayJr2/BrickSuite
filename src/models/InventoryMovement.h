#pragma once

#include <QDateTime>
#include <QString>

class InventoryMovement
{
public:
    InventoryMovement() = default;

    int id() const;
    void setId(int id);

    int workspaceId() const;
    void setWorkspaceId(int workspaceId);

    int inventoryRecordId() const;
    void setInventoryRecordId(int inventoryRecordId);

    int partId() const;
    void setPartId(int partId);

    int colorId() const;
    void setColorId(int colorId);

    QString movementType() const;
    void setMovementType(const QString& movementType);

    int quantityChange() const;
    void setQuantityChange(int quantityChange);

    int fromStorageLocationId() const;
    void setFromStorageLocationId(int locationId);

    int toStorageLocationId() const;
    void setToStorageLocationId(int locationId);

    QString condition() const;
    void setCondition(const QString& condition);

    QString ownershipType() const;
    void setOwnershipType(const QString& ownershipType);

    QString referenceType() const;
    void setReferenceType(const QString& referenceType);

    QString referenceId() const;
    void setReferenceId(const QString& referenceId);

    QString notes() const;
    void setNotes(const QString& notes);

    QDateTime createdUtc() const;
    void setCreatedUtc(const QDateTime& createdUtc);

private:
    int m_id = 0;
    int m_workspaceId = 0;
    int m_inventoryRecordId = 0;
    int m_partId = 0;
    int m_colorId = 0;

    QString m_movementType;

    int m_quantityChange = 0;

    int m_fromStorageLocationId = 0;
    int m_toStorageLocationId = 0;

    QString m_condition;
    QString m_ownershipType;

    QString m_referenceType;
    QString m_referenceId;

    QString m_notes;

    QDateTime m_createdUtc;
};
