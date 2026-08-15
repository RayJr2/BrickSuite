#pragma once

#include <QList>
#include <QString>

class MissingPartsService
{
public:
    struct MissingPart
    {
        int partId = 0;
        int colorId = 0;

        QString partNumber;
        QString partName;
        QString colorName;

        int required = 0;
        int pulled = 0;
        int remaining = 0;

        int owned = 0;
        int thisBuildAllocated = 0;
        int otherBuildsAllocated = 0;
        int available = 0;

        int missing = 0;
    };

    QList<MissingPart> getMissingParts(
        int workspaceId,
        int buildId) const;
};