#pragma once

#include <QDateTime>

class BuildAllocation
{
public:
    BuildAllocation() = default;

    int id() const;
    void setId(int id);
    int buildId() const;
    void setBuildId(int buildId);
    int buildRequirementId() const;
    void setBuildRequirementId(int buildRequirementId);
    int inventoryRecordId() const;
    void setInventoryRecordId(int inventoryRecordId);
    int partId() const;
    void setPartId(int partId);
    int colorId() const;
    void setColorId(int colorId);
    int storageLocationId() const;
    void setStorageLocationId(int storageLocationId);
    int quantityAllocated() const;
    void setQuantityAllocated(int quantityAllocated);
    QDateTime createdUtc() const;
    void setCreatedUtc(const QDateTime& createdUtc);
    QDateTime modifiedUtc() const;
    void setModifiedUtc(const QDateTime& modifiedUtc);

private:
    int m_id = 0;
    int m_buildId = 0;
    int m_buildRequirementId = 0;
    int m_inventoryRecordId = 0;
    int m_partId = 0;
    int m_colorId = 0;
    int m_storageLocationId = 0;
    int m_quantityAllocated = 0;
    QDateTime m_createdUtc;
    QDateTime m_modifiedUtc;
};
