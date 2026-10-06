/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 *
 * This file is part of BrickSuite.
 *
 * BrickSuite is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, version 3 of the License.
 *
 * BrickSuite is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with BrickSuite. If not, see <https://www.gnu.org/licenses/>.
 */

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
