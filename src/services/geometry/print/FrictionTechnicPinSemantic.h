#pragma once
#include "SemanticOperand.h"
#include "../LDrawLoadResult.h"
namespace PrintGeometry {
class FrictionTechnicPinSemantic {
public:
    static FunctionalFeature canonicalPrototype();
    static QVector<FunctionalFeature> recognize(const LDrawGeometry::LDrawLoadResult&);
};
}
