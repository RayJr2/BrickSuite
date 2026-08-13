#pragma once

#include "../models/BuildAllocation.h"

#include <QList>
#include <optional>

class QSqlQuery;

class BuildAllocationRepository
{
public:
    bool create(BuildAllocation& allocation);

    std::optional<BuildAllocation> getById(int id) const;

    QList<BuildAllocation> getByBuild(int buildId) const;

    QList<BuildAllocation> getByInventoryRecord(int inventoryRecordId) const;

    bool update(BuildAllocation& allocation);

    bool remove(int allocationId);

    bool removeAllForBuild(int buildId);

    int totalAllocatedForPartColor(int workspaceId, int partId, int colorId) const;

    int totalAllocatedForPartColorForBuild(int buildId, int partId, int colorId) const;

private:
    BuildAllocation allocationFromQuery(const QSqlQuery& query) const;
};