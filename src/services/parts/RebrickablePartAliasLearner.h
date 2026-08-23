/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 */
#pragma once

#include "../../api/rebrickable/RebrickableService.h"

#include <QString>

class RebrickablePartAliasLearner
{
public:
    struct Result
    {
        bool learned = false;
        bool ambiguous = false;

        int canonicalPartId = 0;

        QString requestedPartNumber;
        QString canonicalPartNumber;
        QString message;
    };

    Result learnFromLocalExternalId(
        const QString& requestedPartNumber) const;

    Result learn(
        const RebrickableService::PartDetailsResult& providerResult) const;
};
