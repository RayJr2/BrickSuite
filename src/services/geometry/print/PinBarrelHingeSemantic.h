#pragma once

#include "SemanticOperand.h"
#include "../LDrawLoadResult.h"

namespace PrintGeometry {
class PinBarrelHingeSemantic {
public:
    // The first contract is the non-clicking 3937/3938 snap barrel, not a
    // Technic pin, a finger hinge, or a generic cylindrical opening.
    static QVector<FunctionalFeature> recognize(const LDrawGeometry::LDrawLoadResult& source);
    static bool adjustPrepared(const LDrawGeometry::LDrawLoadResult& source,
        const PrintMesh& nominal, const FunctionalFeature& pin, double correction,
        PrintMesh* adjusted, QString* diagnostic=nullptr);
};
}
