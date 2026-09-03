#pragma once

#include "CollectionItem.h"

#include <QString>

struct CollectionSearchResult
{
    CollectionItem item;
    QString displayReference;
    QString displayName;
    QString imageUrl;
    QString locationName;
    QString sourceBuildName;
    QString sourceBuildReference;
};
