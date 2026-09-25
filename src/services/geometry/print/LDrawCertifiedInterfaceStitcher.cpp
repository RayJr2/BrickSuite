#include "LDrawCertifiedInterfaceStitcher.h"
#include "PrintMeshAnalysis.h"

#include <QHash>
#include <QSet>
#include <QElapsedTimer>

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <limits>
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

// These cuts use intersections of the authored triangle planes. They partition
// existing surfaces only: no cap, bridge, or inferred volume is generated.
struct PlaneCut {QVector3D normal;double offset=0.0;};
double signedDistance(const PlaneCut&plane,const QVector3D&point)
{return double(QVector3D::dotProduct(plane.normal,point))-plane.offset;}
PlaneCut planeFor(const Triangle&t)
{
    const auto normal=QVector3D::crossProduct(t.b-t.a,t.c-t.a);
    const float magnitude=normal.length();
    if(magnitude<=1e-12f)return {};
    const auto unit=normal/magnitude;
    return {unit,double(QVector3D::dotProduct(unit,t.a))};
}
double polygonArea(const QVector<QVector3D>&polygon)
{
    if(polygon.size()<3)return 0.0;
    QVector3D sum;
    for(int i=0;i<polygon.size();++i)
        sum+=QVector3D::crossProduct(polygon[i],polygon[(i+1)%polygon.size()]);
    return .5*double(sum.length());
}
QVector<QVector3D> clipPolygon(const QVector<QVector3D>&polygon,const PlaneCut&plane,
                              double epsilon,bool positive)
{
    QVector<QVector3D> out;
    for(int i=0;i<polygon.size();++i){
        const auto a=polygon[i],b=polygon[(i+1)%polygon.size()];
        const double da=signedDistance(plane,a)*(positive?1.0:-1.0);
        const double db=signedDistance(plane,b)*(positive?1.0:-1.0);
        if(da>=-epsilon)out.push_back(a);
        if((da>epsilon&&db<-epsilon)||(da<-epsilon&&db>epsilon))
            out.push_back(a+float(da/(da-db))*(b-a));
    }
    return out;
}
bool transverseOverlap(const Triangle&a,const Triangle&b,const PlaneCut&pa,
                       const PlaneCut&pb,double epsilon)
{
    const auto axis=QVector3D::crossProduct(pa.normal,pb.normal).normalized();
    QVector3D linePoint;
    bool haveLinePoint=false;
    auto interval=[&](const Triangle&t,const PlaneCut&plane){
        const std::array<QVector3D,3> vertices{t.a,t.b,t.c};
        double minimum=std::numeric_limits<double>::infinity(),maximum=-minimum;
        for(int i=0;i<3;++i){
            const auto p=vertices[i],q=vertices[(i+1)%3];
            const double dp=signedDistance(plane,p),dq=signedDistance(plane,q);
            if(std::abs(dp)<=epsilon){const double s=double(QVector3D::dotProduct(axis,p));minimum=std::min(minimum,s);maximum=std::max(maximum,s);if(!haveLinePoint){linePoint=p;haveLinePoint=true;}}
            if((dp>epsilon&&dq<-epsilon)||(dp<-epsilon&&dq>epsilon)){
                const auto hit=p+float(dp/(dp-dq))*(q-p);
                const double s=double(QVector3D::dotProduct(axis,hit));minimum=std::min(minimum,s);maximum=std::max(maximum,s);
                if(!haveLinePoint){linePoint=hit;haveLinePoint=true;}
            }
        }
        return std::pair<double,double>{minimum,maximum};
    };
    const auto ia=interval(a,pb),ib=interval(b,pa);
    if(!std::isfinite(ia.first)||!std::isfinite(ib.first)||
       std::min(ia.second,ib.second)-std::max(ia.first,ib.first)<=epsilon||!haveLinePoint)return false;
    const double midpoint=.5*(std::min(ia.second,ib.second)+std::max(ia.first,ib.first));
    const auto point=linePoint+axis*float(midpoint-double(QVector3D::dotProduct(axis,linePoint)));
    auto strictlyInside=[&](const Triangle&t,const QVector3D&normal){
        const std::array<QVector3D,3>v{t.a,t.b,t.c};
        for(int k=0;k<3;++k){
            const auto edge=v[(k+1)%3]-v[k];
            const double side=double(QVector3D::dotProduct(
                QVector3D::crossProduct(edge,point-v[k]),normal));
            if(side<=epsilon*double(edge.length()))return false;
        }
        return true;
    };
    return strictlyInside(a,pa.normal)||strictlyInside(b,pb.normal);
}
bool coplanarOverlap(const Triangle&a,const Triangle&b,const PlaneCut&plane,double epsilon)
{
    QVector<QVector3D> polygon{a.a,a.b,a.c};
    const std::array<QVector3D,3> vertices{b.a,b.b,b.c};
    for(int i=0;i<3;++i){
        const auto edge=vertices[(i+1)%3]-vertices[i];
        const auto inward=QVector3D::crossProduct(plane.normal,edge);
        const auto cut=PlaneCut{inward,double(QVector3D::dotProduct(inward,vertices[i]))};
        polygon=clipPolygon(polygon,cut,epsilon,true);
        if(polygon.size()<3)return false;
    }
    return polygonArea(polygon)>epsilon*epsilon;
}
bool overlappingBounds(const Triangle&a,const Triangle&b,double epsilon)
{
    const std::array<QVector3D,3> x{a.a,a.b,a.c},y{b.a,b.b,b.c};
    for(int axis=0;axis<3;++axis){
        double x0=1e100,x1=-1e100,y0=1e100,y1=-1e100;
        for(const auto&p:x){x0=std::min(x0,double(p[axis]));x1=std::max(x1,double(p[axis]));}
        for(const auto&p:y){y0=std::min(y0,double(p[axis]));y1=std::max(y1,double(p[axis]));}
        if(std::min(x1,y1)<std::max(x0,y0)-epsilon)return false;
    }
    return true;
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
    result.expandedTriangleForStitchedTriangle.reserve(source.mesh.triangles.size());
    for(int i=0;i<source.mesh.triangles.size();++i)result.expandedTriangleForStitchedTriangle.push_back(i);
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

    QVector<Triangle> triangles;QVector<SurfaceRecord> surfaces;QVector<int> ancestry;triangles.reserve(source.mesh.triangles.size()+diagnostics.acceptedSplits);surfaces.reserve(triangles.capacity());ancestry.reserve(triangles.capacity());
    for(int ti=0;ti<source.mesh.triangles.size();++ti){const auto&original=source.mesh.triangles[ti];const std::array<QVector3D,3> point{original.a,original.b,original.c};int splitEdge=-1;
        for(int edge=0;edge<3;++edge)if(!splits.value(ti*3+edge).isEmpty()){if(splitEdge>=0){splitEdge=-2;break;}splitEdge=edge;}
        QVector<Triangle> replacements;
        if(splitEdge<0)replacements.push_back(original);
        else {QVector<QVector3D> chain;chain.push_back(point[splitEdge]);for(const auto&candidate:splits.value(ti*3+splitEdge))chain.push_back(candidate.point);chain.push_back(point[(splitEdge+1)%3]);const QVector3D opposite=point[(splitEdge+2)%3];
            for(int i=0;i+1<chain.size();++i)replacements.push_back(triangle(chain[i],chain[i+1],opposite,original));}
        const SurfaceRecord provenance=source.sourceModel->surfaces[ti];for(const auto&next:replacements){SurfaceRecord copy=provenance;copy.triangleIndex=triangles.size();triangles.push_back(next);surfaces.push_back(copy);ancestry.push_back(ti);}}
    result.expandedTriangleForStitchedTriangle=std::move(ancestry);
    result.changed=triangles.size()!=source.mesh.triangles.size();if(result.changed){result.loadResult.mesh.triangles=triangles;result.loadResult.sourceModel->surfaces=surfaces;}
    diagnostics.trianglesAfter=result.loadResult.mesh.triangles.size();diagnostics.boundariesAfter=boundaryCount(result.loadResult.mesh.triangles,weldLdu);
    diagnostics.messages<<QStringLiteral("Certified interface stitching: candidates=%1 acceptedSplits=%2 ambiguous=%3 triangles=%4->%5 boundaries=%6->%7.")
        .arg(diagnostics.candidateRelationships).arg(diagnostics.acceptedSplits).arg(diagnostics.rejectedAmbiguousCandidates).arg(diagnostics.trianglesBefore).arg(diagnostics.trianglesAfter).arg(diagnostics.boundariesBefore).arg(diagnostics.boundariesAfter);
    return result;
}

