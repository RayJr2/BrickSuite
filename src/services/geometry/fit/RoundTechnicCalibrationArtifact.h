#pragma once

#include "FitCalibrationExperiment.h"
#include "../print/FunctionalOperandRegenerator.h"

namespace PrintGeometry {

struct RoundTechnicCalibrationArtifactResult {
    QString artifactIdentity;
    QString orientationIdentity;
    QString diagnostic;
    QVector<FitCalibrationCandidate> candidates;
    PrintMesh mesh;
    MeshAnalysisResult analysis;
    FunctionalFeature regenerationPrototype;
    bool ok = false;
};

struct RoundTechnicCalibrationArtifactDefinition {
    QString artifactIdentity;
    QString parentArtifactIdentity;
    double centerDiameterCorrectionMillimetres = 0.0;
    double candidateSpacingMillimetres = 0.1;
    int candidateCount = 7;
    FitPrintedOrientation printedOrientation = FitPrintedOrientation::Unknown;
};

class RoundTechnicCalibrationArtifact {
public:
    static QString artifactIdentity();
    static QString orientationIdentity();
    static QString parallelArtifactIdentity();
    static RoundTechnicCalibrationArtifactDefinition parallelCoarseDefinition();
    static FunctionalFeature canonicalPrototype();
    static QVector<double> diameterCorrectionsMillimetres();
    static RoundTechnicCalibrationArtifactResult generate(const FunctionalFeature& prototype);
    static RoundTechnicCalibrationArtifactResult generate(const FunctionalFeature& prototype,
                                                           const RoundTechnicCalibrationArtifactDefinition& definition);
    static FitCalibrationExperiment observationTemplate(const RoundTechnicCalibrationArtifactResult& artifact);
    static FitCalibrationExperiment observationTemplate(const RoundTechnicCalibrationArtifactResult& artifact,
                                                        const RoundTechnicCalibrationArtifactDefinition& definition);
};

} // namespace PrintGeometry
