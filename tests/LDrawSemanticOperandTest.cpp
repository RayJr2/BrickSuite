#include "../src/services/geometry/print/LDrawPrintPreparationService.h"
#include "../src/services/geometry/print/PreparedObjProofWriter.h"
#include "../src/services/geometry/print/PrintMeshAnalysis.h"
#include "../src/services/geometry/LDrawLibraryService.h"
#include "../src/ui/parts/PreparedMeshRenderAdapter.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QTextStream>

#include <cmath>

using namespace PrintGeometry;

namespace {
bool check(bool value,const QString&message)
{
    if(!value)QTextStream(stderr)<<"FAIL: "<<message<<Qt::endl;
    return value;
}

QVector3D sourcePoint(Point p){return QVector3D(float(p.x/0.4),float(-p.z/0.4),float(p.y/0.4));}
LDrawGeometry::LDrawLoadResult roundPassageFixture(bool certified=true,bool oneEnded=false,bool rotated=false,bool mirrored=false)
{
    LDrawGeometry::LDrawLoadResult result;result.sourceModel=std::make_shared<LDrawGeometry::LDrawSourceModel>();
    result.sourceModel->files={{0,"parts/synthetic.dat",LDrawGeometry::SourceClassification::Part},{1,"p/synthetic-round.dat",LDrawGeometry::SourceClassification::Primitive}};
    auto transform=[=](Point p){if(rotated)p={p.z,p.y,-p.x};if(mirrored)p.x=-p.x;return p;};
    auto triangle=[&](Point a,Point b,Point c,int file){if(mirrored)std::swap(b,c);LDrawGeometry::Triangle t;t.a=sourcePoint(transform(a));t.b=sourcePoint(transform(b));t.c=sourcePoint(transform(c));const int index=result.mesh.triangles.size();result.mesh.triangles<<t;result.sourceModel->surfaces<<LDrawGeometry::SurfaceRecord{index,-1,file,index+1,3,certified||file==0,true,false};};
    constexpr int OuterSegments=8,PassageSegments=16;const double pi=3.14159265358979323846;
    auto point=[](double radius,double angle,double z){return Point{radius*std::cos(angle),radius*std::sin(angle),z};};
    for(int i=0;i<OuterSegments;++i){const int n=(i+1)%OuterSegments;const double a=2*pi*i/OuterSegments,b=2*pi*n/OuterSegments;const auto bo=point(2,a,-2),bn=point(2,b,-2),to=point(2,a,2),tn=point(2,b,2),bi=point(1,a,-2),bni=point(1,b,-2),ti=point(1,a,2),tni=point(1,b,2);triangle(bo,bn,to,0);triangle(bn,tn,to,0);triangle(to,tn,ti,0);triangle(tn,tni,ti,0);triangle(bo,bi,bn,0);triangle(bn,bi,bni,0);}
    for(int i=0;i<PassageSegments;++i){const int n=(i+1)%PassageSegments;const double a=2*pi*i/PassageSegments,b=2*pi*n/PassageSegments;const auto low=point(1,a,-2),lowNext=point(1,b,-2),high=point(1,a,2),highNext=point(1,b,2);triangle(low,high,lowNext,1);triangle(lowNext,high,highNext,1);}
    if(oneEnded){const Point center{0,0,-2};for(int i=0;i<PassageSegments;++i){const int n=(i+1)%PassageSegments;triangle(center,point(1,2*pi*i/PassageSegments,-2),point(1,2*pi*n/PassageSegments,-2),1);}}
    return result;
}

bool segmentRemainsOpen(const PrintMesh&mesh,const Point&start,const Point&end)
{
    const Point direction{end.x-start.x,end.y-start.y,end.z-start.z};auto subtract=[](const Point&a,const Point&b){return Point{a.x-b.x,a.y-b.y,a.z-b.z};};auto cross=[](const Point&a,const Point&b){return Point{a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};};auto dot=[](const Point&a,const Point&b){return a.x*b.x+a.y*b.y+a.z*b.z;};
    for(const auto&face:mesh.faces){const auto&a=mesh.vertices[face[0]],&b=mesh.vertices[face[1]],&c=mesh.vertices[face[2]];const Point e1=subtract(b,a),e2=subtract(c,a),p=cross(direction,e2);const double determinant=dot(e1,p);if(std::abs(determinant)<1e-10)continue;const double inverse=1.0/determinant;const Point t=subtract(start,a);const double u=dot(t,p)*inverse;if(u<0.0||u>1.0)continue;const Point q=cross(t,e1);const double v=dot(direction,q)*inverse;if(v<0.0||u+v>1.0)continue;const double distance=dot(e2,q)*inverse;if(distance>=0.0&&distance<=1.0)return false;}
    return true;
}
}

