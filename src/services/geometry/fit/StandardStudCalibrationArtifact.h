#pragma once

#include "FitCalibrationExperiment.h"
#include "../print/FunctionalOperandRegenerator.h"

namespace PrintGeometry {

enum class StandardStudCalibrationDimension { Diameter, Height };

struct StandardStudCalibrationArtifactDefinition {
    QString artifactIdentity;
    QString parentArtifactIdentity;
    StandardStudCalibrationDimension dimension = StandardStudCalibrationDimension::Diameter;
    double centerCorrectionMillimetres = 0.0;
    double candidateSpacingMillimetres = 0.1;
    double fixedDiameterCorrectionMillimetres = 0.0;
    int candidateCount = 7;
};

struct StandardStudCalibrationArtifactResult {
    QString artifactIdentity;
    QString orientationIdentity;
    StandardStudCalibrationDimension dimension = StandardStudCalibrationDimension::Diameter;
    QString diagnostic;
    QVector<FitCalibrationCandidate> candidates;
    PrintMesh mesh;
    MeshAnalysisResult analysis;
    FunctionalFeature regenerationPrototype;
    bool ok = false;
};

class StandardStudCalibrationArtifact {
public:
    static FunctionalFeature canonicalPrototype();
    static QString diameterArtifactIdentity();
    static QString heightArtifactIdentity();
    static QString orientationIdentity();
    static StandardStudCalibrationArtifactResult generate(
        const FunctionalFeature&, const StandardStudCalibrationArtifactDefinition&);
    static FitCalibrationExperiment observationTemplate(
        const StandardStudCalibrationArtifactResult&,
        const StandardStudCalibrationArtifactDefinition&);
};

} // namespace PrintGeometry
