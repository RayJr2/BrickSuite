/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 */
#pragma once

#include <QString>

struct BrickLinkWantedListOptions
{
    QString condition;
    QString notify;
    QString wantedShow;

    QString remarksMode;
    QString customRemarks;
};
