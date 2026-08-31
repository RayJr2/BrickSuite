#include "BuildAllocation.h"
int BuildAllocation::id() const { return m_id; }
void BuildAllocation::setId(int id) { m_id = id; }
int BuildAllocation::buildId() const { return m_buildId; }
void BuildAllocation::setBuildId(int buildId) { m_buildId = buildId; }
int BuildAllocation::buildRequirementId() const { return m_buildRequirementId; }
void BuildAllocation::setBuildRequirementId(int buildRequirementId) { m_buildRequirementId = buildRequirementId; }
int BuildAllocation::inventoryRecordId() const { return m_inventoryRecordId; }
void BuildAllocation::setInventoryRecordId(int inventoryRecordId) { m_inventoryRecordId = inventoryRecordId; }
int BuildAllocation::partId() const { return m_partId; }
void BuildAllocation::setPartId(int partId) { m_partId = partId; }
int BuildAllocation::colorId() const { return m_colorId; }
void BuildAllocation::setColorId(int colorId) { m_colorId = colorId; }
int BuildAllocation::storageLocationId() const { return m_storageLocationId; }
void BuildAllocation::setStorageLocationId(int storageLocationId) { m_storageLocationId = storageLocationId; }
int BuildAllocation::quantityAllocated() const { return m_quantityAllocated; }
void BuildAllocation::setQuantityAllocated(int quantityAllocated) { m_quantityAllocated = quantityAllocated; }
QDateTime BuildAllocation::createdUtc() const { return m_createdUtc; }
void BuildAllocation::setCreatedUtc(const QDateTime& createdUtc) { m_createdUtc = createdUtc; }
QDateTime BuildAllocation::modifiedUtc() const { return m_modifiedUtc; }
void BuildAllocation::setModifiedUtc(const QDateTime& modifiedUtc) { m_modifiedUtc = modifiedUtc; }