SurfaceIntersectionArrangement LDrawCertifiedInterfaceStitcher::arrangeIntersections(
    const LDrawGeometry::LDrawLoadResult& source,const LDrawPrintPreparationProfile& profile)
{
    SurfaceIntersectionArrangement result;
    result.loadResult=source;
    const auto& triangles=source.mesh.triangles;
    result.fragmentsBefore=triangles.size();
    result.fragmentsAfter=triangles.size();
    result.stitchedTriangleForFragment.reserve(triangles.size());
    for(int i=0;i<triangles.size();++i)result.stitchedTriangleForFragment.push_back(i);
    if(!source.ok()||!source.sourceModel||source.sourceModel->surfaces.size()!=triangles.size()){
        result.diagnostic=QStringLiteral("Intersection arrangement requires complete authoritative provenance.");
        result.bounded=false;return result;
    }
    constexpr int MaxInputTriangles=5000,MaxCandidatePairs=150000,MaxCutsPerTriangle=96,
                  MaxFragments=200000,MaxCoplanarPairs=10000;
    if(triangles.size()>MaxInputTriangles){
        result.diagnostic=QStringLiteral("Intersection arrangement input triangle limit exceeded.");
        result.bounded=false;return result;
    }
    const double epsilon=std::max(2e-5,profile.planarityToleranceMillimetres/0.4);
    QElapsedTimer timer;timer.start();
    QVector<QVector<PlaneCut>> cuts(triangles.size());
    QVector<PlaneCut> planes;planes.reserve(triangles.size());
    for(const auto& t:triangles)planes.push_back(planeFor(t));
    for(int i=0;i<triangles.size();++i){
        const auto&sa=source.sourceModel->surfaces[i];
        if(!sa.certified||!sa.clipping||planes[i].normal.isNull())continue;
        for(int j=i+1;j<triangles.size();++j){
            if((j&255)==0&&timer.elapsed()>10000){result.bounded=false;break;}
            const auto&sb=source.sourceModel->surfaces[j];
            if(sa.referenceId==sb.referenceId||!sb.certified||!sb.clipping||
               planes[j].normal.isNull()||triangles[i].color!=triangles[j].color||
               !overlappingBounds(triangles[i],triangles[j],epsilon))continue;
            if(++result.candidatePairs>MaxCandidatePairs){result.bounded=false;break;}
            const auto normalCross=QVector3D::crossProduct(planes[i].normal,planes[j].normal);
            if(normalCross.length()<1e-5f){
                if(std::abs(signedDistance(planes[i],triangles[j].a))>epsilon||
                   !coplanarOverlap(triangles[i],triangles[j],planes[j],epsilon))continue;
                if(++result.coplanarOverlapPairs>MaxCoplanarPairs){result.bounded=false;break;}
                const std::array<QVector3D,3>a{triangles[i].a,triangles[i].b,triangles[i].c},
                                               b{triangles[j].a,triangles[j].b,triangles[j].c};
                for(int k=0;k<3;++k){
                    const auto na=QVector3D::crossProduct(planes[i].normal,a[(k+1)%3]-a[k]);
                    const auto nb=QVector3D::crossProduct(planes[j].normal,b[(k+1)%3]-b[k]);
                    cuts[j].push_back({na,double(QVector3D::dotProduct(na,a[k]))});
                    cuts[i].push_back({nb,double(QVector3D::dotProduct(nb,b[k]))});
                }
            }else if(transverseOverlap(triangles[i],triangles[j],planes[i],planes[j],epsilon)){
                ++result.transversePairs;
                cuts[i].push_back(planes[j]);cuts[j].push_back(planes[i]);
            }
            if(cuts[i].size()>MaxCutsPerTriangle||cuts[j].size()>MaxCutsPerTriangle){
                result.bounded=false;break;
            }
        }
        if(!result.bounded)break;
    }
    if(!result.bounded){
        result.diagnostic=QStringLiteral("Intersection arrangement candidate/coplanar/cut limit exceeded; no source was changed.");
        return result;
    }
    QVector<Triangle> fragments;QVector<SurfaceRecord> surfaces;QVector<int> parents;
    fragments.reserve(triangles.size());surfaces.reserve(triangles.size());parents.reserve(triangles.size());
    for(int i=0;i<triangles.size();++i){
        const auto&t=triangles[i];
        QVector<QVector<QVector3D>> polygons{{t.a,t.b,t.c}};
        for(const auto&cut:cuts[i]){
            if(timer.elapsed()>10000){result.bounded=false;break;}
            QVector<QVector<QVector3D>> next;
            for(const auto&polygon:polygons){
                double minimum=1e100,maximum=-1e100;
                for(const auto&p:polygon){const double d=signedDistance(cut,p);minimum=std::min(minimum,d);maximum=std::max(maximum,d);}
                if(minimum>=-epsilon||maximum<=epsilon){next.push_back(polygon);continue;}
                const auto positive=clipPolygon(polygon,cut,epsilon,true);
                const auto negative=clipPolygon(polygon,cut,epsilon,false);
                if(polygonArea(positive)>epsilon*epsilon&&polygonArea(negative)>epsilon*epsilon){
                    next.push_back(positive);next.push_back(negative);
                }else next.push_back(polygon);
            }
            polygons=std::move(next);
            if(polygons.size()+fragments.size()>MaxFragments){result.bounded=false;break;}
        }
        if(!result.bounded)break;
        double area=0.0;
        for(const auto&polygon:polygons)area+=polygonArea(polygon);
        const double originalArea=polygonArea({t.a,t.b,t.c});
        if(std::abs(area-originalArea)>std::max(1e-5,originalArea*1e-4)){
            result.bounded=false;break;
        }
        for(const auto&polygon:polygons)for(int k=1;k+1<polygon.size();++k){
            if(polygonArea({polygon[0],polygon[k],polygon[k+1]})<=epsilon*epsilon)continue;
            fragments.push_back(triangle(polygon[0],polygon[k],polygon[k+1],t));
            auto surface=source.sourceModel->surfaces[i];surface.triangleIndex=fragments.size()-1;
            surfaces.push_back(surface);parents.push_back(i);
        }
        if(fragments.size()>MaxFragments){result.bounded=false;break;}
    }
    if(!result.bounded){
        result.diagnostic=QStringLiteral("Intersection arrangement fragment/area limit exceeded; no source was changed.");
        return result;
    }
    // Match new vertices lying strictly on another authored fragment edge.
    // This is a conforming subdivision of existing edges, not a proximity join
    // or an assertion that either side bounds material.
    std::map<Key,QVector3D> uniquePoints;
    for(int i=0;i<fragments.size();++i){
        const auto&original=triangles[parents[i]];
        for(const auto&p:{fragments[i].a,fragments[i].b,fragments[i].c}){
            if(length(p-original.a)<=epsilon||length(p-original.b)<=epsilon||
               length(p-original.c)<=epsilon)continue;
            uniquePoints.emplace(key(p,epsilon),p);
        }
    }
    QVector<Triangle> conformed;QVector<SurfaceRecord> conformingSurfaces;QVector<int> conformingParents;
    QString conformanceLimit;
    conformed.reserve(fragments.size());
    for(int i=0;i<fragments.size();++i){
        if(timer.elapsed()>10000){result.bounded=false;conformanceLimit=QStringLiteral("elapsed");break;}
        const auto&t=fragments[i];const std::array<QVector3D,3>vertices{t.a,t.b,t.c};
        if(planes[parents[i]].normal.isNull()){
            conformed.push_back(t);auto surface=surfaces[i];surface.triangleIndex=conformed.size()-1;
            conformingSurfaces.push_back(surface);conformingParents.push_back(parents[i]);continue;
        }
        QVector<QVector3D> boundary;
        for(int edge=0;edge<3;++edge){
            const auto a=vertices[edge],b=vertices[(edge+1)%3];
            boundary.push_back(a);
            QVector<std::pair<double,QVector3D>> interior;
            const double edgeLength=length(b-a);
            if(edgeLength<=epsilon)continue;
            for(const auto&item:uniquePoints){
                const auto&p=item.second;
                if(std::abs(double(p.x()-a.x()))>edgeLength+epsilon||
                   std::abs(double(p.y()-a.y()))>edgeLength+epsilon||
                   std::abs(double(p.z()-a.z()))>edgeLength+epsilon)continue;
                double position=0.0;
                if(distanceToLine(p,a,b,&position)>epsilon||
                   position<=epsilon/edgeLength||position>=1.0-epsilon/edgeLength)continue;
                interior.push_back({position,a+float(position)*(b-a)});
            }
            std::sort(interior.begin(),interior.end(),[](const auto&x,const auto&y){return x.first<y.first;});
            double previous=-1.0;
            for(const auto&point:interior)
                if(point.first-previous>epsilon/edgeLength){boundary.push_back(point.second);previous=point.first;}
        }
        if(!result.bounded)break;
        if(boundary.size()==3){conformed.push_back(t);auto surface=surfaces[i];surface.triangleIndex=conformed.size()-1;
            conformingSurfaces.push_back(surface);conformingParents.push_back(parents[i]);continue;}
        const auto centroid=(t.a+t.b+t.c)/3.0f;
        for(int k=0;k<boundary.size();++k){
            const auto next=boundary[(k+1)%boundary.size()];
            if(polygonArea({centroid,boundary[k],next})<=epsilon*epsilon)continue;
            conformed.push_back(triangle(centroid,boundary[k],next,t));
            auto surface=surfaces[i];surface.triangleIndex=conformed.size()-1;
            conformingSurfaces.push_back(surface);conformingParents.push_back(parents[i]);
        }
        if(conformed.size()>MaxFragments){result.bounded=false;conformanceLimit=QStringLiteral("fragment count");break;}
    }
    if(!result.bounded){
        result.diagnostic=QStringLiteral("Intersection arrangement edge conformance rejected (%1; points=%2, fragments=%3); no source was changed.")
            .arg(conformanceLimit).arg(uniquePoints.size()).arg(conformed.size());
        return result;
    }
    fragments=std::move(conformed);surfaces=std::move(conformingSurfaces);parents=std::move(conformingParents);
    result.fragmentsAfter=fragments.size();result.changed=fragments.size()!=triangles.size();
    if(result.changed){
        result.loadResult.mesh.triangles=std::move(fragments);
        result.loadResult.sourceModel=std::make_shared<LDrawGeometry::LDrawSourceModel>(*source.sourceModel);
        result.loadResult.sourceModel->surfaces=std::move(surfaces);
        result.stitchedTriangleForFragment=std::move(parents);
    }
    result.diagnostic=QStringLiteral("Source intersection arrangement: candidatePairs=%1 transverse=%2 coplanarOverlaps=%3 fragments=%4->%5; no closure inferred.")
        .arg(result.candidatePairs).arg(result.transversePairs).arg(result.coplanarOverlapPairs)
        .arg(result.fragmentsBefore).arg(result.fragmentsAfter);
    return result;
}

