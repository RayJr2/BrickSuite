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
struct StudReceivingCalibrationArtifactDefinition {
    QString artifactIdentity,parentArtifactIdentity;
    double centerDiameterCorrectionMillimetres=0.0,candidateSpacingMillimetres=0.1;
    int candidateCount=7;
};
struct StudReceivingCalibrationArtifactResult {
    QString artifactIdentity,parentArtifactIdentity,orientationIdentity,diagnostic;
    QVector<FitCalibrationCandidate> candidates;
    PrintMesh mesh; MeshAnalysisResult analysis;
    FunctionalFeature regenerationPrototype;
    bool ok=false;
};
class StudReceivingCalibrationArtifact {
public:
    static FunctionalFeature canonicalPrototype();
    static FunctionalFeature canonicalPostWallPrototype();
    static FunctionalFeature canonicalWallPocketPrototype(bool brickDepth);
    static FunctionalFeature canonicalAntiStudBorePrototype();
    static QString artifactIdentity();
    static QString postWallArtifactIdentity();
    static QString wallPocketArtifactIdentity(bool brickDepth);
    static QString antiStudBoreArtifactIdentity();
    static QString orientationIdentity();
    static QVector<double> diameterCorrectionsMillimetres();
    static StudReceivingCalibrationArtifactResult generate();
    static StudReceivingCalibrationArtifactResult generateMarkedTubeWallCell();
    static StudReceivingCalibrationArtifactResult generatePostWallCell();
    static StudReceivingCalibrationArtifactResult generateWallPocket(bool brickDepth);
    static StudReceivingCalibrationArtifactResult generateWallPocket(const FunctionalFeature&,
                                                                    const StudReceivingCalibrationArtifactDefinition&);
    static StudReceivingCalibrationArtifactResult generateAntiStudBore();
    static StudReceivingCalibrationArtifactResult generateAntiStudBore(const FunctionalFeature&,
                                                                        const StudReceivingCalibrationArtifactDefinition&);
    static StudReceivingCalibrationArtifactResult generate(const FunctionalFeature&,const StudReceivingCalibrationArtifactDefinition&);
    static FitCalibrationExperiment observationTemplate(const StudReceivingCalibrationArtifactResult&);
    static FitCalibrationExperiment observationTemplate(const StudReceivingCalibrationArtifactResult&,const StudReceivingCalibrationArtifactDefinition&);
};
}
