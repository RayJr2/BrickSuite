#pragma once

#include "LDrawPrintGeometryBuilder.h"
#include "ManufacturingMesh.h"
#include "MeshBooleanService.h"
#include "../PrintOrientation.h"
#include "../fit/FitCalibrationLibrary.h"
#include <functional>
#include <memory>

namespace PrintGeometry {
enum class ManufacturingMeshError { None, InvalidInput, UnsupportedProofPart, IncompatibleProfile, MissingCorrection, SemanticFailure, RegenerationFailure, BooleanFailure, InvalidResult };
struct ManufacturingMeshResult { ManufacturingMeshError error=ManufacturingMeshError::InvalidInput;std::shared_ptr<ManufacturingMesh> manufacturingMesh;QString diagnostic;bool ok()const{return error==ManufacturingMeshError::None;} };
struct ManufacturingMeshCorrections { const FitProfileCorrection* femaleDiameter=nullptr;const FitProfileCorrection* studDiameter=nullptr;const FitProfileCorrection* studHeight=nullptr;const FitProfileCorrection* receivingTubeDiameter=nullptr;const FitProfileCorrection* receivingPostDiameter=nullptr;const FitProfileCorrection* receivingWallPocketWidth=nullptr;const FitProfileCorrection* receivingAntiStudBoreDiameter=nullptr;const FitProfileCorrection* frictionlessPinDiameter=nullptr;const FitProfileCorrection* frictionPinDiameter=nullptr;const FitProfileCorrection* technicAxleTipToTip=nullptr;const FitProfileCorrection* technicAxleHoleArmWidth=nullptr;QString studHeightDiagnostic;bool any()const{return femaleDiameter||studDiameter||studHeight||receivingTubeDiameter||receivingPostDiameter||receivingWallPocketWidth||receivingAntiStudBoreDiameter||frictionlessPinDiameter||frictionPinDiameter||technicAxleTipToTip||technicAxleHoleArmWidth;} };
class ManufacturingMeshService {
public:
    using BooleanServiceFactory=std::function<std::unique_ptr<MeshBooleanService>()>;
    using SemanticBuilderFunction=std::function<LDrawSemanticOperandBuilder::Result(const LDrawGeometry::LDrawLoadResult&)>;
    explicit ManufacturingMeshService(BooleanServiceFactory factory={},SemanticBuilderFunction builder={});
    static const FitProfileCorrection* compatibleCorrection(const FitProfile&,FitPrintedOrientation,QString* reason=nullptr);
    static ManufacturingMeshCorrections compatibleCorrections(const FitProfile&,FitPrintedOrientation,QString* reason=nullptr);
    static FitPrintedOrientation transformedOrientation(const FunctionalFeature&,const PrintOrientation&);
    static bool hasApplicableCorrection(const FitProfile&,const LDrawSemanticOperandBuilder::Result&,const PrintOrientation&,QString* reason=nullptr);
    static bool hasApplicableCorrection(const FitProfile&,const LDrawGeometry::LDrawLoadResult&,const PrintOrientation&,QString* reason=nullptr);
    ManufacturingMeshResult generate(const LDrawGeometry::LDrawLoadResult&,const PreparedMesh&,const FitProfile&,const PrintOrientation&)const;
    ManufacturingMeshResult generate(const LDrawGeometry::LDrawLoadResult&,const PreparedMesh&,const FitProfile&,FitPrintedOrientation)const;
private:BooleanServiceFactory m_factory;SemanticBuilderFunction m_builder;
};
}
