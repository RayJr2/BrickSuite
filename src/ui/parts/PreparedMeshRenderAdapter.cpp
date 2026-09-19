#include "PreparedMeshRenderAdapter.h"

#include <QHash>
#include <QSet>
#include <algorithm>
#include <array>
#include <cmath>

namespace {
QVector3D viewerPoint(const PrintGeometry::Point&p){return {float(p.x/0.4),float(-p.z/0.4),float(p.y/0.4)};}
quint64 edgeKey(std::uint32_t a,std::uint32_t b){if(a>b)std::swap(a,b);return (quint64(a)<<32)|quint64(b);}
QVector3D normal(const QVector3D&a,const QVector3D&b,const QVector3D&c){const auto n=QVector3D::crossProduct(b-a,c-a);return n.lengthSquared()>0?n.normalized():QVector3D{};}
struct DoubleNormal{double x=0,y=0,z=0;bool valid=false;};
DoubleNormal normal(const PrintGeometry::Point&a,const PrintGeometry::Point&b,const PrintGeometry::Point&c)
{
    const double ux=b.x-a.x,uy=b.y-a.y,uz=b.z-a.z,vx=c.x-a.x,vy=c.y-a.y,vz=c.z-a.z;
    DoubleNormal result{uy*vz-uz*vy,uz*vx-ux*vz,ux*vy-uy*vx,false};const double length=std::sqrt(result.x*result.x+result.y*result.y+result.z*result.z);
    if(length>0){result.x/=length;result.y/=length;result.z/=length;result.valid=true;}return result;
}
double dot(const DoubleNormal&a,const DoubleNormal&b){return a.x*b.x+a.y*b.y+a.z*b.z;}
struct EdgeAdjacency{std::uint32_t first=0,second=0;QVector<DoubleNormal> normals;};
}

PreparedMeshRenderData PreparedMeshRenderAdapter::fromPreparedMesh(const PrintGeometry::PreparedMesh&prepared)
{
    PreparedMeshRenderData out;QHash<quint64,EdgeAdjacency>edges;QVector<quint64>edgeOrder;
    const auto&mesh=prepared.mesh;out.triangles.reserve(int(mesh.faces.size()));out.topologyEdges.reserve(int(mesh.faces.size()*3));
    for(const auto&face:mesh.faces){if(face[0]>=mesh.vertices.size()||face[1]>=mesh.vertices.size()||face[2]>=mesh.vertices.size())continue;
        const auto a=viewerPoint(mesh.vertices[face[0]]),b=viewerPoint(mesh.vertices[face[1]]),c=viewerPoint(mesh.vertices[face[2]]);
        out.triangles.push_back({a,b,c,normal(a,b,c)});
        const auto faceNormal=normal(mesh.vertices[face[0]],mesh.vertices[face[1]],mesh.vertices[face[2]]);
        const std::array<std::pair<std::uint32_t,std::uint32_t>,3>pairs{{{face[0],face[1]},{face[1],face[2]},{face[2],face[0]}}};
        for(const auto&e:pairs){const auto key=edgeKey(e.first,e.second);if(!edges.contains(key)){edges.insert(key,{e.first,e.second,{}});edgeOrder.push_back(key);out.topologyEdges.push_back({viewerPoint(mesh.vertices[e.first]),viewerPoint(mesh.vertices[e.second])});}if(faceNormal.valid)edges[key].normals.push_back(faceNormal);}
    }
    const double featureDot=std::cos(FeatureAngleDegrees*3.14159265358979323846/180.0);out.featureEdges.reserve(out.topologyEdges.size());
    for(const auto key:edgeOrder){const auto&edge=edges[key];const bool unusual=edge.normals.size()!=2;const bool crease=!unusual&&std::clamp(dot(edge.normals[0],edge.normals[1]),-1.0,1.0)<featureDot;if(unusual||crease)out.featureEdges.push_back({viewerPoint(mesh.vertices[edge.first]),viewerPoint(mesh.vertices[edge.second])});}
    if(prepared.millimetreBounds.valid){out.minimumBounds=viewerPoint(prepared.millimetreBounds.minimum);out.maximumBounds=viewerPoint(prepared.millimetreBounds.maximum);const auto low=QVector3D(std::min(out.minimumBounds.x(),out.maximumBounds.x()),std::min(out.minimumBounds.y(),out.maximumBounds.y()),std::min(out.minimumBounds.z(),out.maximumBounds.z()));const auto high=QVector3D(std::max(out.minimumBounds.x(),out.maximumBounds.x()),std::max(out.minimumBounds.y(),out.maximumBounds.y()),std::max(out.minimumBounds.z(),out.maximumBounds.z()));out.minimumBounds=low;out.maximumBounds=high;out.hasBounds=true;}
    return out;
}

const QVector<PreparedMeshRenderLine>& PreparedMeshRenderAdapter::edgesForMode(const PreparedMeshRenderData&mesh,PartViewerRenderMode mode)
{
    static const QVector<PreparedMeshRenderLine>none;
    if(mode==PartViewerRenderMode::Wireframe)return mesh.topologyEdges;
    if(mode==PartViewerRenderMode::SolidEdges)return mesh.featureEdges;
    return none;
}

SourceMeshIssueRenderData PreparedMeshRenderAdapter::sourceIssues(const PrintGeometry::PrintMesh&mesh,const PrintGeometry::MeshAnalysisResult&analysis,std::size_t maximumLines)
{
    SourceMeshIssueRenderData out;QSet<quint64>seen;
    for(const auto&issue:analysis.issues){if(issue.type!=PrintGeometry::MeshIssueType::BoundaryEdge&&issue.type!=PrintGeometry::MeshIssueType::NonManifoldEdge)continue;if(issue.first>=mesh.vertices.size()||issue.second>=mesh.vertices.size())continue;const auto key=edgeKey(std::uint32_t(issue.first),std::uint32_t(issue.second));if(seen.contains(key))continue;seen.insert(key);if(std::size_t(out.boundaryEdges.size()+out.nonManifoldEdges.size())>=maximumLines){out.truncated=true;break;}auto line=PreparedMeshRenderLine{viewerPoint(mesh.vertices[issue.first]),viewerPoint(mesh.vertices[issue.second])};if(issue.type==PrintGeometry::MeshIssueType::BoundaryEdge)out.boundaryEdges.push_back(line);else out.nonManifoldEdges.push_back(line);}
    return out;
}
