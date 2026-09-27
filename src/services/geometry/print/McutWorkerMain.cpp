#include "McutMeshBooleanService.h"
#include "McutWorkerProtocol.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <algorithm>
#ifdef Q_OS_WIN
#include <windows.h>
#else
#include <sys/resource.h>
#endif

namespace {
bool constrainMemory()
{
    constexpr std::size_t bytes=512*1024*1024;
#ifdef Q_OS_WIN
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
    const HANDLE job=CreateJobObjectW(nullptr,nullptr);
    if(!job)return false;
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_PROCESS_MEMORY;
    limits.ProcessMemoryLimit=bytes;
    if(!SetInformationJobObject(job,JobObjectExtendedLimitInformation,&limits,sizeof(limits))||
       !AssignProcessToJobObject(job,GetCurrentProcess())){CloseHandle(job);return false;}
    return true; // Keep the job handle alive until process exit.
#else
    rlimit limit{};
    if(getrlimit(RLIMIT_AS,&limit)!=0)return false;
    limit.rlim_cur=std::min<rlim_t>(limit.rlim_cur,bytes);
    return setrlimit(RLIMIT_AS,&limit)==0;
#endif
}
}
int main(int argc,char** argv)
{
    using namespace PrintGeometry;
    using namespace McutWorkerProtocol;
    QCoreApplication application(argc,argv);
    if(argc!=2||!constrainMemory())return 2;
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
