#pragma once

#include "FitCalibrationExperiment.h"

namespace PrintGeometry {
struct CClipBarReceiverCalibrationDefinition {
    QString artifactIdentity, parentArtifactIdentity;
    int candidateCount=7;
    double centerCorrectionMillimetres=0.0, spacingMillimetres=0.05;
};
struct CClipBarReceiverCalibrationResult {
    QString artifactIdentity,parentArtifactIdentity,orientationIdentity,diagnostic;
    QVector<FitCalibrationCandidate> candidates;
    PrintMesh mesh;
    MeshAnalysisResult analysis;
    FunctionalFeature regenerationPrototype;
    bool ok=false;
};
class CClipBarReceiverCalibrationArtifact {
public:
    static QString artifactIdentity();
    static FunctionalFeature canonicalPrototype();
    static CClipBarReceiverCalibrationResult generate(
        const CClipBarReceiverCalibrationDefinition& definition={});
    static FitCalibrationExperiment observationTemplate(
        const CClipBarReceiverCalibrationResult& result,
        const CClipBarReceiverCalibrationDefinition& definition={});
};
}
