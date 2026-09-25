#pragma once

#include "SemanticOperand.h"
#include "../LDrawLoadResult.h"

namespace PrintGeometry {
class PlainRoundBoreWheelSemantic {
public:
    // The first contract is the certified 30027a blind wheel bearing, not an
    // arbitrary 3.20 mm cylindrical hole or the notched wpinhol family.
    static QVector<FunctionalFeature> recognize(const LDrawGeometry::LDrawLoadResult& source);
    static bool adjustPrepared(const LDrawGeometry::LDrawLoadResult& source,
        const PrintMesh& nominal,const FunctionalFeature& bearing,double correction,
        PrintMesh* adjusted,QString* diagnostic=nullptr);
};
}
