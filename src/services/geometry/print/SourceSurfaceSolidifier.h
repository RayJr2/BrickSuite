#pragma once

#include "../LDrawLoadResult.h"
#include "PrintMesh.h"
#include "PrintMeshAnalysis.h"

#include <QString>
#include <cstdint>

namespace PrintGeometry {

struct SourceSurfaceAttemptMetrics {
    int rayAxis = 0;
    int gridX = 0, gridY = 0, gridZ = 0;
    std::size_t finalVertices = 0, finalFaces = 0;
    std::size_t mixedCells = 0, multiVertexCells = 0, maximumCellVertices = 0;
    std::uint64_t approximatePrimaryBytes = 0;
    std::uint64_t rayTriangleTests = 0, exhaustiveRayTriangleTests = 0;
    std::uint64_t nearestTriangleTests = 0, exhaustiveNearestTriangleTests = 0;
    qint64 sourceAnalysisMilliseconds = 0;
    qint64 indexMilliseconds = 0;
    qint64 rayClassificationMilliseconds = 0;
    qint64 extractionMilliseconds = 0;
    qint64 nearestProjectionMilliseconds = 0;
    qint64 orientationMilliseconds = 0;
    qint64 strictAnalysisMilliseconds = 0;
    MeshAnalysisMetrics strictBreakdown;
    qint64 totalMilliseconds = 0;
    QString outcome;
};

struct SourceSurfaceSolidificationResult {
    PrintMesh mesh;
    MeshAnalysisResult analysis;
    QString diagnostic;
    double samplingPitchMillimetres = 0.0;
    double meanSurfaceDeviationBeforeProjectionMillimetres = 0.0;
    double meanSurfaceDeviationAfterProjectionMillimetres = 0.0;
    double surfaceProjectionFraction = 0.0;
    std::size_t projectedVertices = 0;
    SourceSurfaceAttemptMetrics metrics;
    bool successful = false;
};

struct SourceSurfaceFidelityAudit {
    bool bounded = true;
    bool completeOwnership = false;
    std::size_t reconstructedSamples = 0,sourceSamples = 0;
    std::size_t confidentlyOwnedFaces = 0,ambiguousFaces = 0;
    std::size_t sourceTrianglesRepresented = 0,sourceTrianglesUnresolved = 0;
    std::size_t reconstructedNormalMismatches = 0,sourceNormalMismatches = 0;
    double reconstructedToSourceMaximum = 0.0,sourceToReconstructedMaximum = 0.0;
    double reconstructedToSourceP95 = 0.0,sourceToReconstructedP95 = 0.0;
    double reconstructedToSourceRms = 0.0,sourceToReconstructedRms = 0.0;
    double maximumBoundsDeviation = 0.0;
    bool localFidelityPassed = false;
    qint64 elapsedMilliseconds = 0;
    QString diagnostic;
};

class SourceSurfaceSolidifier
{
public:
    enum class RayAxis { X, Y, Z };
    enum class QueryMode { Indexed, ExhaustiveReference };
    enum class ExtractionMode { MarchingTetrahedra, SurfaceNetsDiagnostic, TopologyAwareSurfaceNetsDiagnostic };
    static SourceSurfaceSolidificationResult solidify(
        const LDrawGeometry::LDrawLoadResult& source,
        double samplingPitchMillimetres = 0.15,
        RayAxis rayAxis = RayAxis::X,
        QueryMode queryMode = QueryMode::Indexed,
        qint64 maximumElapsedMilliseconds = 0,
        std::size_t maximumOutputFaces = 0,
        ExtractionMode extractionMode = ExtractionMode::MarchingTetrahedra);
    static SourceSurfaceFidelityAudit auditCandidate(
        const LDrawGeometry::LDrawLoadResult& source,const PrintMesh& candidate,
        qint64 maximumElapsedMilliseconds = 10000);
};

} // namespace PrintGeometry
