#pragma once

#include "../models/BuildAllocation.h"

#include <QList>
#include <optional>

class QSqlQuery;

struct BuildPartManufacturerProvenance
{
    int manufacturerId = 0;
    int quantityPulled = 0;
};

class BuildAllocationRepository
{
public:
    bool create(BuildAllocation& allocation);
    std::optional<BuildAllocation> getById(int id) const;
    QList<BuildAllocation> getByBuild(int buildId) const;
    QList<BuildAllocation> getByRequirement(int buildRequirementId) const;
    QList<BuildAllocation> getByInventoryRecord(int inventoryRecordId) const;
    bool update(BuildAllocation& allocation);
    bool remove(int allocationId);
    bool removeAllForBuild(int buildId);
    bool removeAllForRequirement(int buildRequirementId);
    int totalAllocatedForRequirement(int buildRequirementId) const;
    int totalAllocatedForPartColor(int workspaceId, int partId, int colorId) const;
    int totalAllocatedForPartColorForBuild(int buildId, int partId, int colorId) const;
    int totalAllocatedForInventoryRecord(int inventoryRecordId) const;
    int totalAllocatedForInventoryRecordForBuild(int inventoryRecordId, int buildId) const;

    bool recordPulledManufacturer(int buildId,
                                  int partId,
                                  int colorId,
                                  int manufacturerId,
                                  int quantity);

    QList<BuildPartManufacturerProvenance> pulledManufacturerProvenance(
        int buildId,
        int partId,
        int colorId) const;

    bool reducePulledManufacturer(int buildId,
                                  int partId,
                                  int colorId,
                                  int manufacturerId,
                                  int quantity);

private:
    BuildAllocation allocationFromQuery(const QSqlQuery& query) const;
};
