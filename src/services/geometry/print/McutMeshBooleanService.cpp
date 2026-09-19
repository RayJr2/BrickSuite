#include "McutMeshBooleanService.h"
#include "LDrawPrintPreparationProfile.h"
#include "PrintMeshAnalysis.h"
#include <mcut/mcut.h>
#include <algorithm>
#include <vector>
namespace PrintGeometry { namespace {
std::vector<double> vertexArray(const PrintMesh&m){std::vector<double>v;v.reserve(m.vertices.size()*3);for(auto&p:m.vertices){v.push_back(p.x);v.push_back(p.y);v.push_back(p.z);}return v;}
std::vector<std::uint32_t> indexArray(const PrintMesh&m){std::vector<std::uint32_t>v;v.reserve(m.faces.size()*3);for(auto&f:m.faces)v.insert(v.end(),f.begin(),f.end());return v;}
bool dispatchBoolean(const PrintMesh&a,const PrintMesh&b,McFlags operationFlags,const char*operation,PrintMesh*out,std::string*error)
{
    const auto av=vertexArray(a);const auto bv=vertexArray(b);const auto ai=indexArray(a);const auto bi=indexArray(b);
    std::vector<std::uint32_t>as(a.faces.size(),3),bs(b.faces.size(),3);McContext context=MC_NULL_HANDLE;
    if(mcCreateContext(&context,0)!=MC_NO_ERROR){*error="MCUT context creation failed";return false;}
    const McFlags flags=MC_DISPATCH_VERTEX_ARRAY_DOUBLE|MC_DISPATCH_ENFORCE_GENERAL_POSITION|operationFlags;
    auto status=mcDispatch(context,flags,av.data(),ai.data(),as.data(),McUint32(a.vertices.size()),McUint32(a.faces.size()),bv.data(),bi.data(),bs.data(),McUint32(b.vertices.size()),McUint32(b.faces.size()));
    if(status!=MC_NO_ERROR){mcReleaseContext(context);*error=std::string("MCUT ")+operation+" dispatch failed ("+std::to_string(int(status))+")";return false;}
    McUint32 count=0;status=mcGetConnectedComponents(context,MC_CONNECTED_COMPONENT_TYPE_FRAGMENT,0,nullptr,&count);
    if(status!=MC_NO_ERROR||count==0){mcReleaseContext(context);*error=std::string("MCUT ")+operation+" returned no fragments";return false;}
    std::vector<McConnectedComponent>components(count,MC_NULL_HANDLE);mcGetConnectedComponents(context,MC_CONNECTED_COMPONENT_TYPE_FRAGMENT,count,components.data(),nullptr);PrintMesh merged;
    for(auto handle:components){McSize bytes=0;mcGetConnectedComponentData(context,handle,MC_CONNECTED_COMPONENT_DATA_VERTEX_DOUBLE,0,nullptr,&bytes);std::vector<double>vertices(bytes/sizeof(double));mcGetConnectedComponentData(context,handle,MC_CONNECTED_COMPONENT_DATA_VERTEX_DOUBLE,bytes,vertices.data(),nullptr);const auto base=std::uint32_t(merged.vertices.size());for(std::size_t i=0;i<vertices.size();i+=3)merged.vertices.push_back({vertices[i],vertices[i+1],vertices[i+2]});mcGetConnectedComponentData(context,handle,MC_CONNECTED_COMPONENT_DATA_FACE_TRIANGULATION,0,nullptr,&bytes);std::vector<McUint32>indices(bytes/sizeof(McUint32));mcGetConnectedComponentData(context,handle,MC_CONNECTED_COMPONENT_DATA_FACE_TRIANGULATION,bytes,indices.data(),nullptr);McPatchLocation patch=MC_PATCH_LOCATION_UNDEFINED;McFragmentLocation location=MC_FRAGMENT_LOCATION_UNDEFINED;mcGetConnectedComponentData(context,handle,MC_CONNECTED_COMPONENT_DATA_PATCH_LOCATION,sizeof(patch),&patch,nullptr);mcGetConnectedComponentData(context,handle,MC_CONNECTED_COMPONENT_DATA_FRAGMENT_LOCATION,sizeof(location),&location,nullptr);const bool reverse=location==MC_FRAGMENT_LOCATION_BELOW&&patch==MC_PATCH_LOCATION_OUTSIDE;for(std::size_t i=0;i+2<indices.size();i+=3){Face face{base+indices[i],base+indices[i+1],base+indices[i+2]};if(reverse)std::swap(face[1],face[2]);merged.faces.push_back(face);}}
    mcReleaseConnectedComponents(context,count,components.data());mcReleaseContext(context);*out=std::move(merged);return true;
}
}
bool McutMeshBooleanService::available(){McContext c=MC_NULL_HANDLE;const bool ok=mcCreateContext(&c,0)==MC_NO_ERROR;if(ok)mcReleaseContext(c);return ok;}
const char*McutMeshBooleanService::version(){return BRICKSUITE_MCUT_VERSION;}
MeshBooleanResult McutMeshBooleanService::unite(const PrintMesh&source,const PrintMesh&additive)
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
QString McutMeshBooleanService::versionIdentity()const{return QString::fromLatin1(version());}
}
