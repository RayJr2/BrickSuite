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

#include "../../services/geometry/print/PreparedMesh.h"
#include "../../services/geometry/PartViewerState.h"

#include <QVector>
#include <QVector3D>

struct PreparedMeshRenderTriangle { QVector3D a,b,c,normal; };
struct PreparedMeshRenderLine { QVector3D a,b; };
struct PreparedMeshRenderData {
    QVector<PreparedMeshRenderTriangle> triangles;
    QVector<PreparedMeshRenderLine> topologyEdges;
    QVector<PreparedMeshRenderLine> featureEdges;
    QVector3D minimumBounds,maximumBounds;
    bool hasBounds=false;
};
struct SourceMeshIssueRenderData {
    QVector<PreparedMeshRenderLine> boundaryEdges;
    QVector<PreparedMeshRenderLine> nonManifoldEdges;
    bool truncated=false;
};

class PreparedMeshRenderAdapter
{
public:
    static constexpr double FeatureAngleDegrees=8.0;
    static PreparedMeshRenderData fromPreparedMesh(const PrintGeometry::PreparedMesh& mesh);
    static const QVector<PreparedMeshRenderLine>& edgesForMode(const PreparedMeshRenderData& mesh,PartViewerRenderMode mode);
    static SourceMeshIssueRenderData sourceIssues(const PrintGeometry::PrintMesh& mesh,const PrintGeometry::MeshAnalysisResult& analysis,std::size_t maximumLines=200000);
};
