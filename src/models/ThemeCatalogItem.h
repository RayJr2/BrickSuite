#pragma once

#include <QString>

struct ThemeCatalogItem
{
    int id = 0;
    QString name;
    int parentThemeCatalogId = 0;
    QString provider;
    QString externalId;
    QString qualifiedName;
    int depth = 0;
};
