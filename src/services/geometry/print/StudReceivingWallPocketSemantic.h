#pragma once

#include "../LDrawLoadResult.h"
#include "SemanticOperand.h"

namespace PrintGeometry {

// Only the certified, square, single-cell pocket/shell construction is eligible.
// Other stud-sized cavities are deliberately left unclassified.
class StudReceivingWallPocketSemantic {
public:
    static QVector<FunctionalFeature> recognize(const LDrawGeometry::LDrawLoadResult& source);
    static bool adjustPrepared(const PrintMesh& nominal, const FunctionalFeature& pocket,
                               double openingWidthCorrectionMillimetres, PrintMesh* adjusted,
                               QString* diagnostic = nullptr);
};

} // namespace PrintGeometry
