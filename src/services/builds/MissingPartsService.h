/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 *
 * This file is part of BrickSuite.
 *
 * BrickSuite is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, version 3 of the License.
 *
 * BrickSuite is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with BrickSuite. If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <QList>
#include <QString>

class MissingPartsService
{
public:
    struct MissingPart
    {
        int partId = 0;
        int colorId = 0;

        QString partNumber;
        QString partName;
        QString colorName;

        int required = 0;
        int pulled = 0;
        int remaining = 0;

        int owned = 0;
        int thisBuildAllocated = 0;
        int otherBuildsAllocated = 0;
        int available = 0;

        int missing = 0;
    };

    QList<MissingPart> getMissingParts(
        int workspaceId,
        int buildId) const;
};