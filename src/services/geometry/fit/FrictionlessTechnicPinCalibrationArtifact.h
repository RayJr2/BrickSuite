#pragma once
#include "FitCalibrationExperiment.h"
namespace PrintGeometry {
struct FrictionlessTechnicPinCalibrationDefinition {QString artifactIdentity,parentArtifactIdentity;double centerCorrectionMillimetres=0,spacingMillimetres=.1;int candidateCount=7;};
struct FrictionlessTechnicPinCalibrationResult {QString artifactIdentity,parentArtifactIdentity,orientationIdentity,diagnostic;QVector<FitCalibrationCandidate>candidates;PrintMesh mesh;MeshAnalysisResult analysis;FunctionalFeature regenerationPrototype;bool ok=false;};
class FrictionlessTechnicPinCalibrationArtifact {
public:
    static QString artifactIdentity();
    static QVector<double> diameterCorrectionsMillimetres();
    static FrictionlessTechnicPinCalibrationResult generate();
    static FrictionlessTechnicPinCalibrationResult generate(const FunctionalFeature&,const FrictionlessTechnicPinCalibrationDefinition&);
    static FitCalibrationExperiment observationTemplate(const FrictionlessTechnicPinCalibrationResult&);
};
}
