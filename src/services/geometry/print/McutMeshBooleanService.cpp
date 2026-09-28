#include "McutMeshBooleanService.h"
#include "LDrawPrintPreparationProfile.h"
#include "PrintMeshAnalysis.h"
#include <mcut/mcut.h>
#include <algorithm>
#include <vector>
#include "McutWorkerProtocol.h"
#include "BoundedGeometryWorker.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>
namespace PrintGeometry { namespace {
std::vector<double> vertexArray(const PrintMesh&m){std::vector<double>v;v.reserve(m.vertices.size()*3);for(auto&p:m.vertices){v.push_back(p.x);v.push_back(p.y);v.push_back(p.z);}return v;}
std::vector<std::uint32_t> indexArray(const PrintMesh&m){std::vector<std::uint32_t>v;v.reserve(m.faces.size()*3);for(auto&f:m.faces)v.insert(v.end(),f.begin(),f.end());return v;}
bool dispatchBoolean(const PrintMesh&a,const PrintMesh&b,McFlags operationFlags,const char*operation,PrintMesh*out,std::string*error)
{
    const auto av=vertexArray(a);const auto bv=vertexArray(b);const auto ai=indexArray(a);const auto bi=indexArray(b);
    std::vector<std::uint32_t>as(a.faces.size(),3),bs(b.faces.size(),3);McContext context=MC_NULL_HANDLE;
    if(mcCreateContext(&context,0)!=MC_NO_ERROR){*error="MCUT context creation failed";return false;}
    // Context destruction also releases its connected components on every exit.
    struct ContextGuard {McContext value;~ContextGuard(){mcReleaseContext(value);}} guard{context};
    const auto checked=[&](McResult status,const char* stage){
        if(status==MC_NO_ERROR)return true;
        *error=std::string("MCUT ")+operation+" "+stage+" failed ("+std::to_string(int(status))+")";
        return false;
    };
    const McFlags flags=MC_DISPATCH_VERTEX_ARRAY_DOUBLE|MC_DISPATCH_ENFORCE_GENERAL_POSITION|operationFlags;
    if(!checked(mcDispatch(context,flags,av.data(),ai.data(),as.data(),McUint32(a.vertices.size()),McUint32(a.faces.size()),
        bv.data(),bi.data(),bs.data(),McUint32(b.vertices.size()),McUint32(b.faces.size())),"dispatch"))return false;
    McUint32 count=0;
    if(!checked(mcGetConnectedComponents(context,MC_CONNECTED_COMPONENT_TYPE_FRAGMENT,0,nullptr,&count),"fragment count"))return false;
    if(!count){*error="MCUT returned no fragments";return false;}
    std::vector<McConnectedComponent> components(count,MC_NULL_HANDLE);
    if(!checked(mcGetConnectedComponents(context,MC_CONNECTED_COMPONENT_TYPE_FRAGMENT,count,components.data(),nullptr),"fragments"))return false;
    PrintMesh merged;const LDrawPrintPreparationProfile profile;
    for(auto handle:components){
        McSize bytes=0;
        if(!checked(mcGetConnectedComponentData(context,handle,MC_CONNECTED_COMPONENT_DATA_VERTEX_DOUBLE,0,nullptr,&bytes),"vertex size query"))return false;
        if(!bytes||bytes%(3*sizeof(double))||bytes/(3*sizeof(double))>profile.maximumVertices-merged.vertices.size()){
            *error="MCUT returned invalid or excessive vertex data";return false;
        }
        std::vector<double> vertices(bytes/sizeof(double));
        if(!checked(mcGetConnectedComponentData(context,handle,MC_CONNECTED_COMPONENT_DATA_VERTEX_DOUBLE,bytes,vertices.data(),nullptr),"vertices"))return false;
        const auto base=std::uint32_t(merged.vertices.size());
        for(std::size_t i=0;i<vertices.size();i+=3)merged.vertices.push_back({vertices[i],vertices[i+1],vertices[i+2]});
        // A failed size query can leave MCUT's CDT cache partially populated.
        // Never call the copy query after failure: Debug asserts on that cache.
        if(!checked(mcGetConnectedComponentData(context,handle,MC_CONNECTED_COMPONENT_DATA_FACE_TRIANGULATION,0,nullptr,&bytes),"face triangulation size query"))return false;
        if(!bytes||bytes%(3*sizeof(McUint32))||bytes/(3*sizeof(McUint32))>profile.maximumFaces-merged.faces.size()){
            *error="MCUT returned invalid or excessive triangulation data";return false;
        }
        std::vector<McUint32> indices(bytes/sizeof(McUint32));
        if(!checked(mcGetConnectedComponentData(context,handle,MC_CONNECTED_COMPONENT_DATA_FACE_TRIANGULATION,bytes,indices.data(),nullptr),"face triangulation copy"))return false;
        McPatchLocation patch=MC_PATCH_LOCATION_UNDEFINED;McFragmentLocation location=MC_FRAGMENT_LOCATION_UNDEFINED;
        if(!checked(mcGetConnectedComponentData(context,handle,MC_CONNECTED_COMPONENT_DATA_PATCH_LOCATION,sizeof(patch),&patch,nullptr),"patch location")||
           !checked(mcGetConnectedComponentData(context,handle,MC_CONNECTED_COMPONENT_DATA_FRAGMENT_LOCATION,sizeof(location),&location,nullptr),"fragment location"))return false;
        const bool reverse=location==MC_FRAGMENT_LOCATION_BELOW&&patch==MC_PATCH_LOCATION_OUTSIDE;
        for(std::size_t i=0;i<indices.size();i+=3){
            if(indices[i]>=vertices.size()/3||indices[i+1]>=vertices.size()/3||indices[i+2]>=vertices.size()/3){*error="MCUT returned invalid triangle indices";return false;}
            Face face{base+indices[i],base+indices[i+1],base+indices[i+2]};
            if(reverse)std::swap(face[1],face[2]);merged.faces.push_back(face);
        }
    }
    *out=std::move(merged);return true;
}
}
bool McutMeshBooleanService::available(){McContext c=MC_NULL_HANDLE;const bool ok=mcCreateContext(&c,0)==MC_NO_ERROR;if(ok)mcReleaseContext(c);return ok;}
const char*McutMeshBooleanService::version(){return BRICKSUITE_MCUT_VERSION;}
MeshBooleanResult McutMeshBooleanService::unite(const PrintMesh&source,const PrintMesh&additive)
{
    if(m_isolated)return runWorker(source,additive,false);
    return uniteInWorker(source,additive);
}
MeshBooleanResult McutMeshBooleanService::uniteInWorker(const PrintMesh&source,const PrintMesh&additive)
{
    MeshBooleanResult r;r.sourceAnalysis=analyzeSource(source);r.additiveAnalysis=analyzeSource(additive);const LDrawPrintPreparationProfile profile;
    if(source.vertices.size()+additive.vertices.size()>profile.maximumVertices||source.faces.size()+additive.faces.size()>profile.maximumFaces){r.error=MeshBooleanError::ResourceLimitExceeded;r.message="Boolean operands exceed resource limits";return r;}
    auto valid=validateBooleanOperand(r.sourceAnalysis);if(!valid.ok()){r.error=MeshBooleanError::InvalidSource;r.message="Boolean source: "+valid.message;return r;}
    valid=validateBooleanOperand(r.additiveAnalysis);if(!valid.ok()){r.error=MeshBooleanError::InvalidAdditive;r.message="Boolean additive: "+valid.message;return r;}
    if(!dispatchBoolean(source,additive,MC_DISPATCH_FILTER_FRAGMENT_SEALING_OUTSIDE|MC_DISPATCH_FILTER_FRAGMENT_LOCATION_ABOVE,"union",&r.mesh,&r.message)){r.error=MeshBooleanError::BackendFailure;return r;}
    r.resultAnalysis=analyzeSource(r.mesh);valid=validateBooleanOperand(r.resultAnalysis);
    if(!valid.ok()&&valid.error==MeshValidationError::NonPositiveVolume){for(auto&f:r.mesh.faces)std::swap(f[1],f[2]);r.resultAnalysis=analyzeSource(r.mesh);valid=validateBooleanOperand(r.resultAnalysis);}
    if(!valid.ok()){r.error=MeshBooleanError::InvalidResult;r.message="MCUT result: "+valid.message;return r;}
    r.error=MeshBooleanError::None;r.message="MCUT Boolean union independently validated";return r;
}
MeshBooleanResult McutMeshBooleanService::subtract(const PrintMesh&source,const PrintMesh&passage)
{
    if(m_isolated)return runWorker(source,passage,true);
    return subtractInWorker(source,passage);
}
MeshBooleanResult McutMeshBooleanService::subtractInWorker(const PrintMesh&source,const PrintMesh&passage)
{
    MeshBooleanResult r;r.sourceAnalysis=analyzeSource(source);r.additiveAnalysis=analyzeSource(passage);const LDrawPrintPreparationProfile profile;
    if(source.vertices.size()+passage.vertices.size()>profile.maximumVertices||source.faces.size()+passage.faces.size()>profile.maximumFaces){r.error=MeshBooleanError::ResourceLimitExceeded;r.message="Boolean operands exceed resource limits";return r;}
    auto valid=validateBooleanOperand(r.sourceAnalysis);if(!valid.ok()){r.error=MeshBooleanError::InvalidSource;r.message="Boolean source: "+valid.message;return r;}
    valid=validateBooleanOperand(r.additiveAnalysis);if(!valid.ok()){r.error=MeshBooleanError::InvalidAdditive;r.message="Boolean passage: "+valid.message;return r;}
    if(!dispatchBoolean(source,passage,MC_DISPATCH_FILTER_FRAGMENT_SEALING_INSIDE|MC_DISPATCH_FILTER_FRAGMENT_LOCATION_ABOVE,"difference",&r.mesh,&r.message)){r.error=MeshBooleanError::BackendFailure;return r;}
    r.resultAnalysis=analyzeSource(r.mesh);valid=validateBooleanOperand(r.resultAnalysis);
    if(!valid.ok()&&valid.error==MeshValidationError::NonPositiveVolume){for(auto&f:r.mesh.faces)std::swap(f[1],f[2]);r.resultAnalysis=analyzeSource(r.mesh);valid=validateBooleanOperand(r.resultAnalysis);}
    if(!valid.ok()){r.error=MeshBooleanError::InvalidResult;r.message="MCUT result: "+valid.message;return r;}
    r.error=MeshBooleanError::None;r.message="MCUT Boolean difference independently validated";return r;
}
MeshBooleanResult McutMeshBooleanService::runWorker(const PrintMesh& a,const PrintMesh& b,bool subtract) const
{
    using namespace McutWorkerProtocol;
    MeshBooleanResult result;
    const auto fail=[&](MeshBooleanError error,const QString& message){result.error=error;result.message=message.toStdString();return result;};
    if(a.faces.size()+b.faces.size()>MaximumFaces||a.vertices.size()+b.vertices.size()>3*MaximumFaces)
        return fail(MeshBooleanError::ResourceLimitExceeded,QStringLiteral("MCUT worker input exceeds 50,000 faces / 150,000 vertices; no Boolean started."));
    QTemporaryDir directory;
    if(!directory.isValid())return fail(MeshBooleanError::BackendFailure,QStringLiteral("Cannot create MCUT worker workspace."));
    QFile input(QDir(directory.path()).filePath(QStringLiteral("input.bin")));
    if(!input.open(QIODevice::WriteOnly))return fail(MeshBooleanError::BackendFailure,QStringLiteral("Cannot write MCUT operands."));
    QDataStream out(&input);out.setVersion(QDataStream::Qt_6_0);
    out<<Version<<subtract;writeMesh(out,a);writeMesh(out,b);
    if(out.status()!=QDataStream::Ok||!input.flush())return fail(MeshBooleanError::BackendFailure,QStringLiteral("Cannot flush MCUT operands."));
    input.close();
    QString executable=m_workerExecutable;
    if(executable.isEmpty())executable=QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("BrickSuiteMeshBooleanWorker")
#ifdef Q_OS_WIN
        +QStringLiteral(".exe")
#endif
    );
    QProcess process;process.setProgram(executable);process.setArguments({directory.path()});
    process.setStandardOutputFile(QProcess::nullDevice());process.setStandardErrorFile(QProcess::nullDevice());
    const auto outcome=BoundedGeometryWorker::run(process,std::clamp(m_deadlineMilliseconds,1,DeadlineMilliseconds),m_memoryBudgetBytes);
    if(outcome==BoundedGeometryWorker::Outcome::MemoryLimit)
        return fail(MeshBooleanError::ResourceLimitExceeded,QStringLiteral("MCUT worker exceeded its memory budget; terminated without a result."));
    if(outcome==BoundedGeometryWorker::Outcome::TimedOut)
        return fail(MeshBooleanError::ResourceLimitExceeded,QStringLiteral("MCUT worker exceeded its bounded deadline; terminated without a result."));
    if(outcome!=BoundedGeometryWorker::Outcome::Finished)
        return fail(MeshBooleanError::BackendFailure,QStringLiteral("MCUT worker unavailable or memory supervision failed: ")+process.errorString());
    if(process.exitStatus()!=QProcess::NormalExit||process.exitCode()!=0)
        return fail(MeshBooleanError::BackendFailure,QStringLiteral("MCUT worker failed (exit %1); backend crash or resource limit contained; no result accepted.").arg(process.exitCode()));
    QFile output(QDir(directory.path()).filePath(QStringLiteral("output.bin")));
    if(output.size()>MaximumWireBytes||!output.open(QIODevice::ReadOnly))return fail(MeshBooleanError::BackendFailure,QStringLiteral("MCUT worker result missing or excessive."));
    QDataStream in(&output);in.setVersion(QDataStream::Qt_6_0);
    quint32 version=0;qint32 error=0;QString message;in>>version>>error>>message;
    if(in.status()!=QDataStream::Ok||version!=Version||error<0||error>qint32(MeshBooleanError::InvalidResult))
        return fail(MeshBooleanError::BackendFailure,QStringLiteral("Malformed MCUT worker result."));
    if(error!=qint32(MeshBooleanError::None))return fail(MeshBooleanError(error),message);
    if(!readMesh(in,result.mesh,MaximumOutputFaces)||!in.atEnd())return fail(MeshBooleanError::BackendFailure,QStringLiteral("Malformed MCUT worker mesh."));
    result.sourceAnalysis=analyzeSource(a);result.additiveAnalysis=analyzeSource(b);
    result.resultAnalysis=analyzeSource(result.mesh);
    const auto valid=validateBooleanOperand(result.resultAnalysis);
    if(!valid.ok())return fail(MeshBooleanError::InvalidResult,QStringLiteral("Parent rejected MCUT result: ")+QString::fromStdString(valid.message));
    result.error=MeshBooleanError::None;result.message=message.toStdString();return result;
}
QString McutMeshBooleanService::versionIdentity()const{return QString::fromLatin1(version());}
}
