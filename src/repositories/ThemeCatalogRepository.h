#pragma once

#include "../models/ThemeCatalogItem.h"

#include <QList>

class ThemeCatalogRepository
{
public:
    QList<ThemeCatalogItem> activeFilterHierarchy() const;
};
