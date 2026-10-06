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

#include "../print/PrintMesh.h"
#include <QString>

namespace PrintGeometry {
enum class FitCalibrationNameKey;

struct FitFixtureLabelRegion {
    double minimumX = 0;
    double maximumX = 0;
    double minimumY = 0;
    double maximumY = 0;
};

class FitCalibrationFixtureLabel {
public:
    // Cuts lettering into the underside (z = 0) of an already validated flat-base fixture.
    // The caller supplies an explicitly protected base region, not a mating surface.
    static bool recess(const PrintMesh& source, const QString& displayLabel,
                       const QString& shortLabel, const FitFixtureLabelRegion& region,
                       PrintMesh* labeled, QString* appliedLabel,
                       QString* error = nullptr);
    // Uses certified underside strips; it does not alter candidate or marker geometry.
    static bool recessStandalone(const PrintMesh& source, FitCalibrationNameKey key,
                                 PrintMesh* labeled, QString* appliedLabel,
                                 QString* error = nullptr);
};

} // namespace PrintGeometry
