#pragma once

#include "../LDrawLoadResult.h"
#include "SemanticOperand.h"

namespace PrintGeometry {

// The official stud4o primitive denotes a centered underside antistud without
// the outer receiving tube. Generic cylindrical holes are not eligible.
class StudReceivingAntiStudSemantic {
public:
    static QVector<FunctionalFeature> recognize(const LDrawGeometry::LDrawLoadResult& source);
    static bool adjustPrepared(const LDrawGeometry::LDrawLoadResult& source, const PrintMesh& nominal,
                               const FunctionalFeature& bore, double diameterCorrectionMillimetres,
                               PrintMesh* adjusted, QString* diagnostic = nullptr);
};

} // namespace PrintGeometry
