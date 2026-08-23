/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 */
#pragma once

#include "../../models/PartResolutionResult.h"

#include <QString>

class PartResolver
{
public:
    PartResolutionResult resolve(const QString& partNumber) const;

private:
    QList<PartResolutionCandidate> relationshipCandidatesForPart(
        int partId) const;
};
