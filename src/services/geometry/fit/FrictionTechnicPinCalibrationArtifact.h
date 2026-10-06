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
namespace PrintGeometry {
struct FrictionTechnicPinCalibrationResult {QString artifactIdentity,parentArtifactIdentity,orientationIdentity,diagnostic;QVector<FitCalibrationCandidate>candidates;PrintMesh mesh;MeshAnalysisResult analysis;FunctionalFeature regenerationPrototype;bool ok=false;};
struct FrictionTechnicPinCalibrationArtifactDefinition {QString artifactIdentity,parentArtifactIdentity;double centerRidgeEnvelopeCorrectionMillimetres=0.0,candidateSpacingMillimetres=.05;int candidateCount=7;};
class FrictionTechnicPinCalibrationArtifact {
public:
    static QString artifactIdentity();
    static QVector<double> ridgeEnvelopeCorrectionsMillimetres();
    static FrictionTechnicPinCalibrationResult generate();
    static FrictionTechnicPinCalibrationResult generate(const FunctionalFeature&,const FrictionTechnicPinCalibrationArtifactDefinition&);
    static FitCalibrationExperiment observationTemplate(const FrictionTechnicPinCalibrationResult&);
};
}
