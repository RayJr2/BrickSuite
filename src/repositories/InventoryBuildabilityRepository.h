#pragma once

#include "RepositoryConnection.h"
#include "../models/InventoryBuildability.h"

class InventoryBuildabilityRepository : protected RepositoryConnection
{
public:
    explicit InventoryBuildabilityRepository(const QSqlDatabase& database)
        : RepositoryConnection(database) {}

    InventoryBuildabilitySearchResult search(const InventoryBuildabilitySearch& request) const;
};
