#include "LocalPrintableOverrideService.h"
#include "McutMeshBooleanService.h"
#include "PrintMeshAnalysis.h"
#include <QCoreApplication>
#include <QDataStream>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>
#include <map>
#include <queue>
#include <cstring>
#include <algorithm>
#ifdef Q_OS_WIN
#include <windows.h>
#else
#include <sys/resource.h>
#endif

namespace PrintGeometry {
namespace {
constexpr std::size_t MaximumUnionFaces=2000;
constexpr std::size_t MaximumUnionVertices=6000;
constexpr std::size_t MaximumOutputFaces=10000;
constexpr int DeadlineMilliseconds=10000;
constexpr auto WorkerArgument="--local-override-union-worker";
constexpr quint32 WireVersion=1;
void writeMesh(QDataStream& s,const PrintMesh& m)
{
    s<<quint32(m.vertices.size())<<quint32(m.faces.size());
    for(const auto& p:m.vertices)s<<p.x<<p.y<<p.z;
    for(const auto& f:m.faces)s<<quint32(f[0])<<quint32(f[1])<<quint32(f[2]);
}
bool readMesh(QDataStream& s,PrintMesh* m,std::size_t maximumFaces)
{
    quint32 vertices=0,faces=0;s>>vertices>>faces;
    if(vertices>3*maximumFaces||faces>maximumFaces)return false;
    m->vertices.resize(vertices);m->faces.resize(faces);
    for(auto& p:m->vertices)s>>p.x>>p.y>>p.z;
    for(auto& f:m->faces)for(auto& v:f)s>>v;
    return s.status()==QDataStream::Ok;
}
bool constrainWorkerMemory()
{
    constexpr std::size_t bytes=512*1024*1024;
#ifdef Q_OS_WIN
    // The handle lives until process exit. No shared/application process joins it.
    const HANDLE job=CreateJobObjectW(nullptr,nullptr);
    if(!job)return false;
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_PROCESS_MEMORY;
    limits.ProcessMemoryLimit=bytes;
    if(!SetInformationJobObject(job,JobObjectExtendedLimitInformation,&limits,sizeof(limits))||
       !AssignProcessToJobObject(job,GetCurrentProcess())){CloseHandle(job);return false;}
    return true;
#else
    rlimit limit{};
    if(getrlimit(RLIMIT_AS,&limit)!=0)return false;
    limit.rlim_cur=std::min<rlim_t>(limit.rlim_cur,bytes);
    return setrlimit(RLIMIT_AS,&limit)==0;
#endif
}
QString summary(std::size_t i,const MeshAnalysisResult& a)
{
    return QStringLiteral("Component %1: %2 vertices, %3 faces, %4 boundaries, %5 non-manifold edges, %6 non-manifold vertices, %7 degenerates, %8 duplicate faces, %9 self-intersections, oriented=%10, signed volume=%11 mm³.")
        .arg(i+1).arg(a.vertices).arg(a.triangles).arg(a.boundaryEdges).arg(a.nonManifoldEdges).arg(a.nonManifoldVertices)
        .arg(a.degenerateFaces).arg(a.duplicateFaces).arg(a.selfIntersections).arg(a.consistentlyOriented?QStringLiteral("yes"):QStringLiteral("no")).arg(a.signedVolume,0,'g',9);
}
}

std::optional<int> LocalPrintableOverrideService::runUnionWorker(int argc,char** argv)
{
    if(argc<2||std::strcmp(argv[1],WorkerArgument)!=0)return std::nullopt;
    if(argc!=3)return 2;
    QCoreApplication application(argc,argv);
    if(!constrainWorkerMemory())return 3;
    try {
        const QDir directory(QString::fromLocal8Bit(argv[2]));
        QFile input(directory.filePath(QStringLiteral("input.bin")));
        if(input.size()>1024*1024||!input.open(QIODevice::ReadOnly))return 4;
        QDataStream in(&input);in.setVersion(QDataStream::Qt_6_0);quint32 version=0;in>>version;
        PrintMesh a,b;
        if(version!=WireVersion||!readMesh(in,&a,MaximumUnionFaces)||!readMesh(in,&b,MaximumUnionFaces)||!in.atEnd()||
           a.faces.size()+b.faces.size()>MaximumUnionFaces||a.vertices.size()+b.vertices.size()>MaximumUnionVertices)return 5;
        // Existing service independently validates operands and union result.
        const auto result=McutMeshBooleanService().unite(a,b);
        const bool accepted=result.ok()&&result.mesh.faces.size()<=MaximumOutputFaces&&result.mesh.vertices.size()<=3*MaximumOutputFaces;
        QFile output(directory.filePath(QStringLiteral("output.bin")));
        if(!output.open(QIODevice::WriteOnly))return 6;
        QDataStream out(&output);out.setVersion(QDataStream::Qt_6_0);
        out<<WireVersion<<accepted<<(result.ok()&&!accepted?QStringLiteral("Union output exceeds 10,000-face / 30,000-vertex limit."):QString::fromStdString(result.message));
        if(accepted)writeMesh(out,result.mesh);
        return out.status()==QDataStream::Ok&&output.flush()?0:7;
    }catch(const std::exception&){return 8;}
}

LocalPrintableOverrideService::NormalizationResult LocalPrintableOverrideService::normalizeClosedComponents(const PrintMesh& mesh)
{
    NormalizationResult result;QElapsedTimer timer;timer.start();
    const auto finish=[&]{result.elapsedMilliseconds=timer.elapsed();return result;};
    const auto whole=analyzeSource(mesh);
    if(validatePreparedMesh(whole).ok()){result.mesh=mesh;result.accepted=true;return finish();}
    if(!whole.finite||!whole.indicesValid||mesh.faces.size()>MaximumUnionFaces||mesh.vertices.size()>MaximumUnionVertices||whole.resourceLimitExceeded){result.diagnostic=QStringLiteral("Closed-component normalization ineligible: invalid input or 2,000-face / 6,000-vertex workload limit exceeded.");return finish();}
    // Edge adjacency matches analyzeSource. Vertex-only contacts do not join solids.
    using Edge=std::pair<std::uint32_t,std::uint32_t>;
    std::map<Edge,std::vector<std::size_t>> edges;
    for(std::size_t i=0;i<mesh.faces.size();++i){const auto& f=mesh.faces[i];for(int j=0;j<3;++j)edges[std::minmax(f[j],f[(j+1)%3])].push_back(i);}
    std::vector<std::vector<std::size_t>> adjacent(mesh.faces.size());
    for(const auto& e:edges)if(e.second.size()==2){adjacent[e.second[0]].push_back(e.second[1]);adjacent[e.second[1]].push_back(e.second[0]);}
    std::vector<int> owner(mesh.faces.size(),-1);std::vector<PrintMesh> parts;
    for(std::size_t root=0;root<mesh.faces.size();++root){
        if(owner[root]>=0)continue;
        if(parts.size()==2){result.diagnostic=QStringLiteral("Closed-component normalization supports at most two components and one union operation.");return finish();}
        const int id=int(parts.size());PrintMesh part;std::map<std::uint32_t,std::uint32_t> vertices;
        std::queue<std::size_t> queue;queue.push(root);owner[root]=id;
        while(!queue.empty()){
            const auto at=queue.front();queue.pop();Face face;
            for(int j=0;j<3;++j){const auto original=mesh.faces[at][j];auto inserted=vertices.emplace(original,std::uint32_t(vertices.size()));if(inserted.second)part.vertices.push_back(mesh.vertices[original]);face[j]=inserted.first->second;}
            part.faces.push_back(face);
            for(auto next:adjacent[at])if(owner[next]<0){owner[next]=id;queue.push(next);}
        }
        parts.push_back(std::move(part));
    }
    QStringList details;bool allValid=true;
    for(std::size_t i=0;i<parts.size();++i){auto a=analyzeSource(parts[i]);allValid&=validateBooleanOperand(a).ok();details<<summary(i,a);result.components.push_back(std::move(a));}
    for(const auto& issue:whole.issues)if(issue.type==MeshIssueType::SelfIntersection){if(owner[issue.first]==owner[issue.second])++result.internalIntersectionPairs;else ++result.crossComponentIntersectionPairs;}
    details<<QStringLiteral("Original intersecting pairs: %1 within components, %2 between components.").arg(result.internalIntersectionPairs).arg(result.crossComponentIntersectionPairs);
    result.diagnostic=details.join(QLatin1Char(' '));
    if(!allValid){result.diagnostic+=QStringLiteral(" Individual component invalid; no Boolean union attempted.");return finish();}
    bool boundsContact=parts.size()==2;
    if(boundsContact){const auto& a=result.components[0].bounds;const auto& b=result.components[1].bounds;
        boundsContact=a.minimum.x<=b.maximum.x&&b.minimum.x<=a.maximum.x&&a.minimum.y<=b.maximum.y&&b.minimum.y<=a.maximum.y&&a.minimum.z<=b.maximum.z&&b.minimum.z<=a.maximum.z;}
    if(parts.size()!=2||!boundsContact||!result.crossComponentIntersectionPairs){result.diagnostic+=QStringLiteral(" No intersecting/touching closed-solid pair established; disconnected or merely nearby solids are not unioned.");return finish();}
    // No uninterruptible MCUT call in the application process. A single child
    // operation has a hard deadline and memory limit; no retry or bridge/cap.
    QTemporaryDir directory;
    if(!directory.isValid()){result.diagnostic+=QStringLiteral(" Cannot create bounded union workspace.");return finish();}
    QFile input(QDir(directory.path()).filePath(QStringLiteral("input.bin")));
    if(!input.open(QIODevice::WriteOnly)){result.diagnostic+=QStringLiteral(" Cannot write union operands.");return finish();}
    QDataStream out(&input);out.setVersion(QDataStream::Qt_6_0);out<<WireVersion;writeMesh(out,parts[0]);writeMesh(out,parts[1]);
    if(out.status()!=QDataStream::Ok||!input.flush()){result.diagnostic+=QStringLiteral(" Cannot flush union operands.");return finish();}input.close();
    QProcess process;process.setProgram(QCoreApplication::applicationFilePath());process.setArguments({QString::fromLatin1(WorkerArgument),directory.path()});
    process.setStandardOutputFile(QProcess::nullDevice());process.setStandardErrorFile(QProcess::nullDevice());
    process.start();
    if(!process.waitForFinished(DeadlineMilliseconds)){process.kill();process.waitForFinished(3000);result.diagnostic+=QStringLiteral(" Union worker failed to finish within 10 seconds; terminated, no override accepted.");return finish();}
    result.elapsedMilliseconds=timer.elapsed();
    if(process.exitStatus()!=QProcess::NormalExit||process.exitCode()!=0){result.diagnostic+=QStringLiteral(" Union worker failed (exit %1); possible resource limit or backend failure; no override accepted.").arg(process.exitCode());return finish();}
    QFile output(QDir(directory.path()).filePath(QStringLiteral("output.bin")));
    if(output.size()>2*1024*1024||!output.open(QIODevice::ReadOnly)){result.diagnostic+=QStringLiteral(" Union worker output missing or excessive.");return finish();}
    QDataStream in(&output);in.setVersion(QDataStream::Qt_6_0);quint32 version=0;bool accepted=false;QString diagnostic;in>>version>>accepted>>diagnostic;
    result.diagnostic+=QLatin1Char(' ')+diagnostic;
    if(version!=WireVersion||!accepted||!readMesh(in,&result.mesh,MaximumOutputFaces)||!in.atEnd()){result.diagnostic+=QStringLiteral(" Union failed; no override accepted.");return finish();}
    const auto final=validatePreparedMesh(analyzeSource(result.mesh));
    if(!final.ok()){result.diagnostic+=QStringLiteral(" Parent validation rejected union: ")+QString::fromStdString(final.message);return finish();}
    result.accepted=true;result.normalized=true;
    return finish();
}
} // namespace PrintGeometry
