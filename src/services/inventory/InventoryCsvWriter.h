#pragma once

#include "../../models/export/InventoryExportTypes.h"

class InventoryCsvWriter
{
public:
    struct Result { bool success=false; QString csv; QString message; };
    static Result generate(const InventoryExportProjection& projection);
    static Result write(const QString& fileName, const InventoryExportProjection& projection);
};
