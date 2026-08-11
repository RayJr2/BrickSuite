#pragma once

#include <QDateTime>
#include <QString>

class InventoryRecord
{
public:
    InventoryRecord() = default;

    int id() const;
    void setId(int id);

    int workspaceId() const;
    void setWorkspaceId(int workspaceId);

    int partId() const;
    void setPartId(int partId);

    int colorId() const;
    void setColorId(int colorId);

    int storageLocationId() const;
    void setStorageLocationId(int storageLocationId);

    QString condition() const;
    void setCondition(const QString& condition);

    QString ownershipType() const;
    void setOwnershipType(const QString& ownershipType);

    int quantity() const;
    void setQuantity(int quantity);

    QDateTime createdUtc() const;
    void setCreatedUtc(const QDateTime& createdUtc);

    QDateTime modifiedUtc() const;
    void setModifiedUtc(const QDateTime& modifiedUtc);

private:
    int m_id = 0;
    int m_workspaceId = 0;
    int m_partId = 0;
    int m_colorId = 0;
    int m_storageLocationId = 0;

    QString m_condition = "Used";
    QString m_ownershipType = "Owned";

    int m_quantity = 0;

    QDateTime m_createdUtc;
    QDateTime m_modifiedUtc;
};
