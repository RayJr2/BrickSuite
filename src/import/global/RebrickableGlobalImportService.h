#pragma once

#include "RebrickableImportCancellation.h"
#include "RebrickableImportTypes.h"

#include <functional>

class QSqlDatabase;

class RebrickableGlobalImportService
{
public:
    using Progress = std::function<void(const RebrickableImportProgress&)>;

    RebrickableImportPlan run(RebrickableImportPlan plan, QSqlDatabase& database,
                              const RebrickableImportCancellation& cancellation,
                              const Progress& progress = {}) const;
};
