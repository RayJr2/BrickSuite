#pragma once

#include "FitCalibrationExperiment.h"

namespace PrintGeometry {
struct BallJointCalibrationDefinition {
    QString artifactIdentity,parentArtifactIdentity;
    int candidateCount=7;
    double centerCorrectionMillimetres=0.0,spacingMillimetres=0.10;
};
struct BallJointCalibrationResult {
    QString artifactIdentity,parentArtifactIdentity,orientationIdentity,diagnostic;
    QVector<FitCalibrationCandidate> candidates;
    PrintMesh mesh;
    MeshAnalysisResult analysis;
    FunctionalFeature regenerationPrototype;
    bool ok=false;
};
class BallJointCalibrationArtifact {
public:
    static QString artifactIdentity();
    static FunctionalFeature canonicalPrototype();
    static BallJointCalibrationResult generate(const BallJointCalibrationDefinition& definition={});
    static FitCalibrationExperiment observationTemplate(const BallJointCalibrationResult& result,
                                                        const BallJointCalibrationDefinition& definition={});
};
}
