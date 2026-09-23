#pragma once

#include "FitCalibrationExperiment.h"

namespace PrintGeometry {
struct StandardBarCalibrationDefinition {
    QString artifactIdentity, parentArtifactIdentity;
    int candidateCount=7;
    double centerCorrectionMillimetres=0.0, spacingMillimetres=0.05;
};
struct StandardBarCalibrationResult {
    QString artifactIdentity,parentArtifactIdentity,orientationIdentity,diagnostic;
    QVector<FitCalibrationCandidate> candidates;
    PrintMesh mesh;
    MeshAnalysisResult analysis;
    FunctionalFeature regenerationPrototype;
    bool ok=false;
};
class StandardBarCalibrationArtifact {
public:
    static QString artifactIdentity();
    static FunctionalFeature canonicalPrototype();
    static StandardBarCalibrationResult generate(const StandardBarCalibrationDefinition& definition={});
    static FitCalibrationExperiment observationTemplate(const StandardBarCalibrationResult& result,
                                                        const StandardBarCalibrationDefinition& definition={});
};
}
