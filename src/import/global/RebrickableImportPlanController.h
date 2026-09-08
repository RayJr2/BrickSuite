#pragma once

#include "RebrickableImportTypes.h"

class RebrickableImportPlanController
{
public:
    static bool beginDataset(RebrickableImportPlan& plan, RebrickableDatasetId dataset);
    static void completeDataset(RebrickableImportPlan& plan, RebrickableDatasetId dataset,
                                const RebrickableImportCounters& counters,
                                qint64 elapsedMilliseconds, bool noChanges);
    static void failDataset(RebrickableImportPlan& plan, RebrickableDatasetId dataset,
                            const QString& message, bool cancelled);
};