OrientedSurfaceArrangement LDrawCertifiedInterfaceStitcher::classifyOrientedFragments(
    const SurfaceIntersectionArrangement& arranged)
{
    OrientedSurfaceArrangement result;
    result.loadResult=arranged.loadResult;
    const auto& triangles=arranged.loadResult.mesh.triangles;
    result.fragmentsBefore=triangles.size();
    result.fragmentsAfter=triangles.size();
    for(int i=0;i<arranged.stitchedTriangleForFragment.size();++i){
        result.stitchedTrianglesForFragment.push_back({arranged.stitchedTriangleForFragment[i]});
        result.arrangedFragmentsForFragment.push_back({i});
    }
    if(!arranged.bounded||!arranged.loadResult.sourceModel||
       arranged.loadResult.sourceModel->surfaces.size()!=triangles.size()||
       arranged.stitchedTriangleForFragment.size()!=triangles.size()||triangles.size()>200000){
        result.bounded=false;
        result.diagnostic=QStringLiteral("Oriented arrangement requires bounded fragments and complete provenance.");
        return result;
    }
    QElapsedTimer timer;timer.start();
    using PointKey=std::array<float,3>;
    using TriangleKey=std::tuple<QString,std::array<PointKey,3>>;
    struct Group {int first=-1,opposite=-1;bool firstOdd=false,counted=false;};
    std::map<TriangleKey,Group> groups;
    QVector<Triangle> retained;
    QVector<SurfaceRecord> surfaces;
    QVector<QVector<int>> ancestry;
    QVector<QVector<int>> arrangedAncestry;
    retained.reserve(triangles.size());surfaces.reserve(triangles.size());ancestry.reserve(triangles.size());
    arrangedAncestry.reserve(triangles.size());
    for(int i=0;i<triangles.size();++i){
        if((i&255)==0&&timer.elapsed()>10000){result.bounded=false;break;}
        const auto& triangle=triangles[i];
        const auto& surface=arranged.loadResult.sourceModel->surfaces[i];
        const QVector3D normal=QVector3D::crossProduct(triangle.b-triangle.a,triangle.c-triangle.a);
        const bool oriented=surface.certified&&surface.clipping&&triangle.backFaceCull&&
            normal.lengthSquared()>1e-20f;
        int representative=-1;
        if(oriented){
            std::array<PointKey,3> points{{{triangle.a.x(),triangle.a.y(),triangle.a.z()},
                                           {triangle.b.x(),triangle.b.y(),triangle.b.z()},
                                           {triangle.c.x(),triangle.c.y(),triangle.c.z()}}};
            const bool odd=(points[1]<points[0])!=(points[2]<points[0])!=(points[2]<points[1]);
            std::sort(points.begin(),points.end());
            auto [it,inserted]=groups.emplace(TriangleKey{triangle.color,points},Group{});
            auto& group=it->second;
            if(!inserted){
                if(!group.counted){++result.exactCoincidentGroups;group.counted=true;}
                if(odd==group.firstOdd)representative=group.first;
                else{
                    if(group.opposite<0){group.opposite=retained.size();++result.opposingCoincidentGroups;}
                    else representative=group.opposite;
                }
            }else{group.first=retained.size();group.firstOdd=odd;}
        }
        if(representative>=0){
            ancestry[representative].push_back(arranged.stitchedTriangleForFragment[i]);
            arrangedAncestry[representative].push_back(i);
            ++result.sameFacingDuplicates;
            continue;
        }
        auto copy=surface;copy.triangleIndex=retained.size();
        retained.push_back(triangle);surfaces.push_back(copy);
        ancestry.push_back({arranged.stitchedTriangleForFragment[i]});
        arrangedAncestry.push_back({i});
    }
    result.elapsedMilliseconds=timer.elapsed();
    if(!result.bounded){
        result.diagnostic=QStringLiteral("Oriented arrangement workload limit exceeded; no fragment was changed.");
        return result;
    }
    result.fragmentsAfter=retained.size();
    result.stitchedTrianglesForFragment=std::move(ancestry);
    result.arrangedFragmentsForFragment=std::move(arrangedAncestry);
    if(result.sameFacingDuplicates){
        result.loadResult.mesh.triangles=std::move(retained);
        result.loadResult.sourceModel=
            std::make_shared<LDrawGeometry::LDrawSourceModel>(*arranged.loadResult.sourceModel);
        result.loadResult.sourceModel->surfaces=std::move(surfaces);
    }
    result.diagnostic=QStringLiteral("Oriented source fragments: exactCoincidentGroups=%1 sameFacingDuplicates=%2 opposingCoincidentGroups=%3 retained=%4/%5 elapsed-ms=%6; opposing/intersecting patches remain unresolved without material-side proof.")
        .arg(result.exactCoincidentGroups).arg(result.sameFacingDuplicates)
        .arg(result.opposingCoincidentGroups).arg(result.fragmentsAfter)
        .arg(result.fragmentsBefore).arg(result.elapsedMilliseconds);
    return result;
}

