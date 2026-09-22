#pragma once

#include "FitCalibrationExperiment.h"

namespace PrintGeometry {

struct TechnicAxleCalibrationArtifactDefinition {
    QString artifactIdentity,parentArtifactIdentity;
    double centerTipToTipCorrectionMillimetres=0.0,candidateSpacingMillimetres=.05;
    int candidateCount=7;
};
struct TechnicAxleCalibrationResult {
    QString artifactIdentity,parentArtifactIdentity,orientationIdentity,diagnostic;
    QVector<FitCalibrationCandidate>candidates;
    PrintMesh mesh;MeshAnalysisResult analysis;FunctionalFeature regenerationPrototype;bool ok=false;
};
class TechnicAxleCalibrationArtifact {
public:
    static QString artifactIdentity();
    static QVector<double> tipToTipCorrectionsMillimetres();
    static TechnicAxleCalibrationResult generate();
    static TechnicAxleCalibrationResult generate(const FunctionalFeature&,const TechnicAxleCalibrationArtifactDefinition&);
    static FitCalibrationExperiment observationTemplate(const TechnicAxleCalibrationResult&,const TechnicAxleCalibrationArtifactDefinition& definition = {});
};

} // namespace PrintGeometry
