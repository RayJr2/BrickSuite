#pragma once

#include <QString>

struct SetCatalogSearchCriteria
{
    QString searchText;

    int year = 0;
    int themeId = 0;

    int limit = 250;
    int offset = 0;
};