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

#include <QString>

class RebrickableMinifigThemeImporter
{
public:
    struct Result {
        bool success = false;
        QString message;
        int themesRead = 0;
        int themesInserted = 0;
        int themesUpdated = 0;
        int themesReactivated = 0;
        int themesDeactivated = 0;
        int relationshipRowsRead = 0;
        int associations = 0;
        int unresolvedMinifigs = 0;
        int duplicateRelationshipsCollapsed = 0;
    };

    Result importDirectory(const QString& directoryPath);
};
