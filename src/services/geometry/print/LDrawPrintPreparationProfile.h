#pragma once
#include <cstddef>
#include <QString>
namespace PrintGeometry {
struct LDrawPrintPreparationProfile {
    static constexpr const char* Version="ordinary-brick-v1";
    QString identity=QString::fromLatin1(Version);
    double seamWeldMillimetres=0.0004,planarityToleranceMillimetres=0.00005;
    double boundaryContactToleranceMillimetres=0.001;
    double ordinaryAttachmentIntrusionMillimetres=0.1;
    double maximumExternalBoundsDeviationMillimetres=0.001;
    std::size_t maximumSemanticGroups=4096,maximumBoundaryLoops=8192,maximumLoopEdges=4096,maximumOperands=512;
    // Sequential CSG repeatedly analyzes and copies the growing result. Bound
    // the operation count before entering MCUT, independently of mesh size.
    std::size_t maximumBooleanOperations=24;
    std::size_t maximumVertices=2000000,maximumFaces=4000000;
    std::size_t maximumIntersectionCandidates=20000000;
};
}
