#pragma once

#include <QString>

struct PartSearchCriteria
{
    QString searchText;

    int categoryId = 0;

    int limit = 200;
    int offset = 0;
};
