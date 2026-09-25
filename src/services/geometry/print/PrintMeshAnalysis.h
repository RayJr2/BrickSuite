#pragma once
#include "PrintMesh.h"
#include <cstddef>
#include <cstdint>
#include <vector>
namespace PrintGeometry {
struct MeshAnalysisMetrics {
    std::uint64_t boundsMicroseconds=0,faceBuildMicroseconds=0,edgeAccountingMicroseconds=0;
    std::uint64_t componentsMicroseconds=0,vertexFansMicroseconds=0;
    std::uint64_t broadPhaseMicroseconds=0,exactIntersectionMicroseconds=0;
    std::uint64_t exactIntersectionTests=0;
};
struct MeshAnalysisOptions { bool collectIssues=true;std::size_t maximumCandidatePairs=20000000;MeshAnalysisMetrics* metrics=nullptr;bool referenceBroadPhase=false; };
MeshAnalysisResult analyzeSource(const PrintMesh&,const MeshAnalysisOptions& options = MeshAnalysisOptions{});
MeshValidationResult validateBooleanOperand(const MeshAnalysisResult&);
MeshValidationResult validatePreparedMesh(const MeshAnalysisResult&,bool allowMultipleComponents=false);
Point boundaryAttachmentDirection(const PrintMesh&,const std::vector<std::uint32_t>&);
double maximumBoundsDeviation(const MeshBounds&,const MeshBounds&);
}
