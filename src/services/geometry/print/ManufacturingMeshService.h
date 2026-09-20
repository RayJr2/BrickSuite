#pragma once

#include "LDrawPrintGeometryBuilder.h"
#include "ManufacturingMesh.h"
#include "MeshBooleanService.h"
#include "../fit/FitCalibrationLibrary.h"
#include <functional>
#include <memory>

namespace PrintGeometry {
enum class ManufacturingMeshError { None, InvalidInput, UnsupportedProofPart, IncompatibleProfile, MissingCorrection, SemanticFailure, RegenerationFailure, BooleanFailure, InvalidResult };
struct ManufacturingMeshResult { ManufacturingMeshError error=ManufacturingMeshError::InvalidInput;std::shared_ptr<ManufacturingMesh> manufacturingMesh;QString diagnostic;bool ok()const{return error==ManufacturingMeshError::None;} };
class ManufacturingMeshService {
public:
    using BooleanServiceFactory=std::function<std::unique_ptr<MeshBooleanService>()>;
    using SemanticBuilderFunction=std::function<LDrawSemanticOperandBuilder::Result(const LDrawGeometry::LDrawLoadResult&)>;
    explicit ManufacturingMeshService(BooleanServiceFactory factory={},SemanticBuilderFunction builder={});
    static const FitProfileCorrection* compatibleCorrection(const FitProfile&,FitPrintedOrientation,QString* reason=nullptr);
    ManufacturingMeshResult generate(const LDrawGeometry::LDrawLoadResult&,const PreparedMesh&,const FitProfile&,FitPrintedOrientation)const;
private:BooleanServiceFactory m_factory;SemanticBuilderFunction m_builder;
};
}
