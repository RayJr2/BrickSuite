#pragma once

#include "SemanticOperand.h"
#include "../LDrawLoadResult.h"

namespace PrintGeometry {
class StandardBarSemantic {
public:
    static QVector<FunctionalFeature> recognize(const LDrawGeometry::LDrawLoadResult& source);
};
}
