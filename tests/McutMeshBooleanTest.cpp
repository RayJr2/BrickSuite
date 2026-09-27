#include "../src/services/geometry/print/McutMeshBooleanService.h"
#include "../src/services/geometry/print/PrintMeshAnalysis.h"
#include <QCoreApplication>
#include <QTextStream>
#include <QThread>
#include <QFile>
#include <QDir>
#include <QElapsedTimer>
using namespace PrintGeometry;
namespace {
int simulatedWorker(const QString& directory)
{
    const auto mode=qgetenv("BRICKSUITE_TEST_BOOLEAN_WORKER");
    if(mode=="failed")return 17;
    if(mode=="timeout"){QThread::sleep(5);return 0;}
    QFile output(QDir(directory).filePath("output.bin"));
    if(!output.open(QIODevice::WriteOnly))return 2;
    output.write("invalid worker output");return 0;
}
bool workerFailureTests(const PrintMesh& a,const PrintMesh& b)
{
    bool ok=true;
    for(const auto& mode:{"failed","timeout","malformed"}){
        qputenv("BRICKSUITE_TEST_BOOLEAN_WORKER",mode);
        QElapsedTimer timer;timer.start();
        const auto result=McutMeshBooleanService(QCoreApplication::applicationFilePath(),200).unite(a,b);
        ok&=!result.ok()&&result.mesh.faces.empty()&&timer.elapsed()<4000;
        ok&=result.error==(QByteArray(mode)=="timeout"?MeshBooleanError::ResourceLimitExceeded:MeshBooleanError::BackendFailure);
    }
    qunsetenv("BRICKSUITE_TEST_BOOLEAN_WORKER");
    ok&=!McutMeshBooleanService("missing-worker-executable").unite(a,b).ok();
    return ok;
}
}
namespace {bool check(bool v,const QString&m){if(!v)QTextStream(stderr)<<"FAIL: "<<m<<Qt::endl;return v;}PrintMesh box(double x0,double y0,double z0,double x1,double y1,double z1){PrintMesh m;m.vertices={{x0,y0,z0},{x1,y0,z0},{x1,y1,z0},{x0,y1,z0},{x0,y0,z1},{x1,y0,z1},{x1,y1,z1},{x0,y1,z1}};m.faces={{0,2,1},{0,3,2},{4,5,6},{4,6,7},{0,1,5},{0,5,4},{3,7,6},{3,6,2},{0,4,7},{0,7,3},{1,2,6},{1,6,5}};return m;}}
int main(int argc,char**argv){QCoreApplication app(argc,argv);if(argc==2)return simulatedWorker(app.arguments()[1]);bool ok=check(McutMeshBooleanService::available(),"MCUT available");McutMeshBooleanService service(McutMeshBooleanService::Execution::Isolated);auto source=box(0,0,0,2,2,2),add=box(1,0,0,3,2,2);auto result=service.unite(source,add);ok&=check(result.ok(),QString::fromStdString(result.message));if(result.ok()){ok&=check(validateBooleanOperand(result.resultAnalysis).ok(),"union independently valid");ok&=check(std::abs(result.resultAnalysis.absoluteVolume-12.0)<0.01,"union volume");auto again=service.unite(source,add);ok&=check(again.ok()&&again.resultAnalysis.triangles==result.resultAnalysis.triangles,"deterministic union metrics");}auto difference=service.subtract(source,add);ok&=check(difference.ok(),QString::fromStdString(difference.message));if(difference.ok()){ok&=check(validateBooleanOperand(difference.resultAnalysis).ok(),"difference independently valid");ok&=check(std::abs(difference.resultAnalysis.absoluteVolume-4.0)<0.01,"difference volume");auto again=service.subtract(source,add);ok&=check(again.ok()&&again.resultAnalysis.triangles==difference.resultAnalysis.triangles,"deterministic difference metrics");}auto open=source;open.faces.pop_back();ok&=check(service.unite(open,add).error==MeshBooleanError::InvalidSource,"open operand rejected before MCUT");ok&=check(service.subtract(source,open).error==MeshBooleanError::InvalidAdditive,"open passage rejected before MCUT");PrintMesh multi=source;auto offset=std::uint32_t(multi.vertices.size());auto far=box(10,0,0,11,1,1);multi.vertices.insert(multi.vertices.end(),far.vertices.begin(),far.vertices.end());for(auto f:far.faces){for(auto&i:f)i+=offset;multi.faces.push_back(f);}ok&=check(service.unite(multi,add).error==MeshBooleanError::InvalidSource,"multi-component operand rejected");ok&=check(workerFailureTests(source,add),"worker failure, timeout, malformed output and missing executable fail closed");return ok?0:1;}
