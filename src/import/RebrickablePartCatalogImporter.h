#pragma once

#include <QString>

class RebrickablePartCatalogImporter
{
public:
    struct Result
    {
        bool success = false;

        int rowsRead = 0;
        int inserted = 0;
        int updated = 0;
        int unchanged = 0;
        int skipped = 0;

        QString message;
    };

    Result importFile(const QString& fileName);
};
