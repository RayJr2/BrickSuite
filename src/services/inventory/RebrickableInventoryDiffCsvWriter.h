/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 */
#pragma once

#include "../../import/InventoryImportTypes.h"

#include <QString>

class RebrickableInventoryDiffCsvWriter
{
public:
    enum class DeltaType
    {
        Append,
        Subtract
    };

    struct Result
    {
        bool success = false;
        QString csv;
        QString message;
        int rows = 0;
        int pieces = 0;
    };

    Result write(const InventoryImportPreview& preview,
                 DeltaType deltaType) const;

    static QString suggestedFileName(
        const InventoryImportPreview& preview,
        DeltaType deltaType);

private:
    static QString csvField(const QString& value);
};
