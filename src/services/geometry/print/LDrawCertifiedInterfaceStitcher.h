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

struct OrientedSurfaceArrangement {
    LDrawGeometry::LDrawLoadResult loadResult;
    // A retained fragment can represent several exactly coincident authored
    // fragments. Each entry remains traceable to its stitched source triangle.
    QVector<QVector<int>> stitchedTrianglesForFragment;
    QVector<QVector<int>> arrangedFragmentsForFragment;
    int exactCoincidentGroups = 0;
    int sameFacingDuplicates = 0;
    int opposingCoincidentGroups = 0;
    int fragmentsBefore = 0;
    int fragmentsAfter = 0;
    qint64 elapsedMilliseconds = 0;
    bool bounded = true;
    QString diagnostic;
};

struct MaterialCellArrangement {
    enum class FragmentStatus {Unresolved,ProvenBoundary,ProvenInternalSharedBoundary,ExplicitDuplicate};
    int inputFragments = 0;
    int exactAdjacencies = 0;
    int ambiguousEdges = 0;
    int candidateCells = 0;
    int provenCells = 0;
    int provenBoundaryFragments = 0;
    int explicitDuplicateFragments = 0;
    int unresolvedFragments = 0;
    int residualBoundaryEdges = 0;
    qint64 elapsedMilliseconds = 0;
    bool bounded = true;
    // Per retained fragment: a proven cell boundary, or unresolved. No source
    // fragment is excluded on the basis of winding or spatial proximity.
    QVector<bool> provenBoundary;
    QVector<FragmentStatus> arrangedFragmentStatus;
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
    static OrientedSurfaceArrangement classifyOrientedFragments(
        const SurfaceIntersectionArrangement& arranged);
    static MaterialCellArrangement proveMaterialCells(
        const OrientedSurfaceArrangement& oriented);
};

} // namespace PrintGeometry
