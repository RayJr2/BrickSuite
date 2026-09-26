#pragma once

#include "FitCalibrationExperiment.h"
#include "../LDrawLoadResult.h"
#include "../print/PrintMesh.h"

namespace PrintGeometry {
struct ClickHingeCalibrationDefinition {
    QString artifactIdentity,parentArtifactIdentity;
    int candidateCount=7;
    double centerCorrectionMillimetres=0,spacingMillimetres=.05;
};
struct ClickHingeCalibrationResult {
    QString artifactIdentity,parentArtifactIdentity,orientationIdentity,diagnostic;
    QVector<FitCalibrationCandidate> candidates;
    QVector<PrintMesh> candidateMeshes;
    FunctionalFeature regenerationPrototype;
    bool ok=false;
};
class ClickHingeCalibrationArtifact {
public:
    static QString artifactIdentity();
    static ClickHingeCalibrationResult generate(const LDrawGeometry::LDrawLoadResult& source, const ClickHingeCalibrationDefinition& definition={});
    static FitCalibrationExperiment observationTemplate(const ClickHingeCalibrationResult& result, const ClickHingeCalibrationDefinition& definition={});
};
}
