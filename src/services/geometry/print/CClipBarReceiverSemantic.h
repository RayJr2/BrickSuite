#pragma once

#include "SemanticOperand.h"
#include "../LDrawLoadResult.h"

namespace PrintGeometry {
class CClipBarReceiverSemantic {
public:
    static QVector<FunctionalFeature> recognize(const LDrawGeometry::LDrawLoadResult& source);
    static bool adjustPrepared(const LDrawGeometry::LDrawLoadResult& source,
                               const PrintMesh& nominal, const FunctionalFeature& clip,
                               double correction, PrintMesh* adjusted, QString* diagnostic=nullptr);
};
}
