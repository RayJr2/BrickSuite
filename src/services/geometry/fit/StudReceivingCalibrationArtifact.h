#pragma once

#include "FitCalibrationExperiment.h"
#include "../print/FunctionalOperandRegenerator.h"

namespace PrintGeometry {
struct StudReceivingCalibrationArtifactDefinition {
    QString artifactIdentity,parentArtifactIdentity;
    double centerDiameterCorrectionMillimetres=0.0,candidateSpacingMillimetres=0.1;
    int candidateCount=7;
};
struct StudReceivingCalibrationArtifactResult {
    QString artifactIdentity,parentArtifactIdentity,orientationIdentity,diagnostic;
    QVector<FitCalibrationCandidate> candidates;
    PrintMesh mesh; MeshAnalysisResult analysis;
    FunctionalFeature regenerationPrototype;
    bool ok=false;
};
class StudReceivingCalibrationArtifact {
public:
    static FunctionalFeature canonicalPrototype();
    static FunctionalFeature canonicalPostWallPrototype();
    static FunctionalFeature canonicalWallPocketPrototype(bool brickDepth);
    static QString artifactIdentity();
    static QString postWallArtifactIdentity();
    static QString wallPocketArtifactIdentity(bool brickDepth);
    static QString orientationIdentity();
    static QVector<double> diameterCorrectionsMillimetres();
    static StudReceivingCalibrationArtifactResult generate();
    static StudReceivingCalibrationArtifactResult generateMarkedTubeWallCell();
    static StudReceivingCalibrationArtifactResult generatePostWallCell();
    static StudReceivingCalibrationArtifactResult generateWallPocket(bool brickDepth);
    static StudReceivingCalibrationArtifactResult generateWallPocket(const FunctionalFeature&,
                                                                    const StudReceivingCalibrationArtifactDefinition&);
    static StudReceivingCalibrationArtifactResult generate(const FunctionalFeature&,const StudReceivingCalibrationArtifactDefinition&);
    static FitCalibrationExperiment observationTemplate(const StudReceivingCalibrationArtifactResult&);
    static FitCalibrationExperiment observationTemplate(const StudReceivingCalibrationArtifactResult&,const StudReceivingCalibrationArtifactDefinition&);
};
}
