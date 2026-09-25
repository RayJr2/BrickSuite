#pragma once

#include "PrintMesh.h"
#include "SemanticOperand.h"
#include "LDrawCertifiedInterfaceStitcher.h"
#include "SourceCoverage.h"
#include "../LDrawLoadResult.h"

#include <QStringList>
#include <functional>

namespace PrintGeometry {

class LDrawSemanticOperandBuilder
{
public:
    enum class Status {
        Ready,
        UnsupportedLibrarySource,
        UncertifiedGeometry,
        AmbiguousBoundary,
        UnsupportedBoundaryTopology,
        OperandClosureFailed,
        OperandValidationFailed,
        ResourceLimitExceeded,
        Cancelled
    };
    struct Result {
        Status status = Status::UnsupportedLibrarySource;
        PrintMesh source;
        MeshAnalysisResult sourceAnalysis;
        QVector<SemanticOperand> operands;
        QStringList diagnostics;
        int semanticGroups = 0;
        int sourceBoundaryLoops = 0;
        int closureTriangles = 0;
        qsizetype approximateProvenanceBytes = 0;
        qint64 sourceConversionAnalysisMs = 0;
        qint64 semanticGenerationMs = 0;
        CertifiedInterfaceStitchDiagnostics stitchDiagnostics;
        int arrangementTransversePairs = 0;
        int arrangementCoplanarOverlapPairs = 0;
        int arrangementFragments = 0;
        int arrangementGroups = 0;
        int arrangementBoundaryLoops = 0;
        int orientedExactCoincidentGroups = 0;
        int orientedSameFacingDuplicates = 0;
        int orientedOpposingCoincidentGroups = 0;
        int orientedFragments = 0;
        int orientedBoundaryLoops = 0;
        qint64 orientedElapsedMilliseconds = 0;
        bool arrangementAttempted = false;
        QVector<int> arrangementBoundaryLoopEdges;
        QVector<int> orientedBoundaryLoopEdges;
        bool arrangementBounded = true;
        SourceCoverage coverage;
        bool ok() const { return status == Status::Ready; }
    };

    static Result build(const LDrawGeometry::LDrawLoadResult& source,
                        const std::function<bool()>& cancellationRequested = {},
                        bool investigateIntersections = false);
};

} // namespace PrintGeometry
