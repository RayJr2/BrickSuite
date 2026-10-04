#include "../src/services/geometry/print/BoundedGeometryWorker.h"
#include "../src/services/geometry/print/McutWorkerProtocol.h"
// BoundedGeometryWorker includes Windows headers, whose IGNORE macro collides
// with CDT's enum. Isolate the undef to this test's third-party header include.
#ifdef IGNORE
#pragma push_macro("IGNORE")
#undef IGNORE
#define BRICKSUITE_TEST_RESTORE_IGNORE
#endif
#include "mcut/internal/cdt/triangulate.h"
#ifdef BRICKSUITE_TEST_RESTORE_IGNORE
#pragma pop_macro("IGNORE")
#undef BRICKSUITE_TEST_RESTORE_IGNORE
#endif
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QProcess>
#include <QTextStream>
#include <limits>

// Executed in a contained child: the original CDT implementation grows its
// KD-tree indefinitely for NaN. A broken guard must fail this test, not hang it.
int mcutFiniteGeometryChild()
{
    if(!PrintGeometry::BoundedGeometryWorker::constrainChild())return 2;
    for(double bad:{std::numeric_limits<double>::quiet_NaN(),
                    std::numeric_limits<double>::infinity(),-std::numeric_limits<double>::infinity()}){
        for(int axis=0;axis<2;++axis){
            for(bool initialized:{false,true}){
                cdt::triangulator_t<double> triangulator(cdt::vertex_insertion_order_t::AS_GIVEN);
                std::vector<vec2_<double>> valid{{0,0},{1,0},{0,1}};
                if(initialized)triangulator.insert_vertices(valid);
                const auto previous=triangulator.vertices.size();
                auto invalid=valid;
                if(axis==0)invalid[1].x()=bad;else invalid[1].y()=bad;
                bool rejected=false;
                try{triangulator.insert_vertices(invalid);}
                catch(const std::invalid_argument& error){rejected=std::string(error.what())=="non-finite CDT vertex";}
                if(!rejected||triangulator.vertices.size()!=previous)return 3;
            }
        }
    }
    return 0;
}

bool checkMcutFiniteGeometry()
{
    QProcess child;child.setProgram(QCoreApplication::applicationFilePath());child.setArguments({"--finite-cdt"});
    QElapsedTimer timer;timer.start();
    const auto outcome=PrintGeometry::BoundedGeometryWorker::run(child,3000,32ULL*1024*1024);
    bool ok=outcome==PrintGeometry::BoundedGeometryWorker::Outcome::Finished&&
        child.exitStatus()==QProcess::NormalExit&&child.exitCode()==0&&timer.elapsed()<4000;
    child.setArguments({"--queue-wakeup"});
    const auto queueOutcome=PrintGeometry::BoundedGeometryWorker::run(child,15000,32ULL*1024*1024);
    ok&=queueOutcome==PrintGeometry::BoundedGeometryWorker::Outcome::Finished&&
        child.exitStatus()==QProcess::NormalExit&&child.exitCode()==0;
    // The transport boundary must also reject every non-finite coordinate.
    for(double bad:{std::numeric_limits<double>::quiet_NaN(),
                    std::numeric_limits<double>::infinity(),-std::numeric_limits<double>::infinity()}){
        PrintGeometry::PrintMesh input;input.vertices={{bad,0,0},{0,1,0},{0,0,1}};input.faces={{0,1,2}};
        QByteArray bytes;QDataStream writer(&bytes,QIODevice::WriteOnly);
        PrintGeometry::McutWorkerProtocol::writeMesh(writer,input);
        QDataStream reader(bytes);PrintGeometry::PrintMesh output;
        ok&=!PrintGeometry::McutWorkerProtocol::readMesh(reader,output,10);
    }
    if(!ok)QTextStream(stderr)<<"FAIL: finite geometry must be rejected before CDT allocation or worker admission\n";
    return ok;
}
