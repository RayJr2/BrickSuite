#pragma once

#include "FitCalibrationExperiment.h"
#include "../LDrawLoadResult.h"
#include "../print/PrintMesh.h"

namespace PrintGeometry {
struct PinBarrelHingeCalibrationDefinition {
    QString artifactIdentity,parentArtifactIdentity;
    int candidateCount=7;
    double centerCorrectionMillimetres=0.0,spacingMillimetres=.05;
    FitPrintedOrientation orientation=FitPrintedOrientation::FeatureAxisParallelToBuildPlate;
};
struct PinBarrelHingeCalibrationResult {
    QString artifactIdentity,parentArtifactIdentity,orientationIdentity,diagnostic;
    QVector<FitCalibrationCandidate> candidates;
    QVector<PrintMesh> candidateMeshes;
    FunctionalFeature regenerationPrototype;
    bool ok=false;
};
class PinBarrelHingeCalibrationArtifact {
public:
    static QString artifactIdentity();
    static PinBarrelHingeCalibrationResult generate(const LDrawGeometry::LDrawLoadResult& maleSource,
        const PinBarrelHingeCalibrationDefinition& definition={});
    static FitCalibrationExperiment observationTemplate(const PinBarrelHingeCalibrationResult& result,
        const PinBarrelHingeCalibrationDefinition& definition={});
};
}
