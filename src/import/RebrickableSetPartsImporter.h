#pragma once

#include <QString>

class RebrickableSetPartsImporter
{
public:
    struct Result { bool success = false; int rowsRead = 0; int compositionRows = 0; QString message; };
    Result importFile(int setCatalogId, const QString& fileName);
};
