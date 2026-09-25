#include "../src/services/geometry/LDrawLibraryService.h"
#include "../src/services/geometry/PrintOrientation.h"
#include "../src/services/geometry/print/PrintMeshAnalysis.h"
#include "../src/services/geometry/print/SourceSurfaceSolidifier.h"
#include "../src/services/geometry/print/SourceSurfaceQueryIndex.h"

#include <QCoreApplication>
#include <QTextStream>

#include <algorithm>
#include <cmath>

using namespace PrintGeometry;

namespace {
bool require(bool condition,const QString&message){if(!condition)QTextStream(stderr)<<"FAIL: "<<message<<Qt::endl;return condition;}
Point dimensions(const MeshBounds&bounds){return {bounds.maximum.x-bounds.minimum.x,bounds.maximum.y-bounds.minimum.y,bounds.maximum.z-bounds.minimum.z};}
bool sameSourceTriangles(const QVector<LDrawGeometry::Triangle>&a,const QVector<LDrawGeometry::Triangle>&b)
{
    if(a.size()!=b.size())return false;
    for(qsizetype i=0;i<a.size();++i){const auto&left=a[i];const auto&right=b[i];if(left.a!=right.a||left.b!=right.b||left.c!=right.c||left.normal!=right.normal||left.color!=right.color||left.backFaceCull!=right.backFaceCull)return false;}
    return true;
}
bool queryIndexEquivalent()
{
    using Triangle=SourceSurfaceQueryIndex::Triangle;
    const std::vector<Triangle> triangles={
        {{0,0,0},{2,2,0},{1,0,2}},
        {{1,0,0},{1,2,0},{1,0,2}},
        {{0,0,0},{2,2,0},{1,0,2}}, // exact nearest-distance tie
        {{0,0,0},{2,0,0},{0,2,0}}  // projected near-parallel triangle
    };
    const SourceSurfaceQueryIndex index(triangles,-.2,-.2,.2,14,14);
    bool ok=true;
    const auto hit=[](const Triangle&t,double y,double z,double*x){
        const double denominator=(t.b.z-t.c.z)*(t.a.y-t.c.y)+(t.c.y-t.b.y)*(t.a.z-t.c.z);
        if(std::abs(denominator)<1e-14)return false;
        const double u=((t.b.z-t.c.z)*(y-t.c.y)+(t.c.y-t.b.y)*(z-t.c.z))/denominator;
        const double v=((t.c.z-t.a.z)*(y-t.c.y)+(t.a.y-t.c.y)*(z-t.c.z))/denominator;
        const double w=1.0-u-v;
        if(u<-1e-10||v<-1e-10||w<-1e-10)return false;
        *x=u*t.a.x+v*t.b.x+w*t.c.x;return true;
    };
    for(int z=0;z<14;++z)for(int y=0;y<14;++y){
        const double sy=-.2+.2*y+.2e-6,sz=-.2+.2*z+.2*2.41421356237e-6;
        std::vector<std::pair<double,int>> indexed,reference;
        for(int i:index.rayCandidates(y,z)){double x;if(hit(triangles[std::size_t(i)],sy,sz,&x))indexed.push_back({x,i});}
        for(int i:index.allTriangles()){double x;if(hit(triangles[std::size_t(i)],sy,sz,&x))reference.push_back({x,i});}
        const auto order=[](const auto&a,const auto&b){return a.first<b.first;};
        std::stable_sort(indexed.begin(),indexed.end(),order);
        std::stable_sort(reference.begin(),reference.end(),order);
        ok&=indexed==reference;
    }
    for(const Point p:std::vector<Point>{{0,0,0},{1,1,0},{1,0,1},{1.25,.25,.5},
                                          {1,1,1e-10},{-1,-1,-1},{3,3,3}}){
        const auto indexed=index.nearest(p),reference=index.exhaustiveNearest(p);
        ok&=indexed.triangle==reference.triangle&&
            std::abs(indexed.squaredDistance-reference.squaredDistance)<1e-12&&
            indexed.point.x==reference.point.x&&indexed.point.y==reference.point.y&&indexed.point.z==reference.point.z;
        const auto repeated=index.nearest(p);
        ok&=repeated.triangle==indexed.triangle&&repeated.squaredDistance==indexed.squaredDistance;
    }
    return ok&&index.rayIndexed()&&index.rayReferences()<triangles.size()*14u*14u;
}
}

