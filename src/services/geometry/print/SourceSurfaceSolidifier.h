#pragma once

#include "../LDrawLoadResult.h"
#include "PrintMesh.h"

#include <QString>
#include <cstdint>

namespace PrintGeometry {

struct SourceSurfaceAttemptMetrics {
    int rayAxis = 0;
    int gridX = 0, gridY = 0, gridZ = 0;
    std::size_t finalVertices = 0, finalFaces = 0;
    std::uint64_t rayTriangleTests = 0, exhaustiveRayTriangleTests = 0;
    std::uint64_t nearestTriangleTests = 0, exhaustiveNearestTriangleTests = 0;
    qint64 sourceAnalysisMilliseconds = 0;
    qint64 indexMilliseconds = 0;
    qint64 rayClassificationMilliseconds = 0;
    qint64 extractionMilliseconds = 0;
    qint64 nearestProjectionMilliseconds = 0;
    qint64 orientationMilliseconds = 0;
    qint64 strictAnalysisMilliseconds = 0;
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

class SourceSurfaceSolidifier
{
public:
    enum class RayAxis { X, Y, Z };
    enum class QueryMode { Indexed, ExhaustiveReference };
    static SourceSurfaceSolidificationResult solidify(
        const LDrawGeometry::LDrawLoadResult& source,
        double samplingPitchMillimetres = 0.15,
        RayAxis rayAxis = RayAxis::X,
        QueryMode queryMode = QueryMode::Indexed);
};

} // namespace PrintGeometry
