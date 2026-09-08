#pragma once

#include "RebrickableImportTypes.h"

struct RebrickableDatasetDescriptor {
    RebrickableDatasetId id;
    QString displayName;
    QString csvFileName;
    QStringList requiredHeaders;
    QVector<RebrickableDatasetId> hardDependencies;
    int importOrder = 0;
    bool importerImplemented = false;
    bool requiresSchema33 = false;
    bool exactHeaders = false;
};

class RebrickableDatasetRegistry
{
public:
    static const QVector<RebrickableDatasetDescriptor>& datasets();
    static const RebrickableDatasetDescriptor* descriptor(RebrickableDatasetId id);
};
