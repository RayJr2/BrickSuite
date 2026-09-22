#pragma once

#include "../LDrawLoadResult.h"
#include "SemanticOperand.h"

namespace PrintGeometry {

class TechnicAxleSemantic
{
public:
    static FunctionalFeature canonicalAxlePrototype();
    static FunctionalFeature canonicalAxleHolePrototype();
    static QVector<FunctionalFeature> recognizeAxles(const LDrawGeometry::LDrawLoadResult& source);
    static QVector<FunctionalFeature> recognizeAxleHoles(const LDrawGeometry::LDrawLoadResult& source);
};

} // namespace PrintGeometry
