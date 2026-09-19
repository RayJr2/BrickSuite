#pragma once
#include "PrintMesh.h"
#include <cstddef>
#include <vector>
namespace PrintGeometry {
struct MeshAnalysisOptions { bool collectIssues=true;std::size_t maximumCandidatePairs=20000000; };
MeshAnalysisResult analyzeSource(const PrintMesh&,const MeshAnalysisOptions& options = MeshAnalysisOptions{});
MeshValidationResult validateBooleanOperand(const MeshAnalysisResult&);
MeshValidationResult validatePreparedMesh(const MeshAnalysisResult&,bool allowMultipleComponents=false);
Point boundaryAttachmentDirection(const PrintMesh&,const std::vector<std::uint32_t>&);
double maximumBoundsDeviation(const MeshBounds&,const MeshBounds&);
}