MaterialCellArrangement LDrawCertifiedInterfaceStitcher::proveMaterialCells(
    const OrientedSurfaceArrangement& oriented)
{
    MaterialCellArrangement result;
    const auto& triangles=oriented.loadResult.mesh.triangles;
    result.inputFragments=triangles.size();
    result.unresolvedFragments=triangles.size();
    result.provenBoundary.fill(false,triangles.size());
    result.arrangedFragmentStatus.fill(MaterialCellArrangement::FragmentStatus::Unresolved,
                                       oriented.fragmentsBefore);
    constexpr int MaxFragments=10000,MaxAdjacencies=30000,MaxCells=10000;
    constexpr qint64 MaxMilliseconds=10000;
    if(!oriented.bounded||!oriented.loadResult.sourceModel||
       oriented.loadResult.sourceModel->surfaces.size()!=triangles.size()||
       oriented.stitchedTrianglesForFragment.size()!=triangles.size()||
       oriented.arrangedFragmentsForFragment.size()!=triangles.size()||
       triangles.size()>MaxFragments){
        result.bounded=false;
        result.diagnostic=QStringLiteral("Material-cell proof requires bounded certified fragments and complete provenance.");
        return result;
    }
    QElapsedTimer timer;timer.start();
    using PointKey=std::array<float,3>;
    using ExactEdge=std::pair<PointKey,PointKey>;
    struct Incidence {int fragment;bool forward;};
    auto point=[](const QVector3D& p){return PointKey{p.x(),p.y(),p.z()};};
    std::map<ExactEdge,QVector<Incidence>> edges;
    QVector<bool> certified(triangles.size(),false);
    QVector<bool> accounted(oriented.fragmentsBefore,false);
    for(const auto& descendants:oriented.arrangedFragmentsForFragment){
        if(descendants.isEmpty()){result.bounded=false;break;}
        for(int i=0;i<descendants.size();++i){
            const int index=descendants[i];
            if(index<0||index>=result.arrangedFragmentStatus.size()||accounted[index]){
                result.bounded=false;break;
            }
            accounted[index]=true;
            if(i>0){
                result.arrangedFragmentStatus[index]=MaterialCellArrangement::FragmentStatus::ExplicitDuplicate;
                ++result.explicitDuplicateFragments;
            }
        }
        if(!result.bounded)break;
    }
    if(std::any_of(accounted.cbegin(),accounted.cend(),[](bool value){return !value;}))
        result.bounded=false;
    if(!result.bounded){
        result.diagnostic=QStringLiteral("Material-cell proof duplicate provenance is incomplete.");
        return result;
    }
    for(int i=0;i<triangles.size();++i){
        if((i&255)==0&&timer.elapsed()>MaxMilliseconds){result.bounded=false;break;}
        const auto& t=triangles[i];
        const auto& s=oriented.loadResult.sourceModel->surfaces[i];
        certified[i]=s.certified&&s.clipping&&t.backFaceCull&&
            !oriented.stitchedTrianglesForFragment[i].isEmpty();
        const std::array<PointKey,3> vertices{point(t.a),point(t.b),point(t.c)};
        for(int k=0;k<3;++k){
            auto a=vertices[k],b=vertices[(k+1)%3];
            if(a==b){certified[i]=false;continue;}
            const bool forward=a<b;
            if(!forward)std::swap(a,b);
            edges[{a,b}].push_back({i,forward});
        }
        if(edges.size()>MaxAdjacencies){result.bounded=false;break;}
    }
    if(!result.bounded){
        result.diagnostic=QStringLiteral("Material-cell proof edge/workload limit exceeded; no cell accepted.");
        return result;
    }
    QVector<QVector<int>> neighbors(triangles.size());
    QVector<bool> unresolvedEdge(triangles.size(),false);
    for(const auto& item:edges){
        const auto& uses=item.second;
        if(uses.size()==2&&uses[0].forward!=uses[1].forward&&
           uses[0].fragment!=uses[1].fragment&&
           certified[uses[0].fragment]&&certified[uses[1].fragment]){
            neighbors[uses[0].fragment].push_back(uses[1].fragment);
            neighbors[uses[1].fragment].push_back(uses[0].fragment);
            ++result.exactAdjacencies;
        }else{
            if(uses.size()!=1)++result.ambiguousEdges;
            else ++result.residualBoundaryEdges;
            for(const auto& use:uses)unresolvedEdge[use.fragment]=true;
        }
    }
    QVector<bool> visited(triangles.size(),false);
    for(int start=0;start<triangles.size();++start){
        if(visited[start])continue;
        if(result.candidateCells>=MaxCells||timer.elapsed()>MaxMilliseconds){result.bounded=false;break;}
        QVector<int> component{start};visited[start]=true;
        for(int cursor=0;cursor<component.size();++cursor)
            for(int next:neighbors[component[cursor]])if(!visited[next]){
                visited[next]=true;component.push_back(next);
            }
        ++result.candidateCells;
        // A partial closed shell may be intersected or partitioned by other
        // authored fragments. Until adjacent spatial cells are proven, it is
        // only a candidate, not a material cell.
        bool closed=component.size()>=4&&component.size()==triangles.size();
        for(int index:component)closed=closed&&certified[index]&&!unresolvedEdge[index];
        if(!closed)continue;
        PrintMesh shell;
        std::map<PointKey,std::uint32_t> vertices;
        for(int index:component){
            const auto& t=triangles[index];
            const std::array<PointKey,3> points{point(t.a),point(t.b),point(t.c)};
            Face face{};
            for(int k=0;k<3;++k){
                const auto [it,inserted]=vertices.emplace(points[k],std::uint32_t(shell.vertices.size()));
                if(inserted)shell.vertices.push_back({points[k][0],points[k][1],points[k][2]});
                face[k]=it->second;
            }
            shell.faces.push_back(face);
        }
        // Exact edge closure is necessary but not sufficient: the ordinary
        // strict analyzer remains authoritative for manifoldness and crossings.
        const auto analysis=analyzeSource(shell);
        if(!validatePreparedMesh(analysis).ok()||analysis.signedVolume<=0.0)continue;
        ++result.provenCells;
        for(int index:component){
            result.provenBoundary[index]=true;++result.provenBoundaryFragments;
            result.arrangedFragmentStatus[oriented.arrangedFragmentsForFragment[index].front()]=
                MaterialCellArrangement::FragmentStatus::ProvenBoundary;
        }
    }
    result.elapsedMilliseconds=timer.elapsed();
    if(!result.bounded){
        result.provenBoundary.fill(false,triangles.size());
        result.arrangedFragmentStatus.fill(MaterialCellArrangement::FragmentStatus::Unresolved,
                                           oriented.fragmentsBefore);
        result.provenCells=0;result.provenBoundaryFragments=0;
        result.diagnostic=QStringLiteral("Material-cell proof cell/time limit exceeded; no cell accepted.");
        return result;
    }
    result.unresolvedFragments=triangles.size()-result.provenBoundaryFragments;
    result.diagnostic=QStringLiteral("Source material-cell proof: fragments=%1 exactAdjacencies=%2 ambiguousEdges=%3 candidateCells=%4 provenCells=%5 provenBoundaryFragments=%6 explicitDuplicates=%7 unresolvedFragments=%8 residualBoundaryEdges=%9 elapsed-ms=%10; no cap or bridge inferred.")
        .arg(result.inputFragments).arg(result.exactAdjacencies).arg(result.ambiguousEdges)
        .arg(result.candidateCells).arg(result.provenCells).arg(result.provenBoundaryFragments)
        .arg(result.explicitDuplicateFragments).arg(result.unresolvedFragments)
        .arg(result.residualBoundaryEdges).arg(result.elapsedMilliseconds);
    return result;
}

} // namespace PrintGeometry
