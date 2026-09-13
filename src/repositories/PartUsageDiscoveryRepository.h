#pragma once

#include "RepositoryConnection.h"
#include "../models/PartUsageDiscovery.h"

class PartUsageDiscoveryRepository : protected RepositoryConnection
{
public:
    explicit PartUsageDiscoveryRepository(const QSqlDatabase& database)
        : RepositoryConnection(database) {}

    PartUsageSearchResult search(const PartUsageSearch& request) const;
};
