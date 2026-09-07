/* BrickSuite - The Digital Twin Platform for Your Brick Workshop */
#pragma once

#include <QDateTime>
#include <QString>

enum class PartReferencePlacement { Before, After, Append };

struct UserPartReferenceEntry
{
    int id = 0;
    int partId = 0;
    QString catalog;
    QString section;
    QString anchorPartNumber;
    PartReferencePlacement placement = PartReferencePlacement::Append;
    QDateTime createdUtc;
    QDateTime modifiedUtc;
};
