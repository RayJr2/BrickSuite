/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 *
 * This file is part of BrickSuite.
 */

#pragma once

#include <QString>

struct PartReferenceEntry
{
    QString partNumber;
    QString partName;
    QString catalog;
    QString section;
    int displayOrder = 0;
    int sourceCategoryId = 0;
    QString sourceCategory;
    QString material;
    QString representativeFor;
    QString notes;
};
