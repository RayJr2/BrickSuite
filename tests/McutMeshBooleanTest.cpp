#include "../src/services/geometry/print/McutMeshBooleanService.h"
#include "../src/services/geometry/print/PrintMeshAnalysis.h"
#include <QCoreApplication>
#include <QTextStream>
#include <QThread>
#include <QFile>
#include <QDir>
#include <QElapsedTimer>
#include <QTemporaryDir>
#include <limits>
#include "../src/services/geometry/print/BoundedGeometryWorker.h"
#include "../src/services/geometry/print/McutWorkerProtocol.h"
#ifdef Q_OS_MACOS
#include <signal.h>
#include <cerrno>
#endif
using namespace PrintGeometry;
int mcutFiniteGeometryChild();
int mcutQueueWakeupChild();
bool checkMcutFiniteGeometry();
namespace {
int simulatedWorker(const QString& directory)
{
    if(!BoundedGeometryWorker::constrainChild())return 19;
    QFile record(QString::fromLocal8Bit(qgetenv("BRICKSUITE_TEST_WORKER_RECORD")));
    if(!record.open(QIODevice::WriteOnly))return 20;
    record.write(directory.toUtf8()+"\n"+QByteArray::number(QCoreApplication::applicationPid()));record.close();
    const auto mode=qgetenv("BRICKSUITE_TEST_BOOLEAN_WORKER");
    if(mode=="memory"){
        // At most 64 MiB, touched page-by-page, against a 32 MiB test budget.
        // Keep the allocation alive so the watchdog can deterministically sample it.
        std::vector<unsigned char> memory(64*1024*1024);
        for(std::size_t i=0;i<memory.size();i+=4096)reinterpret_cast<volatile unsigned char*>(memory.data())[i]=static_cast<unsigned char>(i/4096);
        QThread::sleep(5);return memory.front();
    }
    if(mode=="missing")return 0;
    if(mode=="success"||mode=="nonfinite"){
        QFile input(QDir(directory).filePath("input.bin"));if(!input.open(QIODevice::ReadOnly))return 21;
        QDataStream in(&input);in.setVersion(QDataStream::Qt_6_0);quint32 version;bool subtract;PrintMesh a,b;
        in>>version>>subtract;
        if(!McutWorkerProtocol::readMesh(in,a,100)||!McutWorkerProtocol::readMesh(in,b,100))return 22;
        QFile output(QDir(directory).filePath("output.bin"));if(!output.open(QIODevice::WriteOnly))return 23;
        QDataStream out(&output);out.setVersion(QDataStream::Qt_6_0);
        out<<McutWorkerProtocol::Version<<qint32(MeshBooleanError::None)<<QStringLiteral("transport cleanup fixture");
        if(mode=="nonfinite")a.vertices.front().x=std::numeric_limits<double>::quiet_NaN();
        McutWorkerProtocol::writeMesh(out,a);return 0;
    }
    if(mode=="failed")return 17;
    if(mode=="timeout"){QThread::sleep(5);return 0;}
    QFile output(QDir(directory).filePath("output.bin"));
    if(!output.open(QIODevice::WriteOnly))return 2;
    output.write("invalid worker output");return 0;
}
bool workerFailureTests(const PrintMesh& a,const PrintMesh& b)
{
    bool ok=true;QTemporaryDir records;
    QStringList modes{"success","failed","timeout","malformed","missing","nonfinite"};
#ifdef Q_OS_MACOS
    modes<<"memory";
#endif
    for(const auto& mode:modes){
        const auto path=records.filePath(mode);qputenv("BRICKSUITE_TEST_WORKER_RECORD",path.toUtf8());
        qputenv("BRICKSUITE_TEST_BOOLEAN_WORKER",mode.toUtf8());
        QElapsedTimer timer;timer.start();
        const auto result=McutMeshBooleanService(QCoreApplication::applicationFilePath(),mode=="timeout"?200:3000,
            mode=="memory"?32ULL*1024*1024:BoundedGeometryWorker::MemoryBudgetBytes).unite(a,b);
        const auto expected=mode=="success"?MeshBooleanError::None:
            (mode=="timeout"||mode=="memory"?MeshBooleanError::ResourceLimitExceeded:MeshBooleanError::BackendFailure);
        bool passed=result.error==expected&&timer.elapsed()<4000;
        if(mode!="success")passed&=result.mesh.faces.empty();
        QFile record(path);passed&=record.open(QIODevice::ReadOnly);
        const auto lines=record.readAll().split('\n');
        passed&=lines.size()==2&&!QDir(QString::fromUtf8(lines.value(0))).exists();
#ifdef Q_OS_MACOS
        errno=0;passed&=::kill(lines.value(1).toLongLong(),0)==-1&&errno==ESRCH;
#endif
        QTextStream(stdout)<<"Worker mode "<<mode<<": elapsed="<<timer.elapsed()<<" ms, cleanup="<<passed<<Qt::endl;
        if(!passed)QTextStream(stderr)<<"FAIL worker mode "<<mode<<": "<<QString::fromStdString(result.message)<<Qt::endl;
        ok&=passed;
    }
    qunsetenv("BRICKSUITE_TEST_WORKER_RECORD");
    qunsetenv("BRICKSUITE_TEST_BOOLEAN_WORKER");
    ok&=!McutMeshBooleanService("missing-worker-executable").unite(a,b).ok();
    return ok;
}
}
namespace {bool check(bool v,const QString&m){if(!v)QTextStream(stderr)<<"FAIL: "<<m<<Qt::endl;return v;}PrintMesh box(double x0,double y0,double z0,double x1,double y1,double z1){PrintMesh m;m.vertices={{x0,y0,z0},{x1,y0,z0},{x1,y1,z0},{x0,y1,z0},{x0,y0,z1},{x1,y0,z1},{x1,y1,z1},{x0,y1,z1}};m.faces={{0,2,1},{0,3,2},{4,5,6},{4,6,7},{0,1,5},{0,5,4},{3,7,6},{3,6,2},{0,4,7},{0,7,3},{1,2,6},{1,6,5}};return m;}}
int main(int argc,char**argv){QCoreApplication app(argc,argv);if(argc==2&&app.arguments()[1]=="--queue-wakeup")return mcutQueueWakeupChild();if(argc==2&&app.arguments()[1]=="--finite-cdt")return mcutFiniteGeometryChild();if(argc==2)return simulatedWorker(app.arguments()[1]);bool ok=check(McutMeshBooleanService::available(),"MCUT available");McutMeshBooleanService service(McutMeshBooleanService::Execution::Isolated);auto source=box(0,0,0,2,2,2),add=box(1,0,0,3,2,2);auto result=service.unite(source,add);ok&=check(result.ok(),QString::fromStdString(result.message));if(result.ok()){ok&=check(validateBooleanOperand(result.resultAnalysis).ok(),"union independently valid");ok&=check(std::abs(result.resultAnalysis.absoluteVolume-12.0)<0.01,"union volume");auto again=service.unite(source,add);ok&=check(again.ok()&&again.resultAnalysis.triangles==result.resultAnalysis.triangles,"deterministic union metrics");}auto difference=service.subtract(source,add);ok&=check(difference.ok(),QString::fromStdString(difference.message));if(difference.ok()){ok&=check(validateBooleanOperand(difference.resultAnalysis).ok(),"difference independently valid");ok&=check(std::abs(difference.resultAnalysis.absoluteVolume-4.0)<0.01,"difference volume");auto again=service.subtract(source,add);ok&=check(again.ok()&&again.resultAnalysis.triangles==difference.resultAnalysis.triangles,"deterministic difference metrics");}auto open=source;open.faces.pop_back();ok&=check(service.unite(open,add).error==MeshBooleanError::InvalidSource,"open operand rejected before MCUT");ok&=check(service.subtract(source,open).error==MeshBooleanError::InvalidAdditive,"open passage rejected before MCUT");PrintMesh multi=source;auto offset=std::uint32_t(multi.vertices.size());auto distantBox=box(10,0,0,11,1,1);multi.vertices.insert(multi.vertices.end(),distantBox.vertices.begin(),distantBox.vertices.end());for(auto f:distantBox.faces){for(auto&i:f)i+=offset;multi.faces.push_back(f);}ok&=check(service.unite(multi,add).error==MeshBooleanError::InvalidSource,"multi-component operand rejected");ok&=check(workerFailureTests(source,add),"worker failure, timeout, malformed output and missing executable fail closed");ok&=checkMcutFiniteGeometry();return ok?0:1;}
