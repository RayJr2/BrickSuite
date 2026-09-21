#include "SourceSurfaceSolidifier.h"

#include "PrintMeshAnalysis.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <queue>
#include <tuple>
#include <vector>

namespace PrintGeometry {
namespace {

struct Crossing { double x=0.0; int direction=0; };
struct GridEdge {
    std::uint64_t a=0,b=0;
    bool operator<(const GridEdge&other)const{return std::tie(a,b)<std::tie(other.a,other.b);}
};

Point converted(const QVector3D&p){return {0.4*double(p.x()),0.4*double(p.z()),-0.4*double(p.y())};}
Point subtract(const Point&a,const Point&b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
Point cross(const Point&a,const Point&b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
double dot(const Point&a,const Point&b){return a.x*b.x+a.y*b.y+a.z*b.z;}
Point add(const Point&a,const Point&b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
Point multiply(const Point&a,double value){return {a.x*value,a.y*value,a.z*value};}
double squaredDistance(const Point&a,const Point&b){const auto d=subtract(a,b);return dot(d,d);}

Point closestPointOnTriangle(const Point&p,const Point&a,const Point&b,const Point&c)
{
    const auto ab=subtract(b,a),ac=subtract(c,a),ap=subtract(p,a);const double d1=dot(ab,ap),d2=dot(ac,ap);if(d1<=0.0&&d2<=0.0)return a;
    const auto bp=subtract(p,b);const double d3=dot(ab,bp),d4=dot(ac,bp);if(d3>=0.0&&d4<=d3)return b;
    const double vc=d1*d4-d3*d2;if(vc<=0.0&&d1>=0.0&&d3<=0.0)return add(a,multiply(ab,d1/(d1-d3)));
    const auto cp=subtract(p,c);const double d5=dot(ab,cp),d6=dot(ac,cp);if(d6>=0.0&&d5<=d6)return c;
    const double vb=d5*d2-d1*d6;if(vb<=0.0&&d2>=0.0&&d6<=0.0)return add(a,multiply(ac,d2/(d2-d6)));
    const double va=d3*d6-d5*d4;if(va<=0.0&&(d4-d3)>=0.0&&(d5-d6)>=0.0){const auto bc=subtract(c,b);return add(b,multiply(bc,(d4-d3)/((d4-d3)+(d5-d6))));}
    const double denominator=1.0/(va+vb+vc),v=vb*denominator,w=vc*denominator;return add(a,add(multiply(ab,v),multiply(ac,w)));
}

bool yzIntersection(const Point&a,const Point&b,const Point&c,double y,double z,double*x)
{
    const double denominator=(b.z-c.z)*(a.y-c.y)+(c.y-b.y)*(a.z-c.z);
    if(std::abs(denominator)<1e-14)return false;
    const double u=((b.z-c.z)*(y-c.y)+(c.y-b.y)*(z-c.z))/denominator;
    const double v=((c.z-a.z)*(y-c.y)+(a.y-c.y)*(z-c.z))/denominator;
    const double w=1.0-u-v;
    constexpr double epsilon=1e-10;
    if(u<-epsilon||v<-epsilon||w<-epsilon)return false;
    *x=u*a.x+v*b.x+w*c.x;return true;
}

void normalize(PrintMesh*mesh)
{
    if(analyzeSource(*mesh).signedVolume<0.0)for(auto&face:mesh->faces)std::swap(face[1],face[2]);
}

void orientConsistently(PrintMesh*mesh)
{
    using Edge=std::pair<std::uint32_t,std::uint32_t>;struct Use{int face=0;bool forward=false;};std::map<Edge,std::vector<Use>>uses;
    for(int fi=0;fi<int(mesh->faces.size());++fi){const auto&f=mesh->faces[fi];for(int i=0;i<3;++i){const auto a=f[i],b=f[(i+1)%3];uses[{std::min(a,b),std::max(a,b)}].push_back({fi,a<b});}}
    std::vector<bool>visited(mesh->faces.size()),flipped(mesh->faces.size());
    for(int root=0;root<int(mesh->faces.size());++root)if(!visited[root]){std::queue<int>pending;pending.push(root);visited[root]=true;while(!pending.empty()){const int face=pending.front();pending.pop();const auto f=mesh->faces[face];for(int i=0;i<3;++i){const auto a=f[i],b=f[(i+1)%3];const Edge key{std::min(a,b),std::max(a,b)};const bool currentForward=(a<b)!=flipped[face];for(const auto&use:uses[key])if(use.face!=face){const bool requiredFlip=currentForward==use.forward;if(!visited[use.face]){visited[use.face]=true;flipped[use.face]=requiredFlip;pending.push(use.face);}}}}}
    for(std::size_t i=0;i<mesh->faces.size();++i)if(flipped[i])std::swap(mesh->faces[i][1],mesh->faces[i][2]);normalize(mesh);
}

} // namespace

SourceSurfaceSolidificationResult SourceSurfaceSolidifier::solidify(
    const LDrawGeometry::LDrawLoadResult&source,double pitch)
{
    SourceSurfaceSolidificationResult result;result.samplingPitchMillimetres=pitch;
    if(!source.ok()||!source.sourceModel||source.sourceModel->surfaces.size()!=source.mesh.triangles.size()||pitch<0.025||pitch>0.25){result.diagnostic=QStringLiteral("Complete authoritative triangle provenance and a supported sampling pitch are required.");return result;}
    for(const auto&surface:source.sourceModel->surfaces){if(!surface.certified||!surface.clipping||surface.fileId<0||surface.fileId>=source.sourceModel->files.size()||source.sourceModel->files[surface.fileId].classification==LDrawGeometry::SourceClassification::Unknown){result.diagnostic=QStringLiteral("Source-surface solidification accepts only certified clipping geometry from classified LDraw files.");return result;}}

    struct Triangle {Point a,b,c,normal;};std::vector<Triangle>triangles;triangles.reserve(source.mesh.triangles.size());MeshBounds sourceBounds;
    auto include=[&](const Point&p){if(!sourceBounds.valid){sourceBounds.minimum=sourceBounds.maximum=p;sourceBounds.valid=true;}else{sourceBounds.minimum.x=std::min(sourceBounds.minimum.x,p.x);sourceBounds.minimum.y=std::min(sourceBounds.minimum.y,p.y);sourceBounds.minimum.z=std::min(sourceBounds.minimum.z,p.z);sourceBounds.maximum.x=std::max(sourceBounds.maximum.x,p.x);sourceBounds.maximum.y=std::max(sourceBounds.maximum.y,p.y);sourceBounds.maximum.z=std::max(sourceBounds.maximum.z,p.z);}};
    for(const auto&t:source.mesh.triangles){Triangle convertedTriangle{converted(t.a),converted(t.b),converted(t.c),{}};convertedTriangle.normal=cross(subtract(convertedTriangle.b,convertedTriangle.a),subtract(convertedTriangle.c,convertedTriangle.a));if(dot(convertedTriangle.normal,convertedTriangle.normal)<1e-20)continue;include(convertedTriangle.a);include(convertedTriangle.b);include(convertedTriangle.c);triangles.push_back(convertedTriangle);}
    if(triangles.empty()||!sourceBounds.valid){result.diagnostic=QStringLiteral("The authoritative triangle soup is empty.");return result;}
    const auto extent=[](double low,double high,double step){return int(std::ceil((high-low)/step))+5;};
    const Point origin{sourceBounds.minimum.x-2.0*pitch,sourceBounds.minimum.y-2.0*pitch,sourceBounds.minimum.z-2.0*pitch};
    const int nx=extent(sourceBounds.minimum.x,sourceBounds.maximum.x,pitch),ny=extent(sourceBounds.minimum.y,sourceBounds.maximum.y,pitch),nz=extent(sourceBounds.minimum.z,sourceBounds.maximum.z,pitch);
    const std::uint64_t nodeCount=std::uint64_t(nx)*std::uint64_t(ny)*std::uint64_t(nz);if(nodeCount>30000000ull){result.diagnostic=QStringLiteral("The source exceeds the bounded solidification grid.");return result;}
    const auto node=[&](int x,int y,int z){return std::uint64_t(x)+std::uint64_t(nx)*(std::uint64_t(y)+std::uint64_t(ny)*std::uint64_t(z));};
    std::vector<std::uint8_t>inside;inside.resize(std::size_t(nodeCount));std::vector<Crossing>crossings;crossings.reserve(triangles.size());
    for(int z=0;z<nz;++z)for(int y=0;y<ny;++y){const double sampleY=origin.y+pitch*double(y)+pitch*1.0e-6,sampleZ=origin.z+pitch*double(z)+pitch*2.41421356237e-6;crossings.clear();for(const auto&t:triangles){double x=0.0;if(yzIntersection(t.a,t.b,t.c,sampleY,sampleZ,&x))crossings.push_back({x,t.normal.x>0.0?1:-1});}std::sort(crossings.begin(),crossings.end(),[](const auto&a,const auto&b){return a.x<b.x;});int winding=0;std::size_t crossing=0;for(int x=0;x<nx;++x){const double sampleX=origin.x+pitch*double(x);while(crossing<crossings.size()&&crossings[crossing].x<sampleX){const double at=crossings[crossing].x;int direction=0;while(crossing<crossings.size()&&std::abs(crossings[crossing].x-at)<1e-8){direction+=crossings[crossing].direction;++crossing;}winding+=direction;}inside[std::size_t(node(x,y,z))]=winding!=0;}}

    PrintMesh mesh;std::map<GridEdge,std::uint32_t>edgeVertices;
    const auto point=[&](std::uint64_t id){const int x=int(id%std::uint64_t(nx));id/=std::uint64_t(nx);const int y=int(id%std::uint64_t(ny)),z=int(id/std::uint64_t(ny));return Point{origin.x+pitch*double(x),origin.y+pitch*double(y),origin.z+pitch*double(z)};};
    const auto midpoint=[&](std::uint64_t a,std::uint64_t b){GridEdge key{std::min(a,b),std::max(a,b)};const auto found=edgeVertices.find(key);if(found!=edgeVertices.end())return found->second;const auto pa=point(a),pb=point(b);const auto id=std::uint32_t(mesh.vertices.size());mesh.vertices.push_back({(pa.x+pb.x)*.5,(pa.y+pb.y)*.5,(pa.z+pb.z)*.5});edgeVertices.emplace(key,id);return id;};
    constexpr int corner[8][3]={{0,0,0},{1,0,0},{1,1,0},{0,1,0},{0,0,1},{1,0,1},{1,1,1},{0,1,1}};
    constexpr int tetrahedra[6][4]={{0,1,2,6},{0,2,3,6},{0,3,7,6},{0,7,4,6},{0,4,5,6},{0,5,1,6}};
    for(int z=0;z+1<nz;++z)for(int y=0;y+1<ny;++y)for(int x=0;x+1<nx;++x){std::uint64_t ids[8];for(int i=0;i<8;++i)ids[i]=node(x+corner[i][0],y+corner[i][1],z+corner[i][2]);for(const auto&tetra:tetrahedra){int occupied[4],empty[4],occupiedCount=0,emptyCount=0;for(int i=0;i<4;++i)(inside[std::size_t(ids[tetra[i]])]?occupied[occupiedCount++]:empty[emptyCount++])=tetra[i];if(occupiedCount==0||occupiedCount==4)continue;if(occupiedCount==1||occupiedCount==3){const bool invert=occupiedCount==3;const int isolated=invert?empty[0]:occupied[0];const int*a=invert?occupied:empty;const auto p0=midpoint(ids[isolated],ids[a[0]]),p1=midpoint(ids[isolated],ids[a[1]]),p2=midpoint(ids[isolated],ids[a[2]]);mesh.faces.push_back(invert?Face{p0,p2,p1}:Face{p0,p1,p2});}else{const auto ac=midpoint(ids[occupied[0]],ids[empty[0]]),ad=midpoint(ids[occupied[0]],ids[empty[1]]),bc=midpoint(ids[occupied[1]],ids[empty[0]]),bd=midpoint(ids[occupied[1]],ids[empty[1]]);mesh.faces.push_back({ac,bc,ad});mesh.faces.push_back({ad,bc,bd});}}}
    if(mesh.faces.empty()){result.diagnostic=QStringLiteral("The source did not define a stable occupied volume.");return result;}orientConsistently(&mesh);
    constexpr double projectionFraction=0.95;const double maximumProjectionDistance=1.25*pitch,maximumProjectionDistanceSquared=maximumProjectionDistance*maximumProjectionDistance;double beforeDistance=0.0,afterDistance=0.0;
    for(auto&vertex:mesh.vertices){Point nearest{};double nearestSquared=std::numeric_limits<double>::max();for(const auto&triangle:triangles){const auto candidate=closestPointOnTriangle(vertex,triangle.a,triangle.b,triangle.c);const double candidateSquared=squaredDistance(vertex,candidate);if(candidateSquared<nearestSquared){nearestSquared=candidateSquared;nearest=candidate;}}const double distance=std::sqrt(nearestSquared);beforeDistance+=distance;if(nearestSquared<=maximumProjectionDistanceSquared){vertex=add(vertex,multiply(subtract(nearest,vertex),projectionFraction));++result.projectedVertices;afterDistance+=(1.0-projectionFraction)*distance;}else afterDistance+=distance;}
    result.meanSurfaceDeviationBeforeProjectionMillimetres=beforeDistance/double(mesh.vertices.size());result.meanSurfaceDeviationAfterProjectionMillimetres=afterDistance/double(mesh.vertices.size());orientConsistently(&mesh);
    auto raw=analyzeSource(mesh);if(!raw.bounds.valid){result.diagnostic=QStringLiteral("The extracted solid has invalid bounds.");return result;}
    const Point rawExtent{subtract(raw.bounds.maximum,raw.bounds.minimum)},sourceExtent{subtract(sourceBounds.maximum,sourceBounds.minimum)};if(rawExtent.x<=0||rawExtent.y<=0||rawExtent.z<=0){result.diagnostic=QStringLiteral("The extracted solid is dimensionally degenerate.");return result;}
    for(auto&p:mesh.vertices){p.x=sourceBounds.minimum.x+(p.x-raw.bounds.minimum.x)*sourceExtent.x/rawExtent.x;p.y=sourceBounds.minimum.y+(p.y-raw.bounds.minimum.y)*sourceExtent.y/rawExtent.y;p.z=sourceBounds.minimum.z+(p.z-raw.bounds.minimum.z)*sourceExtent.z/rawExtent.z;}
    orientConsistently(&mesh);result.analysis=analyzeSource(mesh);const auto validation=validateBooleanOperand(result.analysis);if(!validation.ok()){result.diagnostic=QStringLiteral("Solidified source failed independent validation: %1").arg(QString::fromStdString(validation.message));return result;}
    result.mesh=std::move(mesh);result.successful=true;result.diagnostic=QStringLiteral("Certified source surfaces solidified on a %1 mm deterministic occupancy grid; %2 vertices projected to authoritative surfaces (%3 mm mean deviation before, %4 mm after).").arg(pitch,0,'f',3).arg(result.projectedVertices).arg(result.meanSurfaceDeviationBeforeProjectionMillimetres,0,'f',4).arg(result.meanSurfaceDeviationAfterProjectionMillimetres,0,'f',4);return result;
}

} // namespace PrintGeometry
