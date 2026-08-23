/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 */
#pragma once

#include "../models/PartAlias.h"

#include <QList>
#include <optional>

class PartAliasRepository
{
public:
    std::optional<PartAlias> getByAliasPartNumber(
        const QString& aliasPartNumber,
        bool activeOnly = true) const;

    QList<PartAlias> getByPartId(
        int partId,
        bool activeOnly = true) const;

    bool upsert(const PartAlias& alias) const;
    bool setActive(int aliasId, bool active) const;
};