int main(int argc,char**argv)
{
    QCoreApplication app(argc,argv);
    bool ok=true;
    PrintMesh component;
    component.vertices={{-1,-1,0},{1,-1,0},{1,1,0},{-1,1,0},{-1,-1,2},{1,-1,2},{1,1,2},{-1,1,2}};
    const std::vector<std::uint32_t>loop{0,1,2,3};
    auto direction=boundaryAttachmentDirection(component,loop);
    ok&=check(direction.z<-.999,"topology-derived attachment direction");
    for(auto&point:component.vertices){const double x=point.x;point.x=point.z;point.z=-x;}
    direction=boundaryAttachmentDirection(component,loop);
    ok&=check(direction.x<-.999,"rotated attachment direction");

    const auto validFixture=roundPassageFixture();const auto semantic=LDrawSemanticOperandBuilder::build(validFixture);
    ok&=check(semantic.ok(),"certified round through-passage recognized");
    ok&=check(std::count_if(semantic.operands.cbegin(),semantic.operands.cend(),[](const auto&o){return o.role==SemanticRole::SubtractivePassage&&o.feature==SemanticFeature::RoundThroughPassage;})==1,"round passage semantic identity retained");
    const auto repeated=LDrawSemanticOperandBuilder::build(validFixture);ok&=check(repeated.ok()&&repeated.closureTriangles==semantic.closureTriangles&&repeated.operands.size()==semantic.operands.size(),"round passage classification deterministic");
    ok&=check(LDrawSemanticOperandBuilder::build(roundPassageFixture(true,false,true)).ok(),"rotated round passage recognized");
    ok&=check(LDrawSemanticOperandBuilder::build(roundPassageFixture(true,false,false,true)).ok(),"mirrored round passage recognized");
    ok&=check(!LDrawSemanticOperandBuilder::build(roundPassageFixture(true,true)).ok(),"one-ended cavity rejected");
    ok&=check(!LDrawSemanticOperandBuilder::build(roundPassageFixture(false)).ok(),"uncertified passage rejected");
    LDrawPrintPreparationService syntheticService;PrintPreparationRequest syntheticRequest;syntheticRequest.partReference="synthetic-round";syntheticRequest.ldrawIdentity="synthetic-round";syntheticRequest.libraryAuthority="synthetic";syntheticRequest.loadResult=validFixture;const auto syntheticPrepared=syntheticService.prepare(syntheticRequest);ok&=check(syntheticPrepared.ready(),"synthetic round passage prepares");if(syntheticPrepared.ready())ok&=check(segmentRemainsOpen(syntheticPrepared.preparedMesh->mesh,{0,0,-2.1},{0,0,2.1}),"round passage remains connected and uncapped");

    const auto args=app.arguments();
    const int libraryAt=args.indexOf("--ldraw");
    if(libraryAt<0||libraryAt+1>=args.size())return ok?0:1;

    QString output=QDir::currentPath();
    const int outputAt=args.indexOf("--proof-dir");
    if(outputAt>=0&&outputAt+1<args.size())output=args[outputAt+1];
    QDir().mkpath(output);
    auto cache=std::make_shared<PrintPreparationCache>();
    LDrawPrintPreparationService service(cache);
    for(const QString&id:{QStringLiteral("3001"),QStringLiteral("3622"),QStringLiteral("3700"),QStringLiteral("11477"),QStringLiteral("32064a")}){
        auto loaded=LDrawLibraryService::loadPart(args[libraryAt+1],id);
        ok&=check(loaded.ok(),id+" load");
        if(!loaded.ok())continue;
        const auto semantic=LDrawSemanticOperandBuilder::build(loaded);
        QTextStream(stdout)<<id
            <<" semanticStatus="<<int(semantic.status)
            <<" groups="<<semantic.semanticGroups
            <<" operands="<<semantic.operands.size()
            <<" sourceTriangles="<<semantic.sourceAnalysis.triangles
            <<" sourceComponents="<<semantic.sourceAnalysis.connectedComponents
            <<" sourceBoundaries="<<semantic.sourceAnalysis.boundaryEdges
            <<" stitchCandidates="<<semantic.stitchDiagnostics.candidateRelationships
            <<" stitchSplits="<<semantic.stitchDiagnostics.acceptedSplits
            <<" stitchTriangles="<<semantic.stitchDiagnostics.trianglesBefore<<"->"<<semantic.stitchDiagnostics.trianglesAfter
            <<" stitchBoundaries="<<semantic.stitchDiagnostics.boundariesBefore<<"->"<<semantic.stitchDiagnostics.boundariesAfter
            <<" diagnostic="<<semantic.diagnostics.join(' ')<<Qt::endl;
        PrintPreparationRequest request;
        request.partReference=id;
        request.ldrawIdentity=id;
        request.libraryAuthority=args[libraryAt+1];
        request.loadResult=loaded;
        auto result=service.prepare(request);
        if(id=="32064a"){ok&=check(!result.ready(),id+" remains safely unsupported after generic stitching");continue;}
        ok&=check(result.ready()&&!result.cacheHit,id+" production preparation");
        if(!result.ready())continue;
        if(id=="11477"){
            ok&=check(semantic.stitchDiagnostics.trianglesBefore==152&&semantic.stitchDiagnostics.trianglesAfter==156,"11477 certified stitching triangle metrics");
            ok&=check(semantic.stitchDiagnostics.boundariesBefore==8&&semantic.stitchDiagnostics.boundariesAfter==0&&semantic.stitchDiagnostics.acceptedSplits==4,"11477 certified stitching closes topology-only seams");
            ok&=check(result.semanticOperandCount==1&&result.operations.isEmpty(),"11477 is one closed semantic body without Boolean reconstruction");
        }
        QString error;
        ok&=check(PreparedObjProofWriter::write(result.preparedMesh->mesh,QDir(output).filePath(id+"-ldraw-aware-proof.obj"),&error),error);
        QElapsedTimer hitTimer;
        hitTimer.start();
        auto cached=service.prepare(request);
        const auto hitMicros=hitTimer.nsecsElapsed()/1000;
        ok&=check(cached.ready()&&cached.cacheHit,id+" cache hit");
        const auto&analysis=result.finalAnalysis;
        if(id=="3700")ok&=check(segmentRemainsOpen(result.preparedMesh->mesh,{0,-4.1,-4},{0,4.1,-4}),"3700 nominal round passage remains open end-to-end");
        const auto&bounds=analysis.bounds;
        const auto render=PreparedMeshRenderAdapter::fromPreparedMesh(*result.preparedMesh);
        QTextStream(stdout)<<id
            <<" source="<<result.sourceAnalysis.triangles
            <<" sourceComponents="<<result.sourceAnalysis.connectedComponents
            <<" sourceBoundaries="<<result.sourceAnalysis.boundaryEdges
            <<" operands="<<result.semanticOperandCount
            <<" booleans="<<result.operations.size()
            <<" prepared="<<analysis.triangles
            <<" topologyEdges="<<render.topologyEdges.size()
            <<" featureEdges="<<render.featureEdges.size()
            <<" components="<<analysis.connectedComponents
            <<" boundaries="<<analysis.boundaryEdges
            <<" nonManifoldEdges="<<analysis.nonManifoldEdges
            <<" nonManifoldVertices="<<analysis.nonManifoldVertices
            <<" intersections="<<analysis.selfIntersections
            <<" volume="<<analysis.absoluteVolume
            <<" dimensions="<<bounds.maximum.x-bounds.minimum.x<<','<<bounds.maximum.y-bounds.minimum.y<<','<<bounds.maximum.z-bounds.minimum.z
            <<" boundsDeviation="<<result.dimensionalFidelity.maximumBoundsDeviationMillimetres
            <<" sourceMs="<<result.timings.sourceAnalysisMilliseconds
            <<" semanticMs="<<result.timings.semanticConstructionMilliseconds
            <<" operandValidationMs="<<result.timings.operandValidationMilliseconds
            <<" booleanMs="<<result.timings.booleanCompositionMilliseconds
            <<" finalMs="<<result.timings.finalValidationMilliseconds
            <<" totalMs="<<result.timings.totalMilliseconds
            <<" cacheHitUs="<<hitMicros
            <<" cacheBytes="<<cache->statistics().approximateBytes<<Qt::endl;
    }
    return ok?0:1;
}
