#pragma once

#include "FitCalibrationExperiment.h"
#include "../LDrawLoadResult.h"
#include "../print/PrintMesh.h"

namespace PrintGeometry {
struct ClickHingeCalibrationResult {
    QString artifactIdentity,orientationIdentity,diagnostic;
    QVector<FitCalibrationCandidate> candidates;
    QVector<PrintMesh> candidateMeshes;
    FunctionalFeature regenerationPrototype;
    bool ok=false;
};
class ClickHingeCalibrationArtifact {
public:
    static QString artifactIdentity();
    static ClickHingeCalibrationResult generate(const LDrawGeometry::LDrawLoadResult& source);
    static FitCalibrationExperiment observationTemplate(const ClickHingeCalibrationResult& result);
};
}
