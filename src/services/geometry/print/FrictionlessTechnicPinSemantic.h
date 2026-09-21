#pragma once
#include "SemanticOperand.h"
#include "../LDrawLoadResult.h"
namespace PrintGeometry {
class FrictionlessTechnicPinSemantic {
public:
    static QVector<FunctionalFeature> recognize(const LDrawGeometry::LDrawLoadResult&);
    static FunctionalFeature canonicalPrototype();
};
}
