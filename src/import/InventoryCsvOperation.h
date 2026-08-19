/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 */
#pragma once

#include <QString>

enum class InventoryCsvOperation
{
    Append,
    Replace,
    Subtract,
    CompareOnly
};

inline QString inventoryCsvOperationName(InventoryCsvOperation operation)
{
    switch (operation) {
    case InventoryCsvOperation::Append:
        return QStringLiteral("Append");
    case InventoryCsvOperation::Replace:
        return QStringLiteral("Replace");
    case InventoryCsvOperation::Subtract:
        return QStringLiteral("Subtract");
    case InventoryCsvOperation::CompareOnly:
        return QStringLiteral("Compare Only");
    }

    return QStringLiteral("Append");
}
