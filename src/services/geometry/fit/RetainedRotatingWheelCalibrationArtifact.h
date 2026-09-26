#pragma once

#include "FitCalibrationExperiment.h"
#include "../LDrawLoadResult.h"
#include "../print/PrintMesh.h"

namespace PrintGeometry {
struct RetainedRotatingWheelCalibrationDefinition {
    QString artifactIdentity,parentArtifactIdentity;
    int candidateCount=7;
    double centerCorrectionMillimetres=0,spacingMillimetres=.05;
};
struct RetainedRotatingWheelCalibrationResult {
    QString artifactIdentity,parentArtifactIdentity,orientationIdentity,diagnostic;
    QVector<FitCalibrationCandidate> candidates;
    QVector<PrintMesh> candidateMeshes;
    FunctionalFeature regenerationPrototype;
    bool ok=false;
};
class RetainedRotatingWheelCalibrationArtifact {
public:
    static QString artifactIdentity();
    static RetainedRotatingWheelCalibrationResult generate(const LDrawGeometry::LDrawLoadResult& femaleSource, const RetainedRotatingWheelCalibrationDefinition& definition={});
    static FitCalibrationExperiment observationTemplate(const RetainedRotatingWheelCalibrationResult& result, const RetainedRotatingWheelCalibrationDefinition& definition={});
};
}
