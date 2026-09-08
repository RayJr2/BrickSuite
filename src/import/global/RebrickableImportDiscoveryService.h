#pragma once

#include "RebrickableImportTypes.h"

#include <QString>

class RebrickableImportDiscoveryService
{
public:
    RebrickableImportPlan buildPlan(const QString& directoryPath) const;

private:
    void preflight(RebrickableImportPlanEntry& entry) const;
    void applyDependencies(RebrickableImportPlan& plan) const;
};

