#include "../src/services/geometry/LDrawLibraryService.h"
#include "../src/services/geometry/PrintOrientation.h"
#include "../src/services/geometry/print/PrintMeshAnalysis.h"
#include "../src/services/geometry/print/SourceSurfaceSolidifier.h"

#include <QCoreApplication>
#include <QTextStream>

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
}

int main(int argc,char**argv)
{
    QCoreApplication app(argc,argv);const auto arguments=app.arguments();const int libraryAt=arguments.indexOf(QStringLiteral("--ldraw"));if(libraryAt<0||libraryAt+1>=arguments.size())return 0;bool ok=true;
    for(const QString&part:{QStringLiteral("3673"),QStringLiteral("4274")}){
        const auto loaded=LDrawLibraryService::loadPart(arguments[libraryAt+1],part);ok&=require(loaded.ok(),part+QStringLiteral(" loads"));if(!loaded.ok())continue;const auto sourceTriangles=loaded.mesh.triangles;
        const auto first=SourceSurfaceSolidifier::solidify(loaded);ok&=require(first.successful,part+QStringLiteral(" solidifies: ")+first.diagnostic);if(!first.successful)continue;
        ok&=require(sameSourceTriangles(sourceTriangles,loaded.mesh.triangles),part+QStringLiteral(" authoritative Source remains immutable"));
        const auto validation=validateBooleanOperand(first.analysis);ok&=require(validation.ok(),part+QStringLiteral(" validates independently"));const auto source=analyzeSource([&]{PrintMesh mesh;for(const auto&t:loaded.mesh.triangles){const auto convert=[](const QVector3D&p){return Point{.4*double(p.x()),.4*double(p.z()),-.4*double(p.y())};};const auto offset=std::uint32_t(mesh.vertices.size());mesh.vertices.push_back(convert(t.a));mesh.vertices.push_back(convert(t.b));mesh.vertices.push_back(convert(t.c));mesh.faces.push_back({offset,offset+1,offset+2});}return mesh;}());
        ok&=require(first.projectedVertices>first.mesh.vertices.size()*9/10&&first.meanSurfaceDeviationBeforeProjectionMillimetres>0.01&&first.meanSurfaceDeviationAfterProjectionMillimetres<first.meanSurfaceDeviationBeforeProjectionMillimetres*.25,part+QStringLiteral(" authoritative-surface projection measurably reduces volumetric faceting"));
        const auto a=dimensions(source.bounds),b=dimensions(first.analysis.bounds);ok&=require(std::abs(a.x-b.x)<1e-6&&std::abs(a.y-b.y)<1e-6&&std::abs(a.z-b.z)<1e-6,part+QStringLiteral(" exact external dimensions retained"));
        PrintOrientation orientation;orientation.rotate(PrintOrientation::Rotation::YPositive);const auto oriented=orientation.apply(first.mesh);const auto orientedAnalysis=analyzeSource(oriented);const auto orientedDimensions=dimensions(orientedAnalysis.bounds);ok&=require(std::abs(orientedDimensions.x-b.z)<1e-6&&std::abs(orientedDimensions.y-b.y)<1e-6&&std::abs(orientedDimensions.z-b.x)<1e-6&&oriented.vertices.front().x==orientation.map(first.mesh.vertices.front()).x,part+QStringLiteral(" solidified PreparedMesh receives the authoritative unmistakable Y +90 export transform"));
        const auto repeated=SourceSurfaceSolidifier::solidify(loaded);bool identical=repeated.successful&&repeated.mesh.vertices.size()==first.mesh.vertices.size()&&repeated.mesh.faces==first.mesh.faces;if(identical)for(std::size_t i=0;i<first.mesh.vertices.size();++i){const auto&a=first.mesh.vertices[i],&b=repeated.mesh.vertices[i];if(a.x!=b.x||a.y!=b.y||a.z!=b.z){identical=false;break;}}ok&=require(identical,part+QStringLiteral(" deterministic output"));
        QTextStream(stdout)<<part<<" triangles="<<first.analysis.triangles<<" vertices="<<first.analysis.vertices<<" dimensions="<<b.x<<','<<b.y<<','<<b.z<<" volume="<<first.analysis.absoluteVolume<<" projected="<<first.projectedVertices<<" meanSurfaceDeviation="<<first.meanSurfaceDeviationBeforeProjectionMillimetres<<"->"<<first.meanSurfaceDeviationAfterProjectionMillimetres<<Qt::endl;
    }
    return ok?0:1;
}
