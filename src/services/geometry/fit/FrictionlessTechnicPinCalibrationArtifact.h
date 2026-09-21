#pragma once
#include "FitCalibrationExperiment.h"
namespace PrintGeometry {
struct FrictionlessTechnicPinCalibrationResult {QString artifactIdentity,orientationIdentity,diagnostic;QVector<FitCalibrationCandidate>candidates;PrintMesh mesh;MeshAnalysisResult analysis;FunctionalFeature regenerationPrototype;bool ok=false;};
class FrictionlessTechnicPinCalibrationArtifact {
public:
    static QString artifactIdentity();
    static QVector<double> diameterCorrectionsMillimetres();
    static FrictionlessTechnicPinCalibrationResult generate();
    static FitCalibrationExperiment observationTemplate(const FrictionlessTechnicPinCalibrationResult&);
};
}
