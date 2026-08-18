/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 */
#pragma once

#include "ApiProvider.h"

#include <QSet>
#include <QString>

enum class ApiLogPolicy
{
    Normal,
    ErrorsOnly
};

struct ApiRequestContext
{
    ApiProvider provider = ApiProvider::Rebrickable;
    QString operation;
    QString requestId;
    QSet<int> expectedHttpStatusCodes;
    ApiLogPolicy logPolicy = ApiLogPolicy::Normal;
};
