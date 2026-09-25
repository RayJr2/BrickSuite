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
