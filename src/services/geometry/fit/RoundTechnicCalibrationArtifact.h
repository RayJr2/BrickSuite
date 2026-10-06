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

struct RoundTechnicCalibrationArtifactResult {
    QString artifactIdentity;
    QString orientationIdentity;
    QString diagnostic;
    QVector<FitCalibrationCandidate> candidates;
    PrintMesh mesh;
    MeshAnalysisResult analysis;
    FunctionalFeature regenerationPrototype;
    bool ok = false;
};

struct RoundTechnicCalibrationArtifactDefinition {
    QString artifactIdentity;
    QString parentArtifactIdentity;
    double centerDiameterCorrectionMillimetres = 0.0;
    double candidateSpacingMillimetres = 0.1;
    int candidateCount = 7;
    FitPrintedOrientation printedOrientation = FitPrintedOrientation::Unknown;
};

class RoundTechnicCalibrationArtifact {
public:
    static QString artifactIdentity();
    static QString orientationIdentity();
    static QString parallelArtifactIdentity();
    static RoundTechnicCalibrationArtifactDefinition parallelCoarseDefinition();
    static FunctionalFeature canonicalPrototype();
    static QVector<double> diameterCorrectionsMillimetres();
    static RoundTechnicCalibrationArtifactResult generate(const FunctionalFeature& prototype);
    static RoundTechnicCalibrationArtifactResult generate(const FunctionalFeature& prototype,
                                                           const RoundTechnicCalibrationArtifactDefinition& definition);
    static FitCalibrationExperiment observationTemplate(const RoundTechnicCalibrationArtifactResult& artifact);
    static FitCalibrationExperiment observationTemplate(const RoundTechnicCalibrationArtifactResult& artifact,
                                                        const RoundTechnicCalibrationArtifactDefinition& definition);
};

} // namespace PrintGeometry
