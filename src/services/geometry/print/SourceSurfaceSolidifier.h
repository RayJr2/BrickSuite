#pragma once

#include "../LDrawLoadResult.h"
#include "PrintMesh.h"

#include <QString>

namespace PrintGeometry {

struct SourceSurfaceSolidificationResult {
    PrintMesh mesh;
    MeshAnalysisResult analysis;
    QString diagnostic;
    double samplingPitchMillimetres = 0.0;
    double meanSurfaceDeviationBeforeProjectionMillimetres = 0.0;
    double meanSurfaceDeviationAfterProjectionMillimetres = 0.0;
    double surfaceProjectionFraction = 0.0;
    std::size_t projectedVertices = 0;
    bool successful = false;
};

class SourceSurfaceSolidifier
{
public:
    enum class RayAxis { X, Y, Z };
    static SourceSurfaceSolidificationResult solidify(
        const LDrawGeometry::LDrawLoadResult& source,
        double samplingPitchMillimetres = 0.15,
        RayAxis rayAxis = RayAxis::X);
};

} // namespace PrintGeometry
