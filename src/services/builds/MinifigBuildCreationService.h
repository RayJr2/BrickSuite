#pragma once

#include <QString>

class MinifigBuildCreationService
{
public:
    struct Result
    {
        bool success = false;
        int buildId = 0;
        int requirementRows = 0;
        int requiredPieces = 0;
        QString message;
    };

    Result create(int workspaceId, int minifigCatalogId, const QString& buildName) const;
};
