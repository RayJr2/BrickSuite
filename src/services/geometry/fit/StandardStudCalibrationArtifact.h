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

#include "FitCalibrationExperiment.h"
#include "../print/FunctionalOperandRegenerator.h"

namespace PrintGeometry {

enum class StandardStudCalibrationDimension { Diameter, Height };

struct StandardStudCalibrationArtifactDefinition {
    QString artifactIdentity;
    QString parentArtifactIdentity;
    StandardStudCalibrationDimension dimension = StandardStudCalibrationDimension::Diameter;
    double centerCorrectionMillimetres = 0.0;
    double candidateSpacingMillimetres = 0.1;
    double fixedDiameterCorrectionMillimetres = 0.0;
    int candidateCount = 7;
};

struct StandardStudCalibrationArtifactResult {
    QString artifactIdentity;
    QString orientationIdentity;
    StandardStudCalibrationDimension dimension = StandardStudCalibrationDimension::Diameter;
    QString diagnostic;
    QVector<FitCalibrationCandidate> candidates;
    PrintMesh mesh;
    MeshAnalysisResult analysis;
    FunctionalFeature regenerationPrototype;
    bool ok = false;
};

class StandardStudCalibrationArtifact {
public:
    static FunctionalFeature canonicalPrototype();
    static QString diameterArtifactIdentity();
    static QString heightArtifactIdentity();
    static QString orientationIdentity();
    static bool heightVerificationDefinition(
        const FitCalibrationExperiment& provisionalHeight,
        double verifiedDiameterCorrectionMillimetres,
        StandardStudCalibrationArtifactDefinition*,
        QString* error = nullptr);
    static StandardStudCalibrationArtifactResult generate(
        const FunctionalFeature&, const StandardStudCalibrationArtifactDefinition&);
    static FitCalibrationExperiment observationTemplate(
        const StandardStudCalibrationArtifactResult&,
        const StandardStudCalibrationArtifactDefinition&);
};

} // namespace PrintGeometry
