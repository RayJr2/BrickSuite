#pragma once

#include "../LDrawLoadResult.h"
#include "SemanticOperand.h"

namespace PrintGeometry {

class RoundTechnicPassageSemantic
{
public:
    static QVector<FunctionalFeature> recognize(const LDrawGeometry::LDrawLoadResult& source);
};

} // namespace PrintGeometry
