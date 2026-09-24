#pragma once

#include "FitCalibrationExperiment.h"
#include "../LDrawLoadResult.h"
#include "../print/PrintMesh.h"

namespace PrintGeometry {
struct InterleavedFingerHingeCalibrationResult {
    QString artifactIdentity,orientationIdentity,diagnostic;
    QVector<FitCalibrationCandidate> candidates;
    QVector<PrintMesh> candidateMeshes;
    FunctionalFeature regenerationPrototype;
    bool ok=false;
};
class InterleavedFingerHingeCalibrationArtifact {
public:
    static QString artifactIdentity();
    static InterleavedFingerHingeCalibrationResult generate(const LDrawGeometry::LDrawLoadResult& source);
    static FitCalibrationExperiment observationTemplate(const InterleavedFingerHingeCalibrationResult& result);
};
}
