#pragma once

#include "FitCalibrationExperiment.h"
#include "../LDrawLoadResult.h"
#include "../print/PrintMesh.h"

namespace PrintGeometry {
struct InterleavedFingerHingeCalibrationDefinition {
    QString artifactIdentity,parentArtifactIdentity;
    int candidateCount=7;
    double centerCorrectionMillimetres=0,spacingMillimetres=.05;
};
struct InterleavedFingerHingeCalibrationResult {
    QString artifactIdentity,parentArtifactIdentity,orientationIdentity,diagnostic;
    QVector<FitCalibrationCandidate> candidates;
    QVector<PrintMesh> candidateMeshes;
    FunctionalFeature regenerationPrototype;
    bool ok=false;
};
class InterleavedFingerHingeCalibrationArtifact {
public:
    static QString artifactIdentity();
    static InterleavedFingerHingeCalibrationResult generate(const LDrawGeometry::LDrawLoadResult& source, const InterleavedFingerHingeCalibrationDefinition& definition={});
    static FitCalibrationExperiment observationTemplate(const InterleavedFingerHingeCalibrationResult& result, const InterleavedFingerHingeCalibrationDefinition& definition={});
};
}
