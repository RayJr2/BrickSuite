#pragma once

#include <QString>

class RebrickableMinifigPartsImporter
{
public:
    struct Result
    {
        bool success = false;
        int rowsRead = 0;
        int compositionRows = 0;
        QString message;
    };

    Result importFile(int minifigCatalogId, const QString& fileName);
};
