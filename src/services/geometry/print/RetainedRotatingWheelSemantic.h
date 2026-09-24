#pragma once

#include "SemanticOperand.h"
#include "../LDrawLoadResult.h"

namespace PrintGeometry {
class RetainedRotatingWheelSemantic {
public:
    // First contract: wpin2a/wpin male and wpinhol2 female only.
    static QVector<FunctionalFeature> recognize(const LDrawGeometry::LDrawLoadResult& source);
};
}
