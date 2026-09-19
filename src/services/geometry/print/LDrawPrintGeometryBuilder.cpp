#include "LDrawPrintGeometryBuilder.h"

#include "LDrawPrintPreparationProfile.h"
#include "PrintMeshAnalysis.h"

#include <QElapsedTimer>
#include <QSet>

#include <algorithm>
#include <cmath>
#include <map>
#include <queue>
#include <set>
#include <tuple>

namespace PrintGeometry {
namespace {
const LDrawPrintPreparationProfile Profile;
const double WeldMm=Profile.seamWeldMillimetres;
const double PlanarityMm=Profile.planarityToleranceMillimetres;
const double ContactMm=Profile.boundaryContactToleranceMillimetres;
const double InterfaceIntrusionMm=Profile.ordinaryAttachmentIntrusionMillimetres;
const int MaxGroups=int(Profile.maximumSemanticGroups),MaxLoops=int(Profile.maximumBoundaryLoops),
    MaxLoopEdges=int(Profile.maximumLoopEdges),MaxOperands=int(Profile.maximumOperands);
using Edge=std::pair<std::uint32_t,std::uint32_t>;
struct Key{long long x,y,z;bool operator<(const Key&o)const{return std::tie(x,y,z)<std::tie(o.x,o.y,o.z);}};
struct FaceInfo{Face face;int sourceTriangle=-1;};
struct Component{PrintMesh mesh;QVector<int> sourceTriangles;std::vector<std::vector<std::uint32_t>> loops;};
Edge edge(std::uint32_t a,std::uint32_t b){return a<b?Edge{a,b}:Edge{b,a};}
Point convert(const QVector3D&p){return {0.4*double(p.x()),0.4*double(p.z()),-0.4*double(p.y())};}
Key key(const Point&p){return {std::llround(p.x/WeldMm),std::llround(p.y/WeldMm),std::llround(p.z/WeldMm)};}
PrintMesh append(const PrintMesh&a,const PrintMesh&b){PrintMesh o=a;auto offset=std::uint32_t(o.vertices.size());o.vertices.insert(o.vertices.end(),b.vertices.begin(),b.vertices.end());for(auto f:b.faces){for(auto&i:f)i+=offset;o.faces.push_back(f);}return o;}
Point sub(const Point&a,const Point&b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
Point cross(const Point&a,const Point&b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
double dot(const Point&a,const Point&b){return a.x*b.x+a.y*b.y+a.z*b.z;}
double length(const Point&a){return std::sqrt(dot(a,a));}
Point center(const PrintMesh&m,const std::vector<std::uint32_t>&loop){Point c;for(auto i:loop){c.x+=m.vertices[i].x;c.y+=m.vertices[i].y;c.z+=m.vertices[i].z;}const double n=double(loop.size());return {c.x/n,c.y/n,c.z/n};}
bool inBounds(const MeshBounds&b,const Point&p,double e){return b.valid&&p.x>=b.minimum.x-e&&p.x<=b.maximum.x+e&&p.y>=b.minimum.y-e&&p.y<=b.maximum.y+e&&p.z>=b.minimum.z-e&&p.z<=b.maximum.z+e;}
QString roleName(SemanticRole role)
{
    switch(role){
    case SemanticRole::PrimaryBody:return QStringLiteral("primary body");
    case SemanticRole::AdditiveAttachment:return QStringLiteral("additive attachment");
    case SemanticRole::HollowAdditiveAttachment:return QStringLiteral("hollow additive attachment");
    }
    return QStringLiteral("semantic");
}

bool planar(const PrintMesh&m,const std::vector<std::uint32_t>&loop)
{
    if(loop.size()<3)return false;Point n;for(std::size_t i=0;i<loop.size();++i){const auto&a=m.vertices[loop[i]],&b=m.vertices[loop[(i+1)%loop.size()]];n.x+=(a.y-b.y)*(a.z+b.z);n.y+=(a.z-b.z)*(a.x+b.x);n.z+=(a.x-b.x)*(a.y+b.y);}const double l=length(n);if(l<1e-12)return false;n={n.x/l,n.y/l,n.z/l};const auto origin=m.vertices[loop.front()];for(auto i:loop)if(std::abs(dot(sub(m.vertices[i],origin),n))>PlanarityMm)return false;return true;
}

PrintMesh welded(const LDrawGeometry::PartMesh&source,std::vector<FaceInfo>*infos)
{
    PrintMesh out;std::map<Key,std::uint32_t>ids;
    auto index=[&](const QVector3D&p){Point q=convert(p);auto k=key(q);auto it=ids.find(k);if(it!=ids.end())return it->second;auto id=std::uint32_t(out.vertices.size());ids[k]=id;out.vertices.push_back(q);return id;};
    for(int i=0;i<source.triangles.size();++i){const auto&t=source.triangles[i];Face f{index(t.a),index(t.b),index(t.c)};if(f[0]==f[1]||f[1]==f[2]||f[2]==f[0])continue;out.faces.push_back(f);infos->push_back({f,i});}return out;
}

std::vector<Component> components(const PrintMesh&all,const std::vector<FaceInfo>&infos)
{
    std::map<Edge,std::vector<int>>uses;for(int i=0;i<int(all.faces.size());++i){auto f=all.faces[i];uses[edge(f[0],f[1])].push_back(i);uses[edge(f[1],f[2])].push_back(i);uses[edge(f[2],f[0])].push_back(i);}std::vector<std::vector<int>>adj(all.faces.size());for(const auto&u:uses)for(int a:u.second)for(int b:u.second)if(a!=b)adj[a].push_back(b);
    std::vector<bool>seen(all.faces.size());std::vector<Component>out;
    for(int root=0;root<int(all.faces.size());++root)if(!seen[root]){std::queue<int>q;q.push(root);seen[root]=true;std::vector<int>faces;while(!q.empty()){int n=q.front();q.pop();faces.push_back(n);for(int x:adj[n])if(!seen[x]){seen[x]=true;q.push(x);}}Component c;std::map<std::uint32_t,std::uint32_t>remap;for(int fi:faces){Face f=all.faces[fi];for(auto&v:f){auto it=remap.find(v);if(it==remap.end()){auto id=std::uint32_t(c.mesh.vertices.size());remap[v]=id;c.mesh.vertices.push_back(all.vertices[v]);v=id;}else v=it->second;}c.mesh.faces.push_back(f);c.sourceTriangles.push_back(infos[fi].sourceTriangle);}
        std::map<Edge,int>counts;std::map<std::uint32_t,std::vector<std::uint32_t>>next;for(auto f:c.mesh.faces){std::array<std::pair<std::uint32_t,std::uint32_t>,3>es{{{f[0],f[1]},{f[1],f[2]},{f[2],f[0]}}};for(auto d:es)++counts[edge(d.first,d.second)];}for(auto f:c.mesh.faces){std::array<std::pair<std::uint32_t,std::uint32_t>,3>es{{{f[0],f[1]},{f[1],f[2]},{f[2],f[0]}}};for(auto d:es)if(counts[edge(d.first,d.second)]==1)next[d.first].push_back(d.second);}
        std::set<std::pair<std::uint32_t,std::uint32_t>>used;for(const auto&e:next)for(auto target:e.second)if(!used.count({e.first,target})){std::vector<std::uint32_t>loop;auto start=e.first,at=start,to=target;for(int guard=0;guard<MaxLoopEdges;++guard){used.insert({at,to});loop.push_back(at);at=to;if(at==start)break;auto it=next.find(at);if(it==next.end()||it->second.size()!=1){loop.clear();break;}to=it->second.front();}if(loop.size()>=3)c.loops.push_back(std::move(loop));}
        out.push_back(std::move(c));}
    return out;
}

void normalize(PrintMesh*m){auto a=analyzeSource(*m);if(a.signedVolume<0)for(auto&f:m->faces)std::swap(f[1],f[2]);}
std::vector<std::uint32_t> shiftedLoop(PrintMesh*m,const std::vector<std::uint32_t>&loop,const Point&direction)
{
    std::vector<std::uint32_t> shifted;for(auto id:loop){auto p=m->vertices[id];p.x+=direction.x*InterfaceIntrusionMm;p.y+=direction.y*InterfaceIntrusionMm;p.z+=direction.z*InterfaceIntrusionMm;shifted.push_back(std::uint32_t(m->vertices.size()));m->vertices.push_back(p);}for(std::size_t i=0;i<loop.size();++i){auto n=(i+1)%loop.size();m->faces.push_back({loop[n],loop[i],shifted[i]});m->faces.push_back({loop[n],shifted[i],shifted[n]});}return shifted;
}
bool closeSingle(Component c,PrintMesh*out,int*added)
{
    if(c.loops.size()!=1||!planar(c.mesh,c.loops[0]))return false;const auto loop=c.loops[0];const Point direction=boundaryAttachmentDirection(c.mesh,loop);if(length(direction)<0.5)return false;const auto shifted=shiftedLoop(&c.mesh,loop,direction);Point mid=center(c.mesh,shifted);auto midId=std::uint32_t(c.mesh.vertices.size());c.mesh.vertices.push_back(mid);for(std::size_t i=0;i<shifted.size();++i)c.mesh.faces.push_back({shifted[(i+1)%shifted.size()],shifted[i],midId});normalize(&c.mesh);if(!validateBooleanOperand(analyzeSource(c.mesh)).ok())return false;*added=int(loop.size()*3);*out=std::move(c.mesh);return true;
}
bool closeAnnularBoundary(Component c,PrintMesh*out,int*added)
{
    if(c.loops.size()!=2||!planar(c.mesh,c.loops[0])||!planar(c.mesh,c.loops[1]))return false;const auto&a=c.loops[0],&b=c.loops[1];if(a.size()!=b.size()||a.size()<3)return false;
    const Point ca=center(c.mesh,a),cb=center(c.mesh,b);if(length(sub(ca,cb))>ContactMm)return false;
    const Point direction=boundaryAttachmentDirection(c.mesh,a);if(length(direction)<0.5)return false;const auto sa=shiftedLoop(&c.mesh,a,direction),sb=shiftedLoop(&c.mesh,b,direction);
    for(int reverse=0;reverse<2;++reverse)for(std::size_t shift=0;shift<sb.size();++shift){PrintMesh candidate=c.mesh;auto bi=[&](std::size_t i){return sb[reverse?(shift+sb.size()-i)%sb.size():(shift+i)%sb.size()];};for(std::size_t i=0;i<sa.size();++i){auto an=sa[i],ax=sa[(i+1)%sa.size()],bn=bi(i),bx=bi(i+1);candidate.faces.push_back({ax,an,bn});candidate.faces.push_back({ax,bn,bx});}normalize(&candidate);if(validateBooleanOperand(analyzeSource(candidate)).ok()){*added=int(a.size()*6);*out=std::move(candidate);return true;}}
    return false;
}
}

LDrawSemanticOperandBuilder::Result LDrawSemanticOperandBuilder::build(const LDrawGeometry::LDrawLoadResult&loaded,const std::function<bool()>&cancellationRequested)
{
    Result r;if(!loaded.ok()||!loaded.sourceModel){r.diagnostics<<"No hierarchical LDraw source model is available.";return r;}
    QElapsedTimer total,phase;total.start();phase.start();std::vector<FaceInfo>infos;r.source=welded(loaded.mesh,&infos);r.sourceAnalysis=analyzeSource(r.source);r.sourceConversionAnalysisMs=phase.elapsed();
    if(!r.sourceAnalysis.finite||!r.sourceAnalysis.indicesValid||r.sourceAnalysis.resourceLimitExceeded){r.status=r.sourceAnalysis.resourceLimitExceeded?Status::ResourceLimitExceeded:Status::OperandValidationFailed;r.diagnostics<<"Source geometry is unsafe for semantic interpretation.";return r;}
    if(cancellationRequested&&cancellationRequested()){r.status=Status::Cancelled;r.diagnostics<<"Semantic preparation was cancelled after Source analysis.";return r;}
    phase.restart();auto groups=components(r.source,infos);r.semanticGroups=int(groups.size());
    r.approximateProvenanceBytes=loaded.sourceModel->files.size()*qsizetype(sizeof(LDrawGeometry::SourceFileRecord))+loaded.sourceModel->references.size()*qsizetype(sizeof(LDrawGeometry::ReferenceRecord))+loaded.sourceModel->surfaces.size()*qsizetype(sizeof(LDrawGeometry::SurfaceRecord));
    if(groups.empty()||groups.size()>MaxGroups){r.status=Status::ResourceLimitExceeded;r.diagnostics<<"Semantic group limit was exceeded.";return r;}int loopCount=0;for(const auto&g:groups)loopCount+=int(g.loops.size());if(loopCount>MaxLoops){r.status=Status::ResourceLimitExceeded;r.diagnostics<<"Boundary loop limit was exceeded.";return r;}
    int body=-1;double bodySpan=-1;for(int i=0;i<int(groups.size());++i){if(cancellationRequested&&cancellationRequested()){r.status=Status::Cancelled;r.diagnostics<<"Semantic preparation was cancelled during grouping.";return r;}auto a=analyzeSource(groups[i].mesh);if(!validatePreparedMesh(a,true).ok())continue;auto d=sub(a.bounds.maximum,a.bounds.minimum);double span=d.x*d.y*d.z;if(span>bodySpan){body=i;bodySpan=span;}}
    if(body<0){r.status=Status::OperandValidationFailed;r.diagnostics<<"No independently closed body/cavity operand was found.";return r;}const MeshBounds bodyBounds=analyzeSource(groups[body].mesh).bounds;
    auto certified=[&](const Component&g,QStringList*files){QSet<int>ids;for(int ti:g.sourceTriangles){if(ti<0||ti>=loaded.sourceModel->surfaces.size())return false;const auto&s=loaded.sourceModel->surfaces[ti];if(!s.certified)return false;ids.insert(s.fileId);}for(int id:ids){if(id<0||id>=loaded.sourceModel->files.size())return false;const auto&f=loaded.sourceModel->files[id];if(f.classification==LDrawGeometry::SourceClassification::Unknown)return false;files->append(f.relativePath);}files->sort();return true;};
    QVector<int>order;order<<body;for(int loopCount:{1,2})for(int i=0;i<int(groups.size());++i)if(i!=body&&int(groups[i].loops.size())==loopCount)order<<i;
    for(int index:order){if(cancellationRequested&&cancellationRequested()){r.status=Status::Cancelled;r.diagnostics<<"Semantic preparation was cancelled between operands.";return r;}const auto&g=groups[index];SemanticOperand op;op.sourceMesh=g.mesh;op.confidence=SemanticConfidence::HighConfidence;QStringList files;if(!certified(g,&files)){r.status=Status::UncertifiedGeometry;r.diagnostics<<QString("Group %1 is uncertified or not official.").arg(index);return r;}op.sourceFiles=files;
        if(index==body){op.role=SemanticRole::PrimaryBody;op.feature=SemanticFeature::BodyOrCavity;op.closedMesh=g.mesh;normalize(&op.closedMesh);op.analysis=analyzeSource(op.closedMesh);}
        else {bool contacts=true;for(const auto&loop:g.loops)contacts=contacts&&inBounds(bodyBounds,center(g.mesh,loop),ContactMm);if(!contacts){r.status=Status::AmbiguousBoundary;r.diagnostics<<QString("Group %1 does not contact the body at its boundary.").arg(index);return r;}
            if(g.loops.size()==1){op.role=SemanticRole::AdditiveAttachment;op.feature=SemanticFeature::Stud;if(!closeSingle(g,&op.closedMesh,&op.closureTriangles)){r.status=Status::OperandClosureFailed;r.diagnostics<<QString("Single-loop closure failed for group %1.").arg(index);return r;}}
            else if(g.loops.size()==2){op.role=SemanticRole::HollowAdditiveAttachment;op.feature=SemanticFeature::Tube;if(!closeAnnularBoundary(g,&op.closedMesh,&op.closureTriangles)){r.status=Status::OperandClosureFailed;r.diagnostics<<QString("Nested-loop annular closure failed for group %1.").arg(index);return r;}}
            else {r.status=Status::UnsupportedBoundaryTopology;r.diagnostics<<QString("Group %1 has %2 boundary loops.").arg(index).arg(g.loops.size());return r;}op.analysis=analyzeSource(op.closedMesh);}
        if(!validateBooleanOperand(op.analysis).ok()){r.status=Status::OperandValidationFailed;r.diagnostics<<QString("The %1 operand failed Boolean-operand validation (components=%2, nonManifoldVertices=%3, selfIntersections=%4).").arg(roleName(op.role)).arg(op.analysis.connectedComponents).arg(op.analysis.nonManifoldVertices).arg(op.analysis.selfIntersections);return r;}r.closureTriangles+=op.closureTriangles;r.operands.push_back(std::move(op));}
    if(r.operands.size()>MaxOperands){r.status=Status::ResourceLimitExceeded;r.diagnostics<<"Semantic operand limit was exceeded.";return r;}r.semanticGenerationMs=phase.elapsed();r.status=Status::Ready;
    r.diagnostics<<QString("groups=%1 loops=%2 operands=%3 closureTriangles=%4 provenanceBytes~%5 semanticMs=%6 elapsedMs=%7")
        .arg(r.semanticGroups).arg(loopCount).arg(r.operands.size()).arg(r.closureTriangles).arg(r.approximateProvenanceBytes).arg(r.semanticGenerationMs).arg(total.elapsed());return r;
}
} // namespace PrintGeometry
