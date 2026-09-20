#pragma once

#include "SemanticOperand.h"

namespace PrintGeometry {

// Explicit diagnostic/manufacturing parameter: positive values increase a
// female clearance diameter; the governed radius changes by exactly half.
struct FemaleClearanceDiameterCorrection {
    double millimetres = 0.0;
};

enum class FunctionalOperandRegenerationError {
    None,
    UnsupportedFeature,
    InvalidContract,
    UnsafeCorrection,
    InvalidGeneratedOperand
};

struct FunctionalOperandRegenerationResult {
    FunctionalOperandRegenerationError error = FunctionalOperandRegenerationError::InvalidContract;
    QString featureIdentity;
    double requestedDiameterCorrectionMillimetres = 0.0;
    double resultingGoverningRadiusMillimetres = 0.0;
    QVector<FunctionalRadialSection> executedProfile;
    PrintMesh mesh;
    MeshAnalysisResult analysis;
    QString diagnostic;
    bool ok() const { return error == FunctionalOperandRegenerationError::None; }
};

class FunctionalOperandRegenerator {
public:
    static FunctionalOperandRegenerationResult regenerate(
        const FunctionalFeature& feature,
        FemaleClearanceDiameterCorrection correction);
};

} // namespace PrintGeometry
