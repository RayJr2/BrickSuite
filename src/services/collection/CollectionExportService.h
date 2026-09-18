#pragma once

#include "../../models/CollectionSearchResult.h"
#include "../../models/export/CollectionExportTypes.h"

namespace RemoteReadDto { struct CollectionExportRow; }

class CollectionExportService
{
public:
    static QList<CollectionExportRow> createRows(const QList<CollectionSearchResult>& source);
    static QList<CollectionExportRow> createRemoteRows(const QList<RemoteReadDto::CollectionExportRow>& source);
    static QList<CollectionExportFieldDescriptor> fieldDescriptors();
    static CollectionExportConfiguration defaultConfiguration();
    static CollectionExportConfiguration normalizeConfiguration(const QStringList& order,const QStringList& enabled);
    static CollectionExportProjection project(const QList<CollectionExportRow>& rows,
                                               const CollectionExportConfiguration& configuration,
                                               int maximumRows=-1);
};
