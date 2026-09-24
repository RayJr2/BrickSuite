#pragma once

#include "FitCalibrationExperiment.h"
#include "../LDrawLoadResult.h"
#include "../print/PrintMesh.h"

namespace PrintGeometry {
struct BallSocketCalibrationDefinition {
    QString artifactIdentity,parentArtifactIdentity;
    int candidateCount=7;
    double centerCorrectionMillimetres=0.0,spacingMillimetres=0.10;
    FitPrintedOrientation orientation=FitPrintedOrientation::FeatureAxisParallelToBuildPlate;
};
struct BallSocketCalibrationResult {
    QString artifactIdentity,parentArtifactIdentity,orientationIdentity,diagnostic;
    QVector<FitCalibrationCandidate> candidates;
    QVector<PrintMesh> candidateMeshes;
    FunctionalFeature regenerationPrototype;
    bool ok=false;
};
class BallSocketCalibrationArtifact {
public:
    static QString artifactIdentity();
    static BallSocketCalibrationResult generate(const LDrawGeometry::LDrawLoadResult& source,
                                                const BallSocketCalibrationDefinition& definition={});
    static FitCalibrationExperiment observationTemplate(const BallSocketCalibrationResult& result,
                                                        const BallSocketCalibrationDefinition& definition={});
};
}
