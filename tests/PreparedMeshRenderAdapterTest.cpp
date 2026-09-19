#include "../src/ui/parts/PreparedMeshRenderAdapter.h"
#include <QCoreApplication>
#include <QTextStream>
#include <cmath>

namespace { bool check(bool value,const char*message){if(!value)QTextStream(stderr)<<"FAIL: "<<message<<Qt::endl;return value;} }

int main(int argc,char**argv)
{
    QCoreApplication app(argc,argv);bool ok=true;PrintGeometry::PreparedMesh prepared;
    prepared.mesh.vertices={{0,0,0},{4,0,0},{4,4,0},{0,4,0},{0,0,4}};
    prepared.mesh.faces={{0,1,2},{0,2,3},{0,4,1}};prepared.millimetreBounds={{0,0,0},{4,4,4},true};
    const auto original=prepared.mesh;
    const auto render=PreparedMeshRenderAdapter::fromPreparedMesh(prepared);
    ok&=check(render.triangles.size()==3,"all Prepared triangles adapted");
    ok&=check(render.topologyEdges.size()==7,"complete topology edges emitted once");
    ok&=check(render.featureEdges.size()==6,"coplanar triangulation diagonal omitted from feature edges");
    ok&=check(render.featureEdges.size()<render.topologyEdges.size(),"feature edge set filtered");
    const auto hasLine=[](const QVector<PreparedMeshRenderLine>&lines,const QVector3D&a,const QVector3D&b){for(const auto&line:lines)if((line.a==a&&line.b==b)||(line.a==b&&line.b==a))return true;return false;};
    ok&=check(!hasLine(render.featureEdges,{0,0,0},{10,0,10}),"coplanar shared diagonal suppressed");
    ok&=check(hasLine(render.featureEdges,{0,0,0},{10,0,0}),"non-coplanar shared crease retained");
    ok&=check(render.hasBounds&&qFuzzyCompare(render.maximumBounds.x(),10.0f),"millimetre bounds converted to viewer coordinates");
    ok&=check(render.triangles.front().normal.lengthSquared()>0.99f,"deterministic face normal generated");
    const auto repeated=PreparedMeshRenderAdapter::fromPreparedMesh(prepared);
    ok&=check(repeated.featureEdges.size()==render.featureEdges.size(),"feature edge output deterministic");
    bool same=true;for(int i=0;i<render.featureEdges.size();++i)same&=render.featureEdges[i].a==repeated.featureEdges[i].a&&render.featureEdges[i].b==repeated.featureEdges[i].b;
    ok&=check(same,"feature edge ordering deterministic");
    ok&=check(&PreparedMeshRenderAdapter::edgesForMode(render,PartViewerRenderMode::Wireframe)==&render.topologyEdges,"Wireframe uses complete topology edges");
    ok&=check(&PreparedMeshRenderAdapter::edgesForMode(render,PartViewerRenderMode::SolidEdges)==&render.featureEdges,"Solid plus Edges uses filtered feature edges");
    ok&=check(PreparedMeshRenderAdapter::edgesForMode(render,PartViewerRenderMode::Solid).isEmpty(),"Solid uses no edge overlay");
    bool verticesUnchanged=prepared.mesh.vertices.size()==original.vertices.size();for(std::size_t i=0;verticesUnchanged&&i<original.vertices.size();++i){const auto&a=prepared.mesh.vertices[i];const auto&b=original.vertices[i];verticesUnchanged=a.x==b.x&&a.y==b.y&&a.z==b.z;}
    ok&=check(verticesUnchanged&&prepared.mesh.faces==original.faces,"PreparedMesh remains unchanged");

    PrintGeometry::MeshAnalysisResult analysis;analysis.issues={{PrintGeometry::MeshIssueType::BoundaryEdge,0,1},{PrintGeometry::MeshIssueType::BoundaryEdge,1,0},{PrintGeometry::MeshIssueType::NonManifoldEdge,1,2}};
    const auto issues=PreparedMeshRenderAdapter::sourceIssues(prepared.mesh,analysis);
    ok&=check(issues.boundaryEdges.size()==1&&issues.nonManifoldEdges.size()==1,"diagnostic issue edges classified and deduplicated");
    const auto limited=PreparedMeshRenderAdapter::sourceIssues(prepared.mesh,analysis,1);
    ok&=check(limited.truncated&&limited.boundaryEdges.size()+limited.nonManifoldEdges.size()==1,"diagnostic resource limit reported");
    return ok?0:1;
}
