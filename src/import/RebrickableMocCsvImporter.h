#pragma once

#include <QString>

class RebrickableMocCsvImporter
{
public:
    struct Result
    {
        bool success = false;

        int rowsRead = 0;
        int requirementsCreated = 0;
        int regularPieces = 0;
        int sparePieces = 0;

        QString message;
    };

    Result importFile(int buildId, const QString& fileName, bool replaceExisting);
};