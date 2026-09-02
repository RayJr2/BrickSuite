#pragma once

#include <QString>

struct MinifigExternalIdentifier
{
    int id = 0;
    int minifigCatalogId = 0;
    QString provider;
    QString externalId;
    QString source;
    bool isActive = true;
};
