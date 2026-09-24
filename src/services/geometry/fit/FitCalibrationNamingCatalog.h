#pragma once

#include "FitCalibrationExperiment.h"

#include <array>

namespace PrintGeometry {

enum class FitCalibrationNameKey {
    RoundPassagePerpendicular, RoundPassageParallel, StudOd, StudHeight,
    ClutchTubeWall, ClutchPostWall, ClutchWallPocketBrick, ClutchWallPocketPlate, ClutchAntiStudBore, FrictionlessPin, FrictionPin,
    AxleTip, AxleHoleArmWidth, LegacyAxleHoleTip, BarDiameter, CClipBarReceiverClearance, CClipBarReceiverClearanceParallel, BallJointDiameter, BallJointDiameterParallel, BallSocketContactThroatParallel, PinBarrelHingeDiameterParallel, InterleavedFingerHingeContactParallel, ClickHingeArrestorParallel, RetainedWheelBearingPerpendicular
};

struct FitCalibrationName {
    FitCalibrationNameKey key;
    const char* canonical;
    const char* abbreviated;
    const char* slug;
};

class FitCalibrationNamingCatalog {
public:
    static constexpr std::array<FitCalibrationName, 24> entries() {
        return {{{FitCalibrationNameKey::RoundPassagePerpendicular, "Technic Hole — Round Passage — Perpendicular", "TH-R-PERP", "technic-hole-round-perpendicular"},
                 {FitCalibrationNameKey::RoundPassageParallel, "Technic Hole — Round Passage — Parallel", "TH-R-PAR", "technic-hole-round-parallel"},
                 {FitCalibrationNameKey::StudOd, "Standard Stud — OD — Perpendicular", "STUD-OD-PERP", "standard-stud-od-perpendicular"},
                 {FitCalibrationNameKey::StudHeight, "Standard Stud — Height — Perpendicular", "STUD-HT-PERP", "standard-stud-height-perpendicular"},
                 {FitCalibrationNameKey::ClutchTubeWall, "Stud Receiving Clutch — Tube Wall Cell — Perpendicular", "CLUTCH-TW-PERP", "stud-clutch-tube-wall-perpendicular"},
                 {FitCalibrationNameKey::ClutchPostWall, "Stud Receiving Clutch — Post Wall Cell — Perpendicular", "CLUTCH-PW-PERP", "stud-clutch-post-wall-perpendicular"},
                 {FitCalibrationNameKey::ClutchWallPocketBrick, "Stud Receiving Clutch — Wall Pocket Brick Depth — Perpendicular", "CLUTCH-WPB-PERP", "stud-clutch-wall-pocket-brick-perpendicular"},
                 {FitCalibrationNameKey::ClutchWallPocketPlate, "Stud Receiving Clutch — Wall Pocket Plate Depth — Perpendicular", "CLUTCH-WPP-PERP", "stud-clutch-wall-pocket-plate-perpendicular"},
                 {FitCalibrationNameKey::ClutchAntiStudBore, "Stud Receiving Clutch — Anti-Stud Bore — Perpendicular", "CLUTCH-ASB-PERP", "stud-clutch-antistud-bore-perpendicular"},
                 {FitCalibrationNameKey::FrictionlessPin, "Technic Pin — Frictionless Envelope — Perpendicular", "PIN-FL-PERP", "technic-pin-frictionless-perpendicular"},
                 {FitCalibrationNameKey::FrictionPin, "Technic Pin — Friction Ridge Envelope — Perpendicular", "PIN-FR-PERP", "technic-pin-friction-ridge-perpendicular"},
                 {FitCalibrationNameKey::AxleTip, "Technic Axle — Tip-to-Tip Envelope — Perpendicular", "AXLE-TIP-PERP", "technic-axle-tip-envelope-perpendicular"},
                 {FitCalibrationNameKey::AxleHoleArmWidth, "Technic Axle Hole — Arm Width — Perpendicular", "AXLEHOLE-AW-PERP", "technic-axle-hole-arm-width-perpendicular"},
                 {FitCalibrationNameKey::LegacyAxleHoleTip, "Technic Axle Hole — Tip Clearance — Perpendicular (legacy)", "AXLEHOLE-TIP-LEG", "technic-axle-hole-tip-clearance-perpendicular-legacy"},
                 {FitCalibrationNameKey::BarDiameter, "Standard Bar — Diameter — Perpendicular", "BAR-OD-PERP", "standard-bar-diameter-perpendicular"},
                 {FitCalibrationNameKey::CClipBarReceiverClearance, "C-Clip / Bar Receiver — Clearance — Perpendicular", "CLIP-BAR-PERP", "c-clip-bar-receiver-clearance-perpendicular"},
                 {FitCalibrationNameKey::CClipBarReceiverClearanceParallel, "C-Clip / Bar Receiver — Clearance — Parallel", "CLIP-BAR-PAR", "c-clip-bar-receiver-clearance-parallel"},
                 {FitCalibrationNameKey::BallJointDiameter, "Ball Joint — Diameter — Perpendicular", "BALL-OD-PERP", "ball-joint-diameter-perpendicular"},
                 {FitCalibrationNameKey::BallJointDiameterParallel, "Ball Joint — Diameter — Parallel", "BALL-OD-PAR", "ball-joint-diameter-parallel"},
                 {FitCalibrationNameKey::BallSocketContactThroatParallel, "Ball Socket — Contact and Throat — Parallel", "BALL-SOCKET-PAR", "ball-socket-contact-throat-parallel"},
                 {FitCalibrationNameKey::PinBarrelHingeDiameterParallel, "Pin / Barrel Hinge — Male Pin OD — Parallel", "HINGE-PIN-PAR", "pin-barrel-hinge-male-pin-diameter-parallel"},
                 {FitCalibrationNameKey::InterleavedFingerHingeContactParallel, "Interleaved-Finger Hinge — Contact Bump — Parallel", "HINGE-FINGER-PAR", "interleaved-finger-hinge-contact-bump-parallel"},
                 {FitCalibrationNameKey::ClickHingeArrestorParallel, "Click Hinge — Arrestor Contact — Parallel", "HINGE-CLICK-PAR", "click-hinge-arrestor-contact-parallel"},
                 {FitCalibrationNameKey::RetainedWheelBearingPerpendicular, "Retained Rotating Wheel — Notched Bearing / Retention — Perpendicular", "WHEEL-RET-PERP", "retained-wheel-bearing-retention-perpendicular"}}};
    }
    static FitCalibrationName forKey(FitCalibrationNameKey key) {
        for (const auto& item : entries()) if (item.key == key) return item;
        return entries().front();
    }
    static QString suggestedFileName(FitCalibrationNameKey key, const QString& stage = QStringLiteral("coarse")) {
        return QStringLiteral("%1-%2.3mf").arg(QString::fromLatin1(forKey(key).slug), stage);
    }
    static FitCalibrationNameKey keyFor(const FitCalibrationExperiment& experiment,
                                        FitPrintedOrientation orientation) {
        if (experiment.featureFamily == QStringLiteral("RoundTechnicPassage"))
            return orientation == FitPrintedOrientation::FeatureAxisParallelToBuildPlate
                ? FitCalibrationNameKey::RoundPassageParallel : FitCalibrationNameKey::RoundPassagePerpendicular;
        if (experiment.featureFamily == QStringLiteral("StandardStud"))
            return experiment.correctionDimension == FitCorrectionDimension::Height
                ? FitCalibrationNameKey::StudHeight : FitCalibrationNameKey::StudOd;
        if (experiment.featureFamily == QStringLiteral("StudReceivingClutch")) {
            if ((experiment.hasRegenerationPrototype && experiment.regenerationPrototype.constructionRecipe == QStringLiteral("stud-receiving-antistud-bore-v1")) ||
                experiment.artifactIdentity.contains(QStringLiteral("antistud-bore")))
                return FitCalibrationNameKey::ClutchAntiStudBore;
            if ((experiment.hasRegenerationPrototype && experiment.regenerationPrototype.constructionRecipe == QStringLiteral("stud-receiving-wall-pocket-square-v1")) ||
                experiment.artifactIdentity.contains(QStringLiteral("wall-pocket")))
                return (experiment.hasRegenerationPrototype && experiment.regenerationPrototype.evidenceContract == QStringLiteral("official-ldraw-box5-wall-pocket-brick-v1")) ||
                    experiment.artifactIdentity.contains(QStringLiteral("wall-pocket-brick"))
                    ? FitCalibrationNameKey::ClutchWallPocketBrick : FitCalibrationNameKey::ClutchWallPocketPlate;
            return (experiment.hasRegenerationPrototype && experiment.regenerationPrototype.constructionRecipe == QStringLiteral("stud-receiving-post-wall-cell-v1")) ||
                    experiment.artifactIdentity.contains(QStringLiteral("post-wall-cell"))
                ? FitCalibrationNameKey::ClutchPostWall : FitCalibrationNameKey::ClutchTubeWall;
        }
        if (experiment.featureFamily == QStringLiteral("FrictionlessTechnicPin")) return FitCalibrationNameKey::FrictionlessPin;
        if (experiment.featureFamily == QStringLiteral("StandardBar")) return FitCalibrationNameKey::BarDiameter;
        if (experiment.featureFamily == QStringLiteral("CClipBarReceiver"))
            return orientation == FitPrintedOrientation::FeatureAxisParallelToBuildPlate
                ? FitCalibrationNameKey::CClipBarReceiverClearanceParallel : FitCalibrationNameKey::CClipBarReceiverClearance;
        if (experiment.featureFamily == QStringLiteral("BallJoint"))
            return orientation == FitPrintedOrientation::FeatureAxisParallelToBuildPlate
                ? FitCalibrationNameKey::BallJointDiameterParallel : FitCalibrationNameKey::BallJointDiameter;
        if (experiment.featureFamily == QStringLiteral("BallSocket"))
            return FitCalibrationNameKey::BallSocketContactThroatParallel;
        if (experiment.featureFamily == QStringLiteral("PinBarrelHinge"))
            return FitCalibrationNameKey::PinBarrelHingeDiameterParallel;
        if (experiment.featureFamily == QStringLiteral("InterleavedFingerHinge"))
            return FitCalibrationNameKey::InterleavedFingerHingeContactParallel;
        if (experiment.featureFamily == QStringLiteral("ClickHinge"))
            return FitCalibrationNameKey::ClickHingeArrestorParallel;
        if (experiment.featureFamily == QStringLiteral("RetainedRotatingWheel"))
            return FitCalibrationNameKey::RetainedWheelBearingPerpendicular;
        if (experiment.featureFamily == QStringLiteral("FrictionTechnicPin")) return FitCalibrationNameKey::FrictionPin;
        if (experiment.featureFamily == QStringLiteral("TechnicAxle")) return FitCalibrationNameKey::AxleTip;
        if (experiment.featureFamily == QStringLiteral("TechnicAxleHole"))
            return (experiment.hasRegenerationPrototype && experiment.regenerationPrototype.constructionRecipe == QStringLiteral("technic-axle-hole-arm-width-clearance-v2")) ||
                    experiment.artifactIdentity.contains(QStringLiteral("arm-width"))
                ? FitCalibrationNameKey::AxleHoleArmWidth : FitCalibrationNameKey::LegacyAxleHoleTip;
        return FitCalibrationNameKey::RoundPassagePerpendicular;
    }
    static QString canonical(const FitCalibrationExperiment& experiment, FitPrintedOrientation orientation) {
        if (experiment.featureFamily != QStringLiteral("RoundTechnicPassage") &&
            experiment.featureFamily != QStringLiteral("StandardStud") &&
            experiment.featureFamily != QStringLiteral("StudReceivingClutch") &&
            experiment.featureFamily != QStringLiteral("FrictionlessTechnicPin") &&
            experiment.featureFamily != QStringLiteral("FrictionTechnicPin") &&
            experiment.featureFamily != QStringLiteral("TechnicAxle") &&
            experiment.featureFamily != QStringLiteral("TechnicAxleHole") &&
            experiment.featureFamily != QStringLiteral("StandardBar") &&
            experiment.featureFamily != QStringLiteral("CClipBarReceiver") &&
            experiment.featureFamily != QStringLiteral("BallJoint") &&
            experiment.featureFamily != QStringLiteral("BallSocket") &&
            experiment.featureFamily != QStringLiteral("PinBarrelHinge") &&
            experiment.featureFamily != QStringLiteral("InterleavedFingerHinge") &&
            experiment.featureFamily != QStringLiteral("ClickHinge") &&
            experiment.featureFamily != QStringLiteral("RetainedRotatingWheel"))
            return QStringLiteral("Unknown Calibration Feature — Orientation Unknown");
        const auto name = forKey(keyFor(experiment, orientation));
        QString result = QString::fromUtf8(name.canonical);
        if (orientation == FitPrintedOrientation::FeatureAxisParallelToBuildPlate)
            result.replace(QStringLiteral(" — Perpendicular"), QStringLiteral(" — Parallel"));
        else if (orientation == FitPrintedOrientation::Unknown || orientation == FitPrintedOrientation::OtherUnsupported)
            result.replace(QStringLiteral(" — Perpendicular"), QStringLiteral(" — Orientation Unknown"));
        return result;
    }
};

} // namespace PrintGeometry
