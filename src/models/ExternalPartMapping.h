/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 */
#pragma once

#include "ExternalMappingStatus.h"

#include <QString>

struct ExternalPartMapping
{
    int id = 0;
    int partId = 0;

    QString provider;
    QString externalId;

    ExternalMappingStatus status = ExternalMappingStatus::Unknown;

    QString source;
    QString notes;
};
