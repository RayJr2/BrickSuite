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

#include "FitCalibrationFamilyCatalog.h"
#include "../../services/geometry/fit/FitCalibrationCapabilities.h"
#include <QSet>
QVector<FitCalibrationFamilyOption> FitCalibrationFamilyCatalog::availableFamilies()
{
    QVector<FitCalibrationFamilyOption> result;QSet<QString> seen;
    for(const auto& capability:PrintGeometry::FitCalibrationCapabilities::entries()) {
        if(!capability.initial||seen.contains(capability.family))continue;
        seen.insert(capability.family);
        result.push_back({FitCalibrationFamily(result.size()),capability.name,capability.family});
    }
    return result;
}
