#include "../src/services/geometry/print/LDrawPrintPreparationService.h"
#include "../src/services/geometry/print/PreparedObjProofWriter.h"
#include "../src/services/geometry/print/PrintMeshAnalysis.h"
#include "../src/services/geometry/LDrawLibraryService.h"
#include "../src/ui/parts/PreparedMeshRenderAdapter.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QTextStream>

using namespace PrintGeometry;

namespace {
bool check(bool value,const QString&message)
{
    if(!value)QTextStream(stderr)<<"FAIL: "<<message<<Qt::endl;
    return value;
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

    const auto args=app.arguments();
    const int libraryAt=args.indexOf("--ldraw");
    if(libraryAt<0||libraryAt+1>=args.size())return ok?0:1;

    QString output=QDir::currentPath();
    const int outputAt=args.indexOf("--proof-dir");
    if(outputAt>=0&&outputAt+1<args.size())output=args[outputAt+1];
    QDir().mkpath(output);
    auto cache=std::make_shared<PrintPreparationCache>();
    LDrawPrintPreparationService service(cache);
    for(const QString&id:{QStringLiteral("3001"),QStringLiteral("3622")}){
        auto loaded=LDrawLibraryService::loadPart(args[libraryAt+1],id);
        ok&=check(loaded.ok(),id+" load");
        if(!loaded.ok())continue;
        PrintPreparationRequest request;
        request.partReference=id;
        request.ldrawIdentity=id;
        request.libraryAuthority=args[libraryAt+1];
        request.loadResult=loaded;
        auto result=service.prepare(request);
        ok&=check(result.ready()&&!result.cacheHit,id+" production preparation");
        if(!result.ready())continue;
        QString error;
        ok&=check(PreparedObjProofWriter::write(result.preparedMesh->mesh,QDir(output).filePath(id+"-ldraw-aware-proof.obj"),&error),error);
        QElapsedTimer hitTimer;
        hitTimer.start();
        auto cached=service.prepare(request);
        const auto hitMicros=hitTimer.nsecsElapsed()/1000;
        ok&=check(cached.ready()&&cached.cacheHit,id+" cache hit");
        const auto&analysis=result.finalAnalysis;
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
