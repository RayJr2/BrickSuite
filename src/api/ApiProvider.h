/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 */
#pragma once

#include <QString>

enum class ApiProvider
{
    Rebrickable,
    BrickLink,
    Brickset
};

inline QString apiProviderName(ApiProvider provider)
{
    switch (provider) {
    case ApiProvider::Rebrickable:
        return QStringLiteral("Rebrickable");
    case ApiProvider::BrickLink:
        return QStringLiteral("BrickLink");
    case ApiProvider::Brickset:
        return QStringLiteral("Brickset");
    }

    return QStringLiteral("Unknown");
}

inline QString apiProviderKey(ApiProvider provider)
{
    switch (provider) {
    case ApiProvider::Rebrickable:
        return QStringLiteral("rebrickable");
    case ApiProvider::BrickLink:
        return QStringLiteral("bricklink");
    case ApiProvider::Brickset:
        return QStringLiteral("brickset");
    }

    return QStringLiteral("unknown");
}
