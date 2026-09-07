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
    enum class Origin { BuiltIn, User };

    int userEntryId = 0;
    int partId = 0;
    Origin origin = Origin::BuiltIn;
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
