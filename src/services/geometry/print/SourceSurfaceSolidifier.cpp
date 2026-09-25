#include "SourceSurfaceSolidifier.h"
#include "SourceSurfaceQueryIndex.h"

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
#include <QElapsedTimer>

namespace PrintGeometry {
namespace {

struct Crossing { double x=0.0; int direction=0; };
struct GridEdge {
    std::uint64_t a=0,b=0;
    bool operator<(const GridEdge&other)const{return std::tie(a,b)<std::tie(other.a,other.b);}
};

Point converted(const QVector3D&p){return {0.4*double(p.x()),0.4*double(p.z()),-0.4*double(p.y())};}
Point rayCoordinates(const Point&p,int axis){return axis==1?Point{p.y,p.z,p.x}:axis==2?Point{p.z,p.x,p.y}:p;}
Point modelCoordinates(const Point&p,int axis){return axis==1?Point{p.z,p.x,p.y}:axis==2?Point{p.y,p.z,p.x}:p;}
Point subtract(const Point&a,const Point&b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
Point cross(const Point&a,const Point&b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
double dot(const Point&a,const Point&b){return a.x*b.x+a.y*b.y+a.z*b.z;}
Point add(const Point&a,const Point&b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
Point multiply(const Point&a,double value){return {a.x*value,a.y*value,a.z*value};}

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

double signedVolume(const PrintMesh&mesh);
void normalize(PrintMesh*mesh)
{
    if(signedVolume(*mesh)<0.0)for(auto&face:mesh->faces)std::swap(face[1],face[2]);
}

struct FixedTopology { bool closed=true; int components=0; };
FixedTopology orientConsistently(PrintMesh*mesh)
{
    using Edge=std::pair<std::uint32_t,std::uint32_t>;struct Use{int face=0;bool forward=false;};std::map<Edge,std::vector<Use>>uses;
    for(int fi=0;fi<int(mesh->faces.size());++fi){const auto&f=mesh->faces[fi];for(int i=0;i<3;++i){const auto a=f[i],b=f[(i+1)%3];uses[{std::min(a,b),std::max(a,b)}].push_back({fi,a<b});}}
    std::vector<bool>visited(mesh->faces.size()),flipped(mesh->faces.size());
    int components=0;
    for(int root=0;root<int(mesh->faces.size());++root)if(!visited[root]){++components;std::queue<int>pending;pending.push(root);visited[root]=true;while(!pending.empty()){const int face=pending.front();pending.pop();const auto f=mesh->faces[face];for(int i=0;i<3;++i){const auto a=f[i],b=f[(i+1)%3];const Edge key{std::min(a,b),std::max(a,b)};const bool currentForward=(a<b)!=flipped[face];for(const auto&use:uses[key])if(use.face!=face){const bool requiredFlip=currentForward==use.forward;if(!visited[use.face]){visited[use.face]=true;flipped[use.face]=requiredFlip;pending.push(use.face);}}}}}
    bool hasBoundary=false;
    for(const auto&entry:uses)if(entry.second.size()==1){hasBoundary=true;break;}
    for(std::size_t i=0;i<mesh->faces.size();++i)if(flipped[i])std::swap(mesh->faces[i][1],mesh->faces[i][2]);normalize(mesh);
    return {!hasBoundary,components};
}

// Projection and positive axis-aligned bounds restoration move vertices but do
// not change the already-oriented face adjacency. Rebuilding the edge graph for
// each fraction cannot change local winding; only the global volume sign can.
double signedVolume(const PrintMesh&mesh)
{
    double volume=0.0;
    for(const auto&face:mesh.faces){const auto&a=mesh.vertices[face[0]],&b=mesh.vertices[face[1]],&c=mesh.vertices[face[2]];
        volume+=(a.x*(b.y*c.z-b.z*c.y)-a.y*(b.x*c.z-b.z*c.x)+a.z*(b.x*c.y-b.y*c.x))/6.0;}
    return volume;
}
MeshBounds vertexBounds(const PrintMesh&mesh)
{
    MeshBounds bounds;
    for(const auto&p:mesh.vertices){if(!bounds.valid){bounds.minimum=bounds.maximum=p;bounds.valid=true;}
        else{bounds.minimum.x=std::min(bounds.minimum.x,p.x);bounds.minimum.y=std::min(bounds.minimum.y,p.y);bounds.minimum.z=std::min(bounds.minimum.z,p.z);
             bounds.maximum.x=std::max(bounds.maximum.x,p.x);bounds.maximum.y=std::max(bounds.maximum.y,p.y);bounds.maximum.z=std::max(bounds.maximum.z,p.z);}}
    return bounds;
}
void normalizeExistingTopology(PrintMesh*mesh)
{
    if(signedVolume(*mesh)<0.0)for(auto&face:mesh->faces)std::swap(face[1],face[2]);
}

} // namespace

SourceSurfaceSolidificationResult SourceSurfaceSolidifier::solidify(
    const LDrawGeometry::LDrawLoadResult&source,double pitch,RayAxis rayAxis,QueryMode queryMode)
{
    QElapsedTimer total,phase;total.start();phase.start();
    SourceSurfaceSolidificationResult result;result.samplingPitchMillimetres=pitch;
    const int axis=int(rayAxis);
    result.metrics.rayAxis=axis;
    auto finish=[&]() -> SourceSurfaceSolidificationResult {result.metrics.totalMilliseconds=total.elapsed();result.metrics.outcome=result.diagnostic;return std::move(result);};
    if(!source.ok()||!source.sourceModel||source.sourceModel->surfaces.size()!=source.mesh.triangles.size()||pitch<0.025||pitch>0.25||axis<0||axis>2){result.diagnostic=QStringLiteral("Complete authoritative triangle provenance, a supported sampling pitch and a valid ray axis are required.");return finish();}
    for(const auto&surface:source.sourceModel->surfaces){if(!surface.certified||!surface.clipping||surface.fileId<0||surface.fileId>=source.sourceModel->files.size()||source.sourceModel->files[surface.fileId].classification==LDrawGeometry::SourceClassification::Unknown){result.diagnostic=QStringLiteral("Source-surface solidification accepts only certified clipping geometry from classified LDraw files.");return finish();}}

    struct Triangle {Point a,b,c,normal;};std::vector<Triangle>triangles;triangles.reserve(source.mesh.triangles.size());MeshBounds sourceBounds;
    auto include=[&](const Point&p){if(!sourceBounds.valid){sourceBounds.minimum=sourceBounds.maximum=p;sourceBounds.valid=true;}else{sourceBounds.minimum.x=std::min(sourceBounds.minimum.x,p.x);sourceBounds.minimum.y=std::min(sourceBounds.minimum.y,p.y);sourceBounds.minimum.z=std::min(sourceBounds.minimum.z,p.z);sourceBounds.maximum.x=std::max(sourceBounds.maximum.x,p.x);sourceBounds.maximum.y=std::max(sourceBounds.maximum.y,p.y);sourceBounds.maximum.z=std::max(sourceBounds.maximum.z,p.z);}};
    for(const auto&t:source.mesh.triangles){Triangle convertedTriangle{rayCoordinates(converted(t.a),axis),rayCoordinates(converted(t.b),axis),rayCoordinates(converted(t.c),axis),{}};convertedTriangle.normal=cross(subtract(convertedTriangle.b,convertedTriangle.a),subtract(convertedTriangle.c,convertedTriangle.a));if(dot(convertedTriangle.normal,convertedTriangle.normal)<1e-20)continue;include(convertedTriangle.a);include(convertedTriangle.b);include(convertedTriangle.c);triangles.push_back(convertedTriangle);}
    if(triangles.empty()||!sourceBounds.valid){result.diagnostic=QStringLiteral("The authoritative triangle soup is empty.");return finish();}
    const auto extent=[](double low,double high,double step){return int(std::ceil((high-low)/step))+5;};
    const Point origin{sourceBounds.minimum.x-2.0*pitch,sourceBounds.minimum.y-2.0*pitch,sourceBounds.minimum.z-2.0*pitch};
    const int nx=extent(sourceBounds.minimum.x,sourceBounds.maximum.x,pitch),ny=extent(sourceBounds.minimum.y,sourceBounds.maximum.y,pitch),nz=extent(sourceBounds.minimum.z,sourceBounds.maximum.z,pitch);
    result.metrics.gridX=nx;result.metrics.gridY=ny;result.metrics.gridZ=nz;
    const std::uint64_t nodeCount=std::uint64_t(nx)*std::uint64_t(ny)*std::uint64_t(nz);if(nodeCount>30000000ull){result.diagnostic=QStringLiteral("The source exceeds the bounded solidification grid.");return finish();}
    result.metrics.sourceAnalysisMilliseconds=phase.elapsed();phase.restart();
    std::vector<SourceSurfaceQueryIndex::Triangle> queryTriangles;queryTriangles.reserve(triangles.size());
    for(const auto&triangle:triangles)queryTriangles.push_back({triangle.a,triangle.b,triangle.c});
    const SourceSurfaceQueryIndex index(queryTriangles,origin.y,origin.z,pitch,ny,nz);
    result.metrics.indexMilliseconds=phase.elapsed();phase.restart();
    result.metrics.exhaustiveRayTriangleTests=std::uint64_t(ny)*std::uint64_t(nz)*std::uint64_t(triangles.size());
    const auto node=[&](int x,int y,int z){return std::uint64_t(x)+std::uint64_t(nx)*(std::uint64_t(y)+std::uint64_t(ny)*std::uint64_t(z));};
    std::vector<std::uint8_t>inside;inside.resize(std::size_t(nodeCount));std::vector<Crossing>crossings;crossings.reserve(triangles.size());
    for(int z=0;z<nz;++z)for(int y=0;y<ny;++y){const double sampleY=origin.y+pitch*double(y)+pitch*1.0e-6,sampleZ=origin.z+pitch*double(z)+pitch*2.41421356237e-6;crossings.clear();const auto&candidates=queryMode==QueryMode::Indexed?index.rayCandidates(y,z):index.allTriangles();result.metrics.rayTriangleTests+=candidates.size();for(int triangleIndex:candidates){const auto&t=triangles[std::size_t(triangleIndex)];double x=0.0;if(yzIntersection(t.a,t.b,t.c,sampleY,sampleZ,&x))crossings.push_back({x,t.normal.x>0.0?1:-1});}std::sort(crossings.begin(),crossings.end(),[](const auto&a,const auto&b){return a.x<b.x;});int winding=0;std::size_t crossing=0;for(int x=0;x<nx;++x){const double sampleX=origin.x+pitch*double(x);while(crossing<crossings.size()&&crossings[crossing].x<sampleX){const double at=crossings[crossing].x;int direction=0;while(crossing<crossings.size()&&std::abs(crossings[crossing].x-at)<1e-8){direction+=crossings[crossing].direction;++crossing;}winding+=direction;}inside[std::size_t(node(x,y,z))]=winding!=0;}}
    result.metrics.rayClassificationMilliseconds=phase.elapsed();phase.restart();

    PrintMesh mesh;std::map<GridEdge,std::uint32_t>edgeVertices;
    const auto point=[&](std::uint64_t id){const int x=int(id%std::uint64_t(nx));id/=std::uint64_t(nx);const int y=int(id%std::uint64_t(ny)),z=int(id/std::uint64_t(ny));return Point{origin.x+pitch*double(x),origin.y+pitch*double(y),origin.z+pitch*double(z)};};
    const auto midpoint=[&](std::uint64_t a,std::uint64_t b){GridEdge key{std::min(a,b),std::max(a,b)};const auto found=edgeVertices.find(key);if(found!=edgeVertices.end())return found->second;const auto pa=point(a),pb=point(b);const auto id=std::uint32_t(mesh.vertices.size());mesh.vertices.push_back({(pa.x+pb.x)*.5,(pa.y+pb.y)*.5,(pa.z+pb.z)*.5});edgeVertices.emplace(key,id);return id;};
    constexpr int corner[8][3]={{0,0,0},{1,0,0},{1,1,0},{0,1,0},{0,0,1},{1,0,1},{1,1,1},{0,1,1}};
    constexpr int tetrahedra[6][4]={{0,1,2,6},{0,2,3,6},{0,3,7,6},{0,7,4,6},{0,4,5,6},{0,5,1,6}};
    for(int z=0;z+1<nz;++z)for(int y=0;y+1<ny;++y)for(int x=0;x+1<nx;++x){std::uint64_t ids[8];for(int i=0;i<8;++i)ids[i]=node(x+corner[i][0],y+corner[i][1],z+corner[i][2]);for(const auto&tetra:tetrahedra){int occupied[4],empty[4],occupiedCount=0,emptyCount=0;for(int i=0;i<4;++i)(inside[std::size_t(ids[tetra[i]])]?occupied[occupiedCount++]:empty[emptyCount++])=tetra[i];if(occupiedCount==0||occupiedCount==4)continue;if(occupiedCount==1||occupiedCount==3){const bool invert=occupiedCount==3;const int isolated=invert?empty[0]:occupied[0];const int*a=invert?occupied:empty;const auto p0=midpoint(ids[isolated],ids[a[0]]),p1=midpoint(ids[isolated],ids[a[1]]),p2=midpoint(ids[isolated],ids[a[2]]);mesh.faces.push_back(invert?Face{p0,p2,p1}:Face{p0,p1,p2});}else{const auto ac=midpoint(ids[occupied[0]],ids[empty[0]]),ad=midpoint(ids[occupied[0]],ids[empty[1]]),bc=midpoint(ids[occupied[1]],ids[empty[0]]),bd=midpoint(ids[occupied[1]],ids[empty[1]]);mesh.faces.push_back({ac,bc,ad});mesh.faces.push_back({ad,bc,bd});}}}
    result.metrics.extractionMilliseconds=phase.elapsed();phase.restart();
    if(mesh.faces.empty()){result.diagnostic=QStringLiteral("The source did not define a stable occupied volume.");return finish();}const auto fixedTopology=orientConsistently(&mesh);
    result.metrics.orientationMilliseconds+=phase.elapsed();phase.restart();
    // Vertex projection never changes face indices. An open extracted mesh
    // cannot become closed at any projection fraction, so reject this axis
    // before six redundant full self-intersection analyses.
    if(!fixedTopology.closed){result.diagnostic=QStringLiteral("Solidified source failed independent validation at every surface-projection level: mesh is open");return finish();}
    if(fixedTopology.components!=1){result.diagnostic=QStringLiteral("Solidified source failed independent validation at every surface-projection level: mesh must have exactly one component");return finish();}
    struct SurfaceProjection { Point nearest; double distance=0.0; bool eligible=false; };
    const double maximumProjectionDistance=1.25*pitch,maximumProjectionDistanceSquared=maximumProjectionDistance*maximumProjectionDistance;double beforeDistance=0.0;
    std::vector<SurfaceProjection>projections;projections.reserve(mesh.vertices.size());
    result.metrics.exhaustiveNearestTriangleTests=std::uint64_t(mesh.vertices.size())*std::uint64_t(triangles.size());
    for(const auto&vertex:mesh.vertices){const auto nearest=queryMode==QueryMode::Indexed?index.nearest(vertex,&result.metrics.nearestTriangleTests):index.exhaustiveNearest(vertex,&result.metrics.nearestTriangleTests);const double distance=std::sqrt(nearest.squaredDistance);beforeDistance+=distance;projections.push_back({nearest.point,distance,nearest.squaredDistance<=maximumProjectionDistanceSquared});}
    result.metrics.nearestProjectionMilliseconds=phase.elapsed();phase.restart();
    result.meanSurfaceDeviationBeforeProjectionMillimetres=beforeDistance/double(mesh.vertices.size());
    constexpr std::array<double,6>projectionFractions={0.95,0.80,0.60,0.40,0.20,0.0};const Point sourceExtent{subtract(sourceBounds.maximum,sourceBounds.minimum)};QString lastValidationError=QStringLiteral("no projection candidate was valid");
    for(const double projectionFraction:projectionFractions){PrintMesh candidate=mesh;double afterDistance=0.0;std::size_t projectedVertices=0;for(std::size_t i=0;i<candidate.vertices.size();++i){const auto&projection=projections[i];if(projection.eligible){candidate.vertices[i]=add(candidate.vertices[i],multiply(subtract(projection.nearest,candidate.vertices[i]),projectionFraction));if(projectionFraction>0.0)++projectedVertices;afterDistance+=(1.0-projectionFraction)*projection.distance;}else afterDistance+=projection.distance;}phase.restart();normalizeExistingTopology(&candidate);result.metrics.orientationMilliseconds+=phase.elapsed();phase.restart();
        const auto rawBounds=vertexBounds(candidate);if(!rawBounds.valid){lastValidationError=QStringLiteral("invalid bounds");continue;}const Point rawExtent{subtract(rawBounds.maximum,rawBounds.minimum)};if(rawExtent.x<=0||rawExtent.y<=0||rawExtent.z<=0){lastValidationError=QStringLiteral("dimensionally degenerate bounds");continue;}for(auto&p:candidate.vertices){p.x=sourceBounds.minimum.x+(p.x-rawBounds.minimum.x)*sourceExtent.x/rawExtent.x;p.y=sourceBounds.minimum.y+(p.y-rawBounds.minimum.y)*sourceExtent.y/rawExtent.y;p.z=sourceBounds.minimum.z+(p.z-rawBounds.minimum.z)*sourceExtent.z/rawExtent.z;}MeshAnalysisOptions analysisOptions;analysisOptions.metrics=&result.metrics.strictBreakdown;const auto analysis=analyzeSource(candidate,analysisOptions);const auto validation=validateBooleanOperand(analysis);result.metrics.strictAnalysisMilliseconds+=phase.elapsed();phase.restart();if(!validation.ok()){lastValidationError=QString::fromStdString(validation.message);continue;}
        for(auto& point:candidate.vertices)point=modelCoordinates(point,axis);
        const auto restoredAnalysis=axis==0?analysis:analyzeSource(candidate,analysisOptions);
        const auto restoredValidation=validateBooleanOperand(restoredAnalysis);
        result.metrics.strictAnalysisMilliseconds+=phase.elapsed();phase.restart();
        if(!restoredValidation.ok()){
            lastValidationError=QString::fromStdString(restoredValidation.message);
            continue;
        }
        result.mesh=std::move(candidate);result.analysis=restoredAnalysis;result.metrics.finalVertices=result.mesh.vertices.size();result.metrics.finalFaces=result.mesh.faces.size();result.surfaceProjectionFraction=projectionFraction;result.projectedVertices=projectedVertices;result.meanSurfaceDeviationAfterProjectionMillimetres=afterDistance/double(mesh.vertices.size());result.successful=true;result.diagnostic=QStringLiteral("Certified source surfaces solidified on a %1 mm deterministic occupancy grid using ray axis %2; %3 vertices projected by %4% toward authoritative surfaces (%5 mm mean deviation before, %6 mm after).").arg(pitch,0,'f',3).arg(axis).arg(result.projectedVertices).arg(100.0*projectionFraction,0,'f',0).arg(result.meanSurfaceDeviationBeforeProjectionMillimetres,0,'f',4).arg(result.meanSurfaceDeviationAfterProjectionMillimetres,0,'f',4);return finish();}
    result.diagnostic=QStringLiteral("Solidified source failed independent validation at every surface-projection level: %1").arg(lastValidationError);return finish();
}

} // namespace PrintGeometry
