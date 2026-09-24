#pragma once

#include "SemanticOperand.h"
#include "../LDrawLoadResult.h"

namespace PrintGeometry {
class BallSocketSemantic {
public:
    static QVector<FunctionalFeature> recognize(const LDrawGeometry::LDrawLoadResult& source);
    static bool adjustPrepared(const LDrawGeometry::LDrawLoadResult& source,
        const PrintMesh& nominal,const FunctionalFeature& socket,double correction,
        PrintMesh* adjusted,QString* diagnostic=nullptr);
};
}
