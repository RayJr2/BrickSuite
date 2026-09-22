#pragma once

#include "../LDrawLoadResult.h"
#include "SemanticOperand.h"

namespace PrintGeometry {

class StudReceivingPostSemantic
{
public:
    static QVector<FunctionalFeature> recognize(const LDrawGeometry::LDrawLoadResult& source);
};

} // namespace PrintGeometry
