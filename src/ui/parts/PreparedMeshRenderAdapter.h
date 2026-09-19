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
