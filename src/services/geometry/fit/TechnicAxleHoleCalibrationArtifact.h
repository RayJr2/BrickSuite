#pragma once

#include "FitCalibrationExperiment.h"

namespace PrintGeometry {

struct TechnicAxleHoleCalibrationArtifactDefinition {
    QString artifactIdentity, parentArtifactIdentity;
    double centerTipToTipCorrectionMillimetres = .15;
    double candidateSpacingMillimetres = .05;
    int candidateCount = 7;
};

struct TechnicAxleHoleCalibrationResult {
    QString artifactIdentity, parentArtifactIdentity, orientationIdentity, diagnostic;
    QVector<FitCalibrationCandidate> candidates;
    PrintMesh mesh;
    MeshAnalysisResult analysis;
    FunctionalFeature regenerationPrototype;
    bool ok = false;
};

class TechnicAxleHoleCalibrationArtifact {
public:
    static QString artifactIdentity();
    static QVector<double> tipToTipCorrectionsMillimetres();
    static TechnicAxleHoleCalibrationResult generate();
    static TechnicAxleHoleCalibrationResult generate(const FunctionalFeature&, const TechnicAxleHoleCalibrationArtifactDefinition&);
    static FitCalibrationExperiment observationTemplate(const TechnicAxleHoleCalibrationResult&, const TechnicAxleHoleCalibrationArtifactDefinition& definition = {});
};

struct TechnicAxleHoleArmWidthArtifactDefinition {
    QString artifactIdentity, parentArtifactIdentity;
    double centerArmWidthCorrectionMillimetres = .30;
    double candidateSpacingMillimetres = .10;
    double fixedTipToTipCorrectionMillimetres = .30;
    int candidateCount = 7;
};
class TechnicAxleHoleArmWidthCalibrationArtifact {
public:
    static QString artifactIdentity();
    static TechnicAxleHoleCalibrationResult generate();
    static TechnicAxleHoleCalibrationResult generate(const FunctionalFeature&, const TechnicAxleHoleArmWidthArtifactDefinition&);
    static FitCalibrationExperiment observationTemplate(const TechnicAxleHoleCalibrationResult&, const TechnicAxleHoleArmWidthArtifactDefinition& definition = {});
};

} // namespace PrintGeometry
