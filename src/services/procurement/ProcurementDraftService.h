/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 */
#pragma once

#include "../../models/procurement/ProcurementDraft.h"

#include <QString>

class ProcurementDraftService
{
public:
    struct Result
    {
        bool success = false;
        QString message;
        ProcurementDraft draft;
    };

    Result createBrickLinkDraft(int workspaceId, int buildId) const;
};
