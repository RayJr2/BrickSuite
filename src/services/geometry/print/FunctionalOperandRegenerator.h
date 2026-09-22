#pragma once

#include "SemanticOperand.h"

namespace PrintGeometry {

// Explicit diagnostic/manufacturing parameter: positive values increase a
// female clearance diameter; the governed radius changes by exactly half.
struct FemaleClearanceDiameterCorrection {
    double millimetres = 0.0;
};

// Independent dimensional controls for an ordinary solid male stud. Diameter
// governs radial clutch; height governs axial seating and is never inferred
// from the diameter correction.
struct MaleStudDimensionalCorrection {
    double diameterMillimetres = 0.0;
    double heightMillimetres = 0.0;
};
struct ReceivingTubeOutsideDiameterCorrection { double millimetres = 0.0; };
struct FrictionlessPinEnvelopeDiameterCorrection { double millimetres = 0.0; };
struct FrictionPinRidgeEnvelopeDiameterCorrection { double millimetres = 0.0; };
struct TechnicAxleTipToTipCorrection { double millimetres = 0.0; };

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
    double requestedHeightCorrectionMillimetres = 0.0;
    double resultingGoverningRadiusMillimetres = 0.0;
    double resultingAxialExtentMillimetres = 0.0;
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
    static FunctionalOperandRegenerationResult regenerateStud(
        const FunctionalFeature& feature,
        MaleStudDimensionalCorrection correction);
    static FunctionalOperandRegenerationResult regenerateReceivingTube(
        const FunctionalFeature& feature,
        ReceivingTubeOutsideDiameterCorrection correction);
    static FunctionalOperandRegenerationResult regenerateFrictionlessPin(
        const FunctionalFeature& feature,
        FrictionlessPinEnvelopeDiameterCorrection correction);
    static FunctionalOperandRegenerationResult regenerateFrictionPin(
        const FunctionalFeature& feature,
        FrictionPinRidgeEnvelopeDiameterCorrection correction);
    static FunctionalOperandRegenerationResult regenerateTechnicAxleProfile(
        const FunctionalFeature& feature,
        TechnicAxleTipToTipCorrection correction);
};

} // namespace PrintGeometry
