/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 */
#pragma once

#include <QString>
#include "global/RebrickableImportCancellation.h"

class QSqlDatabase;

class RebrickablePartRelationshipImporter
{
public:
    struct Result
    {
        bool success = false;

        int rowsRead = 0;
        int inserted = 0;
        int updated = 0;
        int unchanged = 0;

        int skippedInvalid = 0;
        int skippedMissingParent = 0;
        int skippedMissingChild = 0;
        int selfReferencesIgnored = 0;

        int deactivated = 0;

        QString message;
    };

    Result importFile(const QString& fileName);
    Result importFile(const QString& fileName, QSqlDatabase& database,
                      const RebrickableImportCancellation* cancellation = nullptr,
                      const RebrickableRowProgress& progress = {},
                      bool requireCompleteSnapshot = false);
};
