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
#include "../LDrawLoadResult.h"
#include "../print/PrintMesh.h"

namespace PrintGeometry {
struct PlainRoundBoreWheelCalibrationResult {
    QString artifactIdentity,parentArtifactIdentity,orientationIdentity,diagnostic;
    QVector<FitCalibrationCandidate> candidates;
    QVector<PrintMesh> candidateMeshes;
    FunctionalFeature regenerationPrototype;
    bool ok=false;
};
struct PlainRoundBoreWheelCalibrationDefinition {
    QString artifactIdentity,parentArtifactIdentity;
    double centerCorrectionMillimetres=0.0,candidateSpacingMillimetres=.1;
    int candidateCount=7;
};
class PlainRoundBoreWheelCalibrationArtifact {
public:
    static QString artifactIdentity();
    static PlainRoundBoreWheelCalibrationResult generate(const LDrawGeometry::LDrawLoadResult& femaleSource);
    static PlainRoundBoreWheelCalibrationResult generate(const LDrawGeometry::LDrawLoadResult& femaleSource,
        const FunctionalFeature& prototype,const PlainRoundBoreWheelCalibrationDefinition& definition);
    static FitCalibrationExperiment observationTemplate(const PlainRoundBoreWheelCalibrationResult& result);
};
}
