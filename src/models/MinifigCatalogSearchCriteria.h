#pragma once

#include <QString>

struct MinifigCatalogSearchCriteria
{
    QString searchText;
    QString provider;
    int themeCatalogId = 0;
    bool includeInactive = false;
    int limit = 250;
    int offset = 0;
};
