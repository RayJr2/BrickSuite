#pragma once
#include "FitCalibrationExperiment.h"
namespace PrintGeometry {
struct FrictionTechnicPinCalibrationResult {QString artifactIdentity,parentArtifactIdentity,orientationIdentity,diagnostic;QVector<FitCalibrationCandidate>candidates;PrintMesh mesh;MeshAnalysisResult analysis;FunctionalFeature regenerationPrototype;bool ok=false;};
struct FrictionTechnicPinCalibrationArtifactDefinition {QString artifactIdentity,parentArtifactIdentity;double centerRidgeEnvelopeCorrectionMillimetres=0.0,candidateSpacingMillimetres=.05;int candidateCount=7;};
class FrictionTechnicPinCalibrationArtifact {
public:
    static QString artifactIdentity();
    static QVector<double> ridgeEnvelopeCorrectionsMillimetres();
    static FrictionTechnicPinCalibrationResult generate();
    static FrictionTechnicPinCalibrationResult generate(const FunctionalFeature&,const FrictionTechnicPinCalibrationArtifactDefinition&);
    static FitCalibrationExperiment observationTemplate(const FrictionTechnicPinCalibrationResult&);
};
}
