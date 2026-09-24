#pragma once

#include "SemanticOperand.h"
#include "../LDrawLoadResult.h"

namespace PrintGeometry {
class ClickHingeSemantic {
public:
    // First contract only: certified clh1 single-finger arrestors and the
    // paired clh4 dual-finger indexed mate. Other clh variants are distinct.
    static QVector<FunctionalFeature> recognize(const LDrawGeometry::LDrawLoadResult& source);
};
}
