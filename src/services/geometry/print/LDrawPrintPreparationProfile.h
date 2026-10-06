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
#include <cstddef>
#include <QString>
namespace PrintGeometry {
struct LDrawPrintPreparationProfile {
    static constexpr const char* Version="ordinary-brick-bounded-v2";
    QString identity=QString::fromLatin1(Version);
    double seamWeldMillimetres=0.0004,planarityToleranceMillimetres=0.00005;
    double boundaryContactToleranceMillimetres=0.001;
    double ordinaryAttachmentIntrusionMillimetres=0.1;
    double maximumExternalBoundsDeviationMillimetres=0.001;
    std::size_t maximumSemanticGroups=4096,maximumBoundaryLoops=8192,maximumLoopEdges=4096,maximumOperands=512;
    // Sequential CSG repeatedly analyzes and copies the growing result. Bound
    // the operation count before entering MCUT, independently of mesh size.
    static constexpr std::size_t MaximumSequentialBooleanOperations=8;
    std::size_t maximumBooleanOperations=MaximumSequentialBooleanOperations;
    std::size_t maximumVertices=2000000,maximumFaces=4000000;
    std::size_t maximumIntersectionCandidates=20000000;
};
}
