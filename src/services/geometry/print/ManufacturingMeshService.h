/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 *
 * This file is part of BrickSuite.
 *
 * BrickSuite is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, version 3 of the License.
 *
 * BrickSuite is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with BrickSuite. If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include "LDrawPrintGeometryBuilder.h"
#include "ManufacturingMesh.h"
#include "ManufacturingFitSummary.h"
#include "MeshBooleanService.h"
#include "../PrintOrientation.h"
#include "../fit/FitCalibrationLibrary.h"
#include <functional>
#include <memory>
#include <QJsonObject>

namespace PrintGeometry {
enum class ManufacturingMeshError { None, InvalidInput, UnsupportedProofPart, IncompatibleProfile, MissingCorrection, SemanticFailure, RegenerationFailure, BooleanFailure, InvalidResult };
struct ManufacturingMeshResult { ManufacturingMeshError error=ManufacturingMeshError::InvalidInput;std::shared_ptr<ManufacturingMesh> manufacturingMesh;QString diagnostic;ManufacturingFitSummary fitSummary;QJsonObject experimentalProvenance;bool ok()const{return error==ManufacturingMeshError::None;} };
struct ManufacturingMeshCorrections { const FitProfileCorrection* femaleDiameter=nullptr;const FitProfileCorrection* studDiameter=nullptr;const FitProfileCorrection* studHeight=nullptr;const FitProfileCorrection* receivingTubeDiameter=nullptr;const FitProfileCorrection* receivingPostDiameter=nullptr;const FitProfileCorrection* receivingWallPocketWidth=nullptr;const FitProfileCorrection* receivingAntiStudBoreDiameter=nullptr;const FitProfileCorrection* frictionlessPinDiameter=nullptr;const FitProfileCorrection* frictionPinDiameter=nullptr;const FitProfileCorrection* technicAxleTipToTip=nullptr;const FitProfileCorrection* technicAxleHoleArmWidth=nullptr;const FitProfileCorrection* standardBarDiameter=nullptr;const FitProfileCorrection* cClipClearance=nullptr;const FitProfileCorrection* ballJointDiameter=nullptr;const FitProfileCorrection* ballSocketClearance=nullptr;const FitProfileCorrection* pinBarrelHingeDiameter=nullptr;const FitProfileCorrection* interleavedFingerBump=nullptr;const FitProfileCorrection* clickHingeArrestor=nullptr;const FitProfileCorrection* retainedWheelBearing=nullptr;const FitProfileCorrection* plainWheelBearing=nullptr;QString studHeightDiagnostic;bool any()const{return femaleDiameter||studDiameter||studHeight||receivingTubeDiameter||receivingPostDiameter||receivingWallPocketWidth||receivingAntiStudBoreDiameter||frictionlessPinDiameter||frictionPinDiameter||technicAxleTipToTip||technicAxleHoleArmWidth||standardBarDiameter||cClipClearance||ballJointDiameter||ballSocketClearance||pinBarrelHingeDiameter||interleavedFingerBump||clickHingeArrestor||retainedWheelBearing||plainWheelBearing;} };
class ManufacturingMeshService {
public:
    using BooleanServiceFactory=std::function<std::unique_ptr<MeshBooleanService>()>;
    using SemanticBuilderFunction=std::function<LDrawSemanticOperandBuilder::Result(const LDrawGeometry::LDrawLoadResult&)>;
    explicit ManufacturingMeshService(BooleanServiceFactory factory={},SemanticBuilderFunction builder={});
    static const FitProfileCorrection* compatibleCorrection(const FitProfile&,FitPrintedOrientation,QString* reason=nullptr);
    static ManufacturingMeshCorrections compatibleCorrections(const FitProfile&,FitPrintedOrientation,QString* reason=nullptr);
    static FitPrintedOrientation transformedOrientation(const FunctionalFeature&,const PrintOrientation&);
    // Source-only recognizers: no operand closure, repair, Boolean or PreparedMesh.
    static QVector<FunctionalFeature> inspectSourceFitFeatures(const LDrawGeometry::LDrawLoadResult&);
    static QVector<const FitProfileCorrection*> featureCorrections(const FitProfile&,
        const FunctionalFeature&,const PrintOrientation&);
    // Recognition only: this does not grant profile compatibility or fit ownership.
    static bool hasRecognizedFitFeatures(const LDrawSemanticOperandBuilder::Result&);
    static bool hasRecognizedFitFeatures(const LDrawGeometry::LDrawLoadResult&);
    static bool hasApplicableCorrection(const FitProfile&,const LDrawSemanticOperandBuilder::Result&,const PrintOrientation&,QString* reason=nullptr);
    static bool hasApplicableCorrection(const FitProfile&,const LDrawGeometry::LDrawLoadResult&,const PrintOrientation&,QString* reason=nullptr);
    ManufacturingMeshResult generate(const LDrawGeometry::LDrawLoadResult&,const PreparedMesh&,const FitProfile&,const PrintOrientation&)const;
    ManufacturingMeshResult generate(const LDrawGeometry::LDrawLoadResult&,const PreparedMesh&,const FitProfile&,FitPrintedOrientation)const;
    ManufacturingMeshResult attemptExperimentalOverride(const LDrawGeometry::LDrawLoadResult&,const PreparedMesh&,const FitProfile&,const PrintOrientation&)const;
private:BooleanServiceFactory m_factory;SemanticBuilderFunction m_builder;
};
}
