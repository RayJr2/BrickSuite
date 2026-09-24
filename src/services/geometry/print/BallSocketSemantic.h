#pragma once

#include "SemanticOperand.h"
#include "../LDrawLoadResult.h"

namespace PrintGeometry {
class BallSocketSemantic {
public:
    static QVector<FunctionalFeature> recognize(const LDrawGeometry::LDrawLoadResult& source);
};
}
