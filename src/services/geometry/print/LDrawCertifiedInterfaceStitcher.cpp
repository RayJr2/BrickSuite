#include "LDrawCertifiedInterfaceStitcher.h"

#include <QHash>
#include <QSet>

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <tuple>
#include <vector>

namespace PrintGeometry {
namespace {
using LDrawGeometry::SurfaceRecord;
using LDrawGeometry::Triangle;

struct Key {
    qint64 x = 0, y = 0, z = 0;
    bool operator<(const Key& other) const { return std::tie(x,y,z)<std::tie(other.x,other.y,other.z); }
    bool operator==(const Key& other) const { return x==other.x&&y==other.y&&z==other.z; }
};
struct EdgeKey {
    Key a,b;
    bool operator<(const EdgeKey& other) const { return std::tie(a,b)<std::tie(other.a,other.b); }
};
struct BoundaryEdge {
    int triangle = -1;
    int edge = -1;
    int component = -1;
    QVector3D a,b;
    Key aKey,bKey;
    QString color;
    SurfaceRecord surface;
};
struct Candidate {
    double position = 0.0;
    QVector3D point;
    Key key;
};

Key key(const QVector3D& point,double toleranceLdu)
{
    return {std::llround(double(point.x())/toleranceLdu),
            std::llround(double(point.y())/toleranceLdu),
            std::llround(double(point.z())/toleranceLdu)};
}
EdgeKey edgeKey(Key a,Key b){return b<a?EdgeKey{b,a}:EdgeKey{a,b};}
double length(const QVector3D& value){return std::sqrt(double(QVector3D::dotProduct(value,value)));}
double distanceToLine(const QVector3D& point,const QVector3D& a,const QVector3D& b,double* position)
{
    const QVector3D direction=b-a;const double denominator=double(QVector3D::dotProduct(direction,direction));
    if(denominator<=1e-20){*position=0.0;return 1e100;}
    *position=double(QVector3D::dotProduct(point-a,direction))/denominator;
    const QVector3D projection=a+float(*position)*direction;
    return length(point-projection);
}
bool officialCertified(const LDrawGeometry::LDrawLoadResult& source,const BoundaryEdge& edge)
{
    if(!edge.surface.certified||!edge.surface.clipping||!source.sourceModel)return false;
    const int file=edge.surface.fileId;
    return file>=0&&file<source.sourceModel->files.size()
        &&source.sourceModel->files[file].classification!=LDrawGeometry::SourceClassification::Unknown;
}
int boundaryCount(const QVector<Triangle>& triangles,double toleranceLdu)
{
    std::map<EdgeKey,int> uses;
    for(const auto& triangle:triangles){const std::array<QVector3D,3> points{triangle.a,triangle.b,triangle.c};
        for(int i=0;i<3;++i)++uses[edgeKey(key(points[i],toleranceLdu),key(points[(i+1)%3],toleranceLdu))];}
    return int(std::count_if(uses.cbegin(),uses.cend(),[](const auto& item){return item.second==1;}));
}
QVector<BoundaryEdge> boundaryEdges(const LDrawGeometry::LDrawLoadResult& source,double toleranceLdu)
{
    struct Use {int triangle=-1,edge=-1;};std::map<EdgeKey,QVector<Use>> uses;
    for(int ti=0;ti<source.mesh.triangles.size();++ti){const auto&t=source.mesh.triangles[ti];const std::array<QVector3D,3> p{t.a,t.b,t.c};
        for(int i=0;i<3;++i)uses[edgeKey(key(p[i],toleranceLdu),key(p[(i+1)%3],toleranceLdu))].push_back({ti,i});}
    std::vector<int> parent(source.mesh.triangles.size());for(int i=0;i<int(parent.size());++i)parent[i]=i;
    auto root=[&](int value){while(parent[value]!=value){parent[value]=parent[parent[value]];value=parent[value];}return value;};
    for(const auto&item:uses)if(item.second.size()>1){const int first=root(item.second.front().triangle);for(int i=1;i<item.second.size();++i)parent[root(item.second[i].triangle)]=first;}
    QVector<BoundaryEdge> result;
    for(const auto& item:uses)if(item.second.size()==1){const auto use=item.second.front();const auto&t=source.mesh.triangles[use.triangle];const std::array<QVector3D,3> p{t.a,t.b,t.c};BoundaryEdge edge;
        edge.triangle=use.triangle;edge.edge=use.edge;edge.component=root(use.triangle);edge.a=p[use.edge];edge.b=p[(use.edge+1)%3];edge.aKey=key(edge.a,toleranceLdu);edge.bKey=key(edge.b,toleranceLdu);edge.color=t.color;
        if(source.sourceModel&&use.triangle<source.sourceModel->surfaces.size())edge.surface=source.sourceModel->surfaces[use.triangle];
        result.push_back(edge);}
    std::sort(result.begin(),result.end(),[](const auto&a,const auto&b){return std::tie(a.triangle,a.edge)<std::tie(b.triangle,b.edge);});return result;
}
bool oppositeCollinearOverlap(const BoundaryEdge& target,const BoundaryEdge& other,double lineToleranceLdu,double endpointToleranceLdu,double* minimum,double* maximum)
{
    const QVector3D targetDirection=target.b-target.a,otherDirection=other.b-other.a;
    const double targetLength=length(targetDirection),otherLength=length(otherDirection);if(targetLength<=endpointToleranceLdu||otherLength<=endpointToleranceLdu)return false;
    const double directionDot=double(QVector3D::dotProduct(targetDirection,otherDirection))/(targetLength*otherLength);
    if(directionDot>-0.9999)return false;
    double p0=0,p1=0;if(distanceToLine(other.a,target.a,target.b,&p0)>lineToleranceLdu||distanceToLine(other.b,target.a,target.b,&p1)>lineToleranceLdu)return false;
    *minimum=std::max(0.0,std::min(p0,p1));*maximum=std::min(1.0,std::max(p0,p1));return *maximum-*minimum>endpointToleranceLdu/targetLength;
}
Triangle triangle(const QVector3D&a,const QVector3D&b,const QVector3D&c,const Triangle&source)
{
    Triangle value=source;value.a=a;value.b=b;value.c=c;const QVector3D normal=QVector3D::crossProduct(b-a,c-a);if(normal.lengthSquared()>1e-20f)value.normal=normal.normalized();return value;
}
}

CertifiedInterfaceStitchResult LDrawCertifiedInterfaceStitcher::stitch(
    const LDrawGeometry::LDrawLoadResult& source,const LDrawPrintPreparationProfile& profile)
{
    CertifiedInterfaceStitchResult result;result.loadResult=source;
    if(source.sourceModel)result.loadResult.sourceModel=std::make_shared<LDrawGeometry::LDrawSourceModel>(*source.sourceModel);
    auto& diagnostics=result.diagnostics;diagnostics.trianglesBefore=source.mesh.triangles.size();
    const double weldLdu=profile.seamWeldMillimetres/0.4;
    const double lineToleranceLdu=profile.planarityToleranceMillimetres/0.4;
    diagnostics.boundariesBefore=boundaryCount(source.mesh.triangles,weldLdu);
    if(!source.ok()||!source.sourceModel||source.sourceModel->surfaces.size()!=source.mesh.triangles.size()){
        diagnostics.trianglesAfter=diagnostics.trianglesBefore;diagnostics.boundariesAfter=diagnostics.boundariesBefore;
        diagnostics.messages<<QStringLiteral("Certified stitching skipped because complete triangle provenance was unavailable.");return result;}

    const auto edges=boundaryEdges(source,weldLdu);QHash<int,QVector<Candidate>> splits;
    for(const auto& target:edges){if(!officialCertified(source,target))continue;QVector<Candidate> candidates;std::vector<std::pair<double,double>> coverage;
        for(const auto& other:edges){if(other.triangle==target.triangle&&other.edge==target.edge)continue;if(other.component!=target.component||other.color!=target.color||!officialCertified(source,other))continue;
            double minimum=0,maximum=0;if(!oppositeCollinearOverlap(target,other,lineToleranceLdu,weldLdu,&minimum,&maximum))continue;coverage.push_back({minimum,maximum});
            for(const auto& endpoint:{std::pair<QVector3D,Key>{other.a,other.aKey},std::pair<QVector3D,Key>{other.b,other.bKey}}){double position=0;const double distance=distanceToLine(endpoint.first,target.a,target.b,&position);
                if(distance<=lineToleranceLdu&&position>weldLdu/length(target.b-target.a)&&position<1.0-weldLdu/length(target.b-target.a))candidates.push_back({position,endpoint.first,endpoint.second});}}
        if(candidates.isEmpty())continue;++diagnostics.candidateRelationships;
        std::sort(coverage.begin(),coverage.end());double covered=0.0;bool complete=false;for(const auto& interval:coverage){if(interval.first>covered+weldLdu/length(target.b-target.a))break;covered=std::max(covered,interval.second);if(covered>=1.0-weldLdu/length(target.b-target.a)){complete=true;break;}}if(!complete)continue;
        std::sort(candidates.begin(),candidates.end(),[](const auto&a,const auto&b){if(a.position!=b.position)return a.position<b.position;return std::tie(a.key.x,a.key.y,a.key.z)<std::tie(b.key.x,b.key.y,b.key.z);});QVector<Candidate> unique;bool ambiguous=false;
        for(const auto& candidate:candidates){auto same=std::find_if(unique.begin(),unique.end(),[&](const auto&existing){return existing.key==candidate.key;});if(same==unique.end())unique.push_back(candidate);else if((same->point-candidate.point).length()>float(lineToleranceLdu))ambiguous=true;}
        if(ambiguous){++diagnostics.rejectedAmbiguousCandidates;continue;}splits[target.triangle*3+target.edge]=unique;diagnostics.acceptedSplits+=unique.size();}

    for(int ti=0;ti<source.mesh.triangles.size();++ti){int splitEdges=0,splitPoints=0;for(int edge=0;edge<3;++edge)if(!splits.value(ti*3+edge).isEmpty()){++splitEdges;splitPoints+=splits.value(ti*3+edge).size();}
        if(splitEdges<=1)continue;for(int edge=0;edge<3;++edge)splits.remove(ti*3+edge);diagnostics.acceptedSplits-=splitPoints;++diagnostics.rejectedAmbiguousCandidates;}

    QVector<Triangle> triangles;QVector<SurfaceRecord> surfaces;triangles.reserve(source.mesh.triangles.size()+diagnostics.acceptedSplits);surfaces.reserve(triangles.capacity());
    for(int ti=0;ti<source.mesh.triangles.size();++ti){const auto&original=source.mesh.triangles[ti];const std::array<QVector3D,3> point{original.a,original.b,original.c};int splitEdge=-1;
        for(int edge=0;edge<3;++edge)if(!splits.value(ti*3+edge).isEmpty()){if(splitEdge>=0){splitEdge=-2;break;}splitEdge=edge;}
        QVector<Triangle> replacements;
        if(splitEdge<0)replacements.push_back(original);
        else {QVector<QVector3D> chain;chain.push_back(point[splitEdge]);for(const auto&candidate:splits.value(ti*3+splitEdge))chain.push_back(candidate.point);chain.push_back(point[(splitEdge+1)%3]);const QVector3D opposite=point[(splitEdge+2)%3];
            for(int i=0;i+1<chain.size();++i)replacements.push_back(triangle(chain[i],chain[i+1],opposite,original));}
        const SurfaceRecord provenance=source.sourceModel->surfaces[ti];for(const auto&next:replacements){SurfaceRecord copy=provenance;copy.triangleIndex=triangles.size();triangles.push_back(next);surfaces.push_back(copy);}}
    result.changed=triangles.size()!=source.mesh.triangles.size();if(result.changed){result.loadResult.mesh.triangles=triangles;result.loadResult.sourceModel->surfaces=surfaces;}
    diagnostics.trianglesAfter=result.loadResult.mesh.triangles.size();diagnostics.boundariesAfter=boundaryCount(result.loadResult.mesh.triangles,weldLdu);
    diagnostics.messages<<QStringLiteral("Certified interface stitching: candidates=%1 acceptedSplits=%2 ambiguous=%3 triangles=%4->%5 boundaries=%6->%7.")
        .arg(diagnostics.candidateRelationships).arg(diagnostics.acceptedSplits).arg(diagnostics.rejectedAmbiguousCandidates).arg(diagnostics.trianglesBefore).arg(diagnostics.trianglesAfter).arg(diagnostics.boundariesBefore).arg(diagnostics.boundariesAfter);
    return result;
}

} // namespace PrintGeometry
