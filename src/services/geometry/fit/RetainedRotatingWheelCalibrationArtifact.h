#pragma once

#include "FitCalibrationExperiment.h"
#include "../LDrawLoadResult.h"
#include "../print/PrintMesh.h"

namespace PrintGeometry {
struct RetainedRotatingWheelCalibrationResult {
    QString artifactIdentity,orientationIdentity,diagnostic;
    QVector<FitCalibrationCandidate> candidates;
    QVector<PrintMesh> candidateMeshes;
    FunctionalFeature regenerationPrototype;
    bool ok=false;
};
class RetainedRotatingWheelCalibrationArtifact {
public:
    static QString artifactIdentity();
    static RetainedRotatingWheelCalibrationResult generate(const LDrawGeometry::LDrawLoadResult& femaleSource);
    static FitCalibrationExperiment observationTemplate(const RetainedRotatingWheelCalibrationResult& result);
};
}
