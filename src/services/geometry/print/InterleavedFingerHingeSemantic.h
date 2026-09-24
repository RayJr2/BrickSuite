#pragma once

#include "SemanticOperand.h"
#include "../LDrawLoadResult.h"

namespace PrintGeometry {
class InterleavedFingerHingeSemantic {
public:
    // Complementary h2 three-finger and h1 two-finger rotating halves. This
    // contract does not include snap barrels, click hinges, or generic rods.
    static QVector<FunctionalFeature> recognize(const LDrawGeometry::LDrawLoadResult& source);
};
}
