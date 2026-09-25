#pragma once

#include "../LDrawLoadResult.h"
#include "LDrawPrintPreparationProfile.h"

#include <QStringList>
#include <QVector>

namespace PrintGeometry {

struct CertifiedInterfaceStitchDiagnostics {
    int candidateRelationships = 0;
    int acceptedSplits = 0;
    int rejectedAmbiguousCandidates = 0;
    int trianglesBefore = 0;
    int trianglesAfter = 0;
    int boundariesBefore = 0;
    int boundariesAfter = 0;
    QStringList messages;
};

struct CertifiedInterfaceStitchResult {
    LDrawGeometry::LDrawLoadResult loadResult;
    CertifiedInterfaceStitchDiagnostics diagnostics;
    // One authoritative expanded-source triangle for every stitched triangle.
    QVector<int> expandedTriangleForStitchedTriangle;
    bool changed = false;
};

struct SurfaceIntersectionArrangement {
    LDrawGeometry::LDrawLoadResult loadResult;
    // Each fragment has exactly one authoritative stitched parent; a parent may
    // own many fragments. The parent's SurfaceRecord retains reference ancestry.
    QVector<int> stitchedTriangleForFragment;
    int candidatePairs = 0;
    int transversePairs = 0;
    int coplanarOverlapPairs = 0;
    int fragmentsBefore = 0;
    int fragmentsAfter = 0;
    bool bounded = true;
    bool changed = false;
    QString diagnostic;
};

class LDrawCertifiedInterfaceStitcher
{
public:
    static CertifiedInterfaceStitchResult stitch(
        const LDrawGeometry::LDrawLoadResult& source,
        const LDrawPrintPreparationProfile& profile = {});
    static SurfaceIntersectionArrangement arrangeIntersections(
        const LDrawGeometry::LDrawLoadResult& stitched,
        const LDrawPrintPreparationProfile& profile = {});
};

} // namespace PrintGeometry
