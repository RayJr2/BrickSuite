#pragma once

#include <QString>

class SetBuildCreationService
{
public:
    struct Result
    {
        bool success = false;
        int buildId = 0;
        int requirementRows = 0;
        int requiredPieces = 0;
        int excludedSparePieces = 0;
        QString message;
    };

    Result create(int workspaceId, int setCatalogId, const QString& buildName) const;
};
