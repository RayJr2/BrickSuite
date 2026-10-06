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

#include "../fit/FitCalibrationLibrary.h"
#include "LDrawPrintGeometryBuilder.h"
#include "../PrintOrientation.h"
#include <QString>
#include <QVector>

namespace PrintGeometry {
enum class AutoFitResolutionState { Disabled, UnsupportedPart, NoCompatibleProfile, Ambiguous, Resolved };
struct AutoFitProfileResolution {
    AutoFitResolutionState state = AutoFitResolutionState::Disabled;
    FitProfile profile;
    QString diagnostic;
    bool resolved() const { return state == AutoFitResolutionState::Resolved; }
};
class AutoFitProfileResolver {
public:
    static AutoFitProfileResolution resolveExplicit(const FitProfile&,
        const QString& partReference,const LDrawGeometry::LDrawLoadResult&,
        const PrintOrientation&);
    static AutoFitProfileResolution resolve(bool enabled, const QString& partReference,
                                            const QVector<FitProfile>& candidates,
                                            const LDrawSemanticOperandBuilder::Result& semantics,
                                            const PrintOrientation& printOrientation);
    static AutoFitProfileResolution resolve(bool enabled, const QString& partReference,
                                            const QVector<FitProfile>& candidates,
                                            const LDrawGeometry::LDrawLoadResult& source,
                                            const PrintOrientation& printOrientation);
    static AutoFitProfileResolution resolve(bool enabled, const QString& partReference,
                                            const QVector<FitProfile>& candidates,
                                            FitPrintedOrientation orientation);
    static AutoFitProfileResolution resolveManaged(bool enabled, const QString& partReference,
                                                   const FitCalibrationLibrary& library,
                                                   const LDrawGeometry::LDrawLoadResult& source,
                                                   const PrintOrientation& printOrientation);
};
}
