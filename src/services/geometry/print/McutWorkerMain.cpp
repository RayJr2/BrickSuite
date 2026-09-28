#include "McutMeshBooleanService.h"
#include "McutWorkerProtocol.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include "BoundedGeometryWorker.h"

int main(int argc,char** argv)
{
    using namespace PrintGeometry;
    using namespace McutWorkerProtocol;
    QCoreApplication application(argc,argv);
    if(argc!=2||!BoundedGeometryWorker::constrainChild())return 2;
    try {
        const QDir directory(QString::fromLocal8Bit(argv[1]));
        QFile input(directory.filePath(QStringLiteral("input.bin")));
        if(input.size()>MaximumWireBytes||!input.open(QIODevice::ReadOnly))return 3;
        QDataStream in(&input);in.setVersion(QDataStream::Qt_6_0);
        quint32 version=0;bool subtract=false;in>>version>>subtract;
        PrintMesh a,b;
        if(version!=Version||!readMesh(in,a,MaximumFaces)||!readMesh(in,b,MaximumFaces)||!in.atEnd()||
           a.faces.size()+b.faces.size()>MaximumFaces)return 4;
        // Already memory-limited and supervised by the parent; never spawn recursively.
        McutMeshBooleanService backend(McutMeshBooleanService::Execution::InProcess);
        const auto result=subtract?backend.subtract(a,b):backend.unite(a,b);
        if(result.mesh.faces.size()>MaximumOutputFaces||result.mesh.vertices.size()>3*MaximumOutputFaces)return 5;
        QFile output(directory.filePath(QStringLiteral("output.bin")));
        if(!output.open(QIODevice::WriteOnly))return 6;
        QDataStream out(&output);out.setVersion(QDataStream::Qt_6_0);
        out<<Version<<qint32(result.error)<<QString::fromStdString(result.message);
        if(result.ok())writeMesh(out,result.mesh);
        return out.status()==QDataStream::Ok&&output.flush()?0:7;
    }catch(const std::exception&){return 8;}
}
