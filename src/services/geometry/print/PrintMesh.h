#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace PrintGeometry {
struct Point { double x=0.0,y=0.0,z=0.0; };
using Face=std::array<std::uint32_t,3>;

// Double-precision, Z-up, millimetre, nominal 100% physical geometry.
struct PrintMesh { std::vector<Point> vertices; std::vector<Face> faces; };
struct MeshBounds { Point minimum; Point maximum; bool valid=false; };

enum class MeshIssueType { NonFiniteVertex,InvalidIndex,DegenerateFace,DuplicateFace,
    BoundaryEdge,NonManifoldEdge,NonManifoldVertex,SelfIntersection };
struct MeshIssue { MeshIssueType type=MeshIssueType::InvalidIndex;std::size_t first=0,second=0; };

struct MeshAnalysisResult {
    std::size_t vertices=0,triangles=0,invalidVertices=0,invalidIndices=0;
    std::size_t degenerateFaces=0,duplicateFaces=0,boundaryEdges=0;
    std::size_t nonManifoldEdges=0,nonManifoldVertices=0,selfIntersections=0;
    std::size_t connectedComponents=0,broadPhaseCandidatePairs=0,narrowPhaseChecks=0;
    bool finite=true,indicesValid=true,consistentlyOriented=true,resourceLimitExceeded=false;
    MeshBounds bounds;double signedVolume=0.0,absoluteVolume=0.0;
    std::vector<MeshIssue> issues;
};

enum class MeshValidationError { None,Empty,NonFinite,InvalidIndex,DegenerateFace,
    DuplicateFace,BoundaryEdge,NonManifoldEdge,NonManifoldVertex,SelfIntersection,
    MultipleComponents,InconsistentOrientation,NonPositiveVolume,ResourceLimitExceeded };
struct MeshValidationResult { MeshValidationError error=MeshValidationError::None;
    std::string message;bool ok()const{return error==MeshValidationError::None;} };

enum class MeshBooleanError { None,InvalidSource,InvalidAdditive,ResourceLimitExceeded,
    BackendFailure,InvalidResult };
struct MeshBooleanResult { MeshBooleanError error=MeshBooleanError::BackendFailure;
    PrintMesh mesh;MeshAnalysisResult sourceAnalysis,additiveAnalysis,resultAnalysis;
    std::string message;bool ok()const{return error==MeshBooleanError::None;} };
} // namespace PrintGeometry
