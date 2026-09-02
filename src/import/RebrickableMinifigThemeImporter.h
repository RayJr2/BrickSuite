#pragma once

#include <QString>

class RebrickableMinifigThemeImporter
{
public:
    struct Result {
        bool success = false;
        QString message;
        int themesRead = 0;
        int themesInserted = 0;
        int themesUpdated = 0;
        int themesReactivated = 0;
        int themesDeactivated = 0;
        int relationshipRowsRead = 0;
        int associations = 0;
        int unresolvedMinifigs = 0;
        int duplicateRelationshipsCollapsed = 0;
    };

    Result importDirectory(const QString& directoryPath);
};