int main(int argc,char**argv)
{
    QCoreApplication app(argc,argv);const auto arguments=app.arguments();bool ok=require(queryIndexEquivalent(),QStringLiteral("indexed ray hits/order and nearest source agree with exhaustive reference at boundaries and ties"));const int libraryAt=arguments.indexOf(QStringLiteral("--ldraw"));if(libraryAt<0||libraryAt+1>=arguments.size())return ok?0:1;
    for(const QString&part:{QStringLiteral("3673"),QStringLiteral("4274"),QStringLiteral("2780")}){
        const auto loaded=LDrawLibraryService::loadPart(arguments[libraryAt+1],part);ok&=require(loaded.ok(),part+QStringLiteral(" loads"));if(!loaded.ok())continue;const auto sourceTriangles=loaded.mesh.triangles;
        const auto first=SourceSurfaceSolidifier::solidify(loaded);ok&=require(first.successful,part+QStringLiteral(" solidifies: ")+first.diagnostic);if(!first.successful)continue;
        ok&=require(sameSourceTriangles(sourceTriangles,loaded.mesh.triangles),part+QStringLiteral(" authoritative Source remains immutable"));
        const auto validation=validateBooleanOperand(first.analysis);ok&=require(validation.ok(),part+QStringLiteral(" validates independently"));const auto source=analyzeSource([&]{PrintMesh mesh;for(const auto&t:loaded.mesh.triangles){const auto convert=[](const QVector3D&p){return Point{.4*double(p.x()),.4*double(p.z()),-.4*double(p.y())};};const auto offset=std::uint32_t(mesh.vertices.size());mesh.vertices.push_back(convert(t.a));mesh.vertices.push_back(convert(t.b));mesh.vertices.push_back(convert(t.c));mesh.faces.push_back({offset,offset+1,offset+2});}return mesh;}());
        ok&=require(first.surfaceProjectionFraction>0.0&&first.projectedVertices>first.mesh.vertices.size()*9/10&&first.meanSurfaceDeviationBeforeProjectionMillimetres>0.01&&first.meanSurfaceDeviationAfterProjectionMillimetres<first.meanSurfaceDeviationBeforeProjectionMillimetres,part+QStringLiteral(" maximum valid authoritative-surface projection measurably reduces volumetric faceting"));
        if(part!=QStringLiteral("2780"))ok&=require(std::abs(first.surfaceProjectionFraction-.95)<1e-9&&first.meanSurfaceDeviationAfterProjectionMillimetres<first.meanSurfaceDeviationBeforeProjectionMillimetres*.25,part+QStringLiteral(" retains the established high-fidelity projection"));
        const auto a=dimensions(source.bounds),b=dimensions(first.analysis.bounds);ok&=require(std::abs(a.x-b.x)<1e-6&&std::abs(a.y-b.y)<1e-6&&std::abs(a.z-b.z)<1e-6,part+QStringLiteral(" exact external dimensions retained"));
        PrintOrientation orientation;orientation.rotate(PrintOrientation::Rotation::YPositive);const auto oriented=orientation.apply(first.mesh);const auto orientedAnalysis=analyzeSource(oriented);const auto orientedDimensions=dimensions(orientedAnalysis.bounds);ok&=require(std::abs(orientedDimensions.x-b.z)<1e-6&&std::abs(orientedDimensions.y-b.y)<1e-6&&std::abs(orientedDimensions.z-b.x)<1e-6&&oriented.vertices.front().x==orientation.map(first.mesh.vertices.front()).x,part+QStringLiteral(" solidified PreparedMesh receives the authoritative unmistakable Y +90 export transform"));
        const auto repeated=SourceSurfaceSolidifier::solidify(loaded);bool identical=repeated.successful&&repeated.mesh.vertices.size()==first.mesh.vertices.size()&&repeated.mesh.faces==first.mesh.faces;if(identical)for(std::size_t i=0;i<first.mesh.vertices.size();++i){const auto&a=first.mesh.vertices[i],&b=repeated.mesh.vertices[i];if(a.x!=b.x||a.y!=b.y||a.z!=b.z){identical=false;break;}}ok&=require(identical,part+QStringLiteral(" deterministic output"));
        if(part==QStringLiteral("3673")){
            const auto exhaustive=SourceSurfaceSolidifier::solidify(loaded,.15,
                SourceSurfaceSolidifier::RayAxis::X,SourceSurfaceSolidifier::QueryMode::ExhaustiveReference);
            bool equal=exhaustive.successful&&exhaustive.mesh.faces==first.mesh.faces&&
                exhaustive.mesh.vertices.size()==first.mesh.vertices.size();
            if(equal)for(std::size_t i=0;i<first.mesh.vertices.size();++i){const auto&a=first.mesh.vertices[i],&b=exhaustive.mesh.vertices[i];
                if(a.x!=b.x||a.y!=b.y||a.z!=b.z){equal=false;break;}}
            ok&=require(equal,QStringLiteral("real 3673 indexed PreparedMesh equals exhaustive reference exactly"));
        }
        QTextStream(stdout)<<part<<" sourceComponents="<<source.connectedComponents<<" sourceBoundaries="<<source.boundaryEdges<<" triangles="<<first.analysis.triangles<<" vertices="<<first.analysis.vertices<<" dimensions="<<b.x<<','<<b.y<<','<<b.z<<" volume="<<first.analysis.absoluteVolume<<" projected="<<first.projectedVertices<<" projectionFraction="<<first.surfaceProjectionFraction<<" meanSurfaceDeviation="<<first.meanSurfaceDeviationBeforeProjectionMillimetres<<"->"<<first.meanSurfaceDeviationAfterProjectionMillimetres<<Qt::endl;
    }
    if(arguments.contains(QStringLiteral("--compare-real")))
        for(const auto&caseEntry:std::vector<std::pair<QString,SourceSurfaceSolidifier::RayAxis>>{
                {QStringLiteral("4488"),SourceSurfaceSolidifier::RayAxis::X},
                {QStringLiteral("4488"),SourceSurfaceSolidifier::RayAxis::Y},
                {QStringLiteral("76385"),SourceSurfaceSolidifier::RayAxis::X},
                {QStringLiteral("23922"),SourceSurfaceSolidifier::RayAxis::X},
                {QStringLiteral("14419"),SourceSurfaceSolidifier::RayAxis::X},
                {QStringLiteral("14419"),SourceSurfaceSolidifier::RayAxis::Y}}){
            const auto loaded=LDrawLibraryService::loadPart(arguments[libraryAt+1],caseEntry.first);
            ok&=require(loaded.ok(),caseEntry.first+QStringLiteral(" comparison source loads"));
            if(!loaded.ok())continue;
            const auto indexed=SourceSurfaceSolidifier::solidify(loaded,.15,caseEntry.second);
            const auto reference=SourceSurfaceSolidifier::solidify(loaded,.15,caseEntry.second,
                SourceSurfaceSolidifier::QueryMode::ExhaustiveReference);
            bool equal=indexed.successful==reference.successful&&indexed.diagnostic==reference.diagnostic&&
                indexed.mesh.faces==reference.mesh.faces&&indexed.mesh.vertices.size()==reference.mesh.vertices.size();
            if(equal)for(std::size_t i=0;i<indexed.mesh.vertices.size();++i){const auto&a=indexed.mesh.vertices[i],&b=reference.mesh.vertices[i];
                if(a.x!=b.x||a.y!=b.y||a.z!=b.z){equal=false;break;}}
            ok&=require(equal,caseEntry.first+QStringLiteral(" axis ")+QString::number(int(caseEntry.second))+
                        QStringLiteral(" indexed and exhaustive outputs match exactly"));
            QTextStream(stdout)<<"comparison part="<<caseEntry.first<<" axis="<<int(caseEntry.second)
                <<" indexed-ms="<<indexed.metrics.totalMilliseconds
                <<" exhaustive-ms="<<reference.metrics.totalMilliseconds
                <<" equal="<<equal<<Qt::endl;
        }
    return ok?0:1;
}
