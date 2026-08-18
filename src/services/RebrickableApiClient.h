/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 *
 * Compatibility facade retained during M14.2. New provider-specific
 * implementation lives in src/api/rebrickable/RebrickableService.
 */
#pragma once

#include "../api/rebrickable/RebrickableService.h"

class RebrickableApiClient : public RebrickableService
{
    Q_OBJECT

public:
    explicit RebrickableApiClient(QObject* parent = nullptr);
};
