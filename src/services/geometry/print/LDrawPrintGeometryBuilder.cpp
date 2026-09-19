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
    case SemanticRole::SubtractivePassage:return QStringLiteral("subtractive passage");
    case SemanticRole::AdditiveAttachment:return QStringLiteral("additive attachment");
    case SemanticRole::HollowAdditiveAttachment:return QStringLiteral("hollow additive attachment");
    }
    return QStringLiteral("semantic");
}

bool planar(const PrintMesh&m,const std::vector<std::uint32_t>&loop)
{
    if(loop.size()<3)return false;Point n;for(std::size_t i=0;i<loop.size();++i){const auto&a=m.vertices[loop[i]],&b=m.vertices[loop[(i+1)%loop.size()]];n.x+=(a.y-b.y)*(a.z+b.z);n.y+=(a.z-b.z)*(a.x+b.x);n.z+=(a.x-b.x)*(a.y+b.y);}const double l=length(n);if(l<1e-12)return false;n={n.x/l,n.y/l,n.z/l};const auto origin=m.vertices[loop.front()];for(auto i:loop)if(std::abs(dot(sub(m.vertices[i],origin),n))>PlanarityMm)return false;return true;
}

std::vector<Component> components(const PrintMesh&all,const std::vector<FaceInfo>&infos);
void normalize(PrintMesh*m);

bool repairCollinearSeam(Component*c,const std::vector<std::uint32_t>&loop)
{
    if(loop.size()!=3)return false;
    int a=0,b=1;double longest=-1.0;
    for(int i=0;i<3;++i)for(int j=i+1;j<3;++j){const double d=length(sub(c->mesh.vertices[loop[i]],c->mesh.vertices[loop[j]]));if(d>longest){longest=d;a=i;b=j;}}
    const int middle=3-a-b;const auto pa=c->mesh.vertices[loop[a]],pb=c->mesh.vertices[loop[b]],pm=c->mesh.vertices[loop[middle]];
    if(longest<=WeldMm||length(cross(sub(pm,pa),sub(pb,pa)))>WeldMm*longest)return false;
    if(std::abs(length(sub(pm,pa))+length(sub(pb,pm))-longest)>WeldMm)return false;
    const auto u=loop[a],v=loop[b],mid=loop[middle];
    for(std::size_t fi=0;fi<c->mesh.faces.size();++fi){const Face f=c->mesh.faces[fi];for(int k=0;k<3;++k){const auto x=f[k],y=f[(k+1)%3],z=f[(k+2)%3];if((x==u&&y==v)||(x==v&&y==u)){c->mesh.faces[fi]={x,mid,z};c->mesh.faces.push_back({mid,y,z});return true;}}}
    return false;
}

void repairCollinearSeams(Component*c)
{
    for(int pass=0;pass<8;++pass){bool changed=false;for(const auto&loop:c->loops)if(repairCollinearSeam(c,loop)){changed=true;break;}if(!changed)return;
        std::vector<FaceInfo>infos(c->mesh.faces.size());for(std::size_t i=0;i<infos.size();++i)infos[i]={c->mesh.faces[i],-1};auto refreshed=components(c->mesh,infos);if(refreshed.size()!=1)return;const auto provenance=c->sourceTriangles;c->mesh=std::move(refreshed[0].mesh);c->loops=std::move(refreshed[0].loops);c->sourceTriangles=provenance;}
}

bool circularPassage(const Component&c,Point*axisOut)
{
    if(c.loops.size()!=2||c.loops[0].size()!=c.loops[1].size()||c.loops[0].size()<4)return false;
    const Point c0=center(c.mesh,c.loops[0]),c1=center(c.mesh,c.loops[1]);Point axis=sub(c1,c0);const double separation=length(axis);if(separation<ContactMm)return false;axis={axis.x/separation,axis.y/separation,axis.z/separation};
    for(int li=0;li<2;++li){const Point origin=li?c1:c0;for(auto id:c.loops[li])if(std::abs(dot(sub(c.mesh.vertices[id],origin),axis))>PlanarityMm)return false;}
    std::vector<double>candidateRadii;for(const auto&p:c.mesh.vertices){const double radius=length(cross(sub(p,c0),axis));if(radius>ContactMm&&std::none_of(candidateRadii.cbegin(),candidateRadii.cend(),[radius](double existing){return std::abs(existing-radius)<=ContactMm;}))candidateRadii.push_back(radius);}
    bool cylindricalWall=false;
    for(double candidateRadius:candidateRadii){Point basis;bool haveBasis=false;double axialMinimum=1e100,axialMaximum=-1e100;int radialPoints=0;bool quadrants[4]={false,false,false,false};
        for(const auto&p:c.mesh.vertices){const Point delta=sub(p,c0);const double axial=dot(delta,axis);const Point radial=sub(delta,{axis.x*axial,axis.y*axial,axis.z*axial});if(std::abs(length(radial)-candidateRadius)>ContactMm)continue;
            if(!haveBasis){basis={radial.x/candidateRadius,radial.y/candidateRadius,radial.z/candidateRadius};haveBasis=true;}
            const Point perpendicular=cross(axis,basis);const double x=dot(radial,basis),y=dot(radial,perpendicular);quadrants[(x<0?2:0)+(y<0?1:0)]=true;axialMinimum=std::min(axialMinimum,axial);axialMaximum=std::max(axialMaximum,axial);++radialPoints;}
        if(radialPoints>=8&&axialMinimum<=ContactMm&&axialMaximum>=separation-ContactMm&&std::all_of(std::begin(quadrants),std::end(quadrants),[](bool value){return value;})){cylindricalWall=true;break;}}
    if(!cylindricalWall)return false;
    *axisOut=axis;return true;
}

bool closeRoundPassage(const Component&c,const Point&axis,PrintMesh*out,int*added)
{
    const Point c0=center(c.mesh,c.loops[0]),c1=center(c.mesh,c.loops[1]);const double separation=length(sub(c1,c0));
    struct Band{double radius=0.0,minimum=1e100,maximum=-1e100;};std::vector<Band>bands;
    for(const auto&p:c.mesh.vertices){const Point delta=sub(p,c0);const double axial=dot(delta,axis),radius=length(cross(delta,axis));if(radius<=ContactMm)continue;auto it=std::find_if(bands.begin(),bands.end(),[radius](const Band&b){return std::abs(b.radius-radius)<=ContactMm;});if(it==bands.end()){bands.push_back({radius,axial,axial});}else{it->minimum=std::min(it->minimum,axial);it->maximum=std::max(it->maximum,axial);}}
    std::sort(bands.begin(),bands.end(),[](const Band&a,const Band&b){return a.radius<b.radius;});if(bands.empty())return false;
    const Band*inner=nullptr,*outer=nullptr;for(const auto&band:bands){if(!inner&&band.minimum>ContactMm&&band.maximum<separation-ContactMm)inner=&band;if(inner&&band.radius>inner->radius+ContactMm&&band.minimum<=ContactMm&&band.maximum>=separation-ContactMm){outer=&band;break;}}
    if(!outer)outer=&bands.front();if(outer->minimum>ContactMm||outer->maximum<separation-ContactMm)return false;
    const Band*basisBand=inner?inner:outer;Point basis;bool found=false;for(const auto&p:c.mesh.vertices){const Point delta=sub(p,c0);const double axial=dot(delta,axis);const Point radial=sub(delta,{axis.x*axial,axis.y*axial,axis.z*axial});if(std::abs(length(radial)-basisBand->radius)<=ContactMm){basis={radial.x/basisBand->radius,radial.y/basisBand->radius,radial.z/basisBand->radius};found=true;break;}}if(!found)return false;const Point perpendicular=cross(axis,basis);
    std::vector<std::pair<double,double>>sections;if(inner&&inner->minimum<inner->maximum)sections={{-InterfaceIntrusionMm,outer->radius},{inner->minimum,outer->radius},{inner->minimum,inner->radius},{inner->maximum,inner->radius},{inner->maximum,outer->radius},{separation+InterfaceIntrusionMm,outer->radius}};else sections={{-InterfaceIntrusionMm,outer->radius},{separation+InterfaceIntrusionMm,outer->radius}};constexpr int Segments=16;PrintMesh mesh;
    for(const auto&section:sections)for(int i=0;i<Segments;++i){const double angle=2.0*3.14159265358979323846*double(i)/double(Segments),cs=std::cos(angle),sn=std::sin(angle);mesh.vertices.push_back({c0.x+axis.x*section.first+(basis.x*cs+perpendicular.x*sn)*section.second,c0.y+axis.y*section.first+(basis.y*cs+perpendicular.y*sn)*section.second,c0.z+axis.z*section.first+(basis.z*cs+perpendicular.z*sn)*section.second});}
    for(std::size_t s=0;s+1<sections.size();++s)for(int i=0;i<Segments;++i){const int n=(i+1)%Segments;const auto a=std::uint32_t(s*Segments+i),b=std::uint32_t(s*Segments+n),d=std::uint32_t((s+1)*Segments+i),e=std::uint32_t((s+1)*Segments+n);mesh.faces.push_back({a,b,d});mesh.faces.push_back({b,e,d});}
    const auto firstCenter=std::uint32_t(mesh.vertices.size());mesh.vertices.push_back({c0.x-axis.x*InterfaceIntrusionMm,c0.y-axis.y*InterfaceIntrusionMm,c0.z-axis.z*InterfaceIntrusionMm});const auto lastCenter=std::uint32_t(mesh.vertices.size());mesh.vertices.push_back({c0.x+axis.x*(separation+InterfaceIntrusionMm),c0.y+axis.y*(separation+InterfaceIntrusionMm),c0.z+axis.z*(separation+InterfaceIntrusionMm)});
    const auto lastBase=std::uint32_t((sections.size()-1)*Segments);for(int i=0;i<Segments;++i){const auto n=(i+1)%Segments;mesh.faces.push_back({firstCenter,std::uint32_t(n),std::uint32_t(i)});mesh.faces.push_back({lastCenter,lastBase+std::uint32_t(i),lastBase+std::uint32_t(n)});}normalize(&mesh);if(!validateBooleanOperand(analyzeSource(mesh)).ok())return false;*added=int(mesh.faces.size());*out=std::move(mesh);return true;
}

bool opensOnOppositeBounds(const Component&passage,const MeshBounds&bounds,const Point&axis)
{
    const Point a=center(passage.mesh,passage.loops[0]),b=center(passage.mesh,passage.loops[1]);
    const double values[3]={std::abs(axis.x),std::abs(axis.y),std::abs(axis.z)};const int dimension=int(std::max_element(values,values+3)-values);
    const auto coordinate=[dimension](const Point&p){return dimension==0?p.x:dimension==1?p.y:p.z;};
    const double minimum=coordinate(bounds.minimum),maximum=coordinate(bounds.maximum),ca=coordinate(a),cb=coordinate(b);
    return (std::abs(ca-minimum)<=ContactMm&&std::abs(cb-maximum)<=ContactMm)||(std::abs(cb-minimum)<=ContactMm&&std::abs(ca-maximum)<=ContactMm);
}

bool hasMatchingOpenings(const Component&shell,const Component&passage)
{
    for(const auto&opening:passage.loops){const Point expected=center(passage.mesh,opening);bool found=false;for(const auto&candidate:shell.loops)if(length(sub(center(shell.mesh,candidate),expected))<=ContactMm){found=true;break;}if(!found)return false;}return true;
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
bool closeSingle(Component c,PrintMesh*out,int*added,bool allowNonPlanar=false)
{
    if(c.loops.size()!=1||(!allowNonPlanar&&!planar(c.mesh,c.loops[0])))return false;const auto loop=c.loops[0];const Point direction=boundaryAttachmentDirection(c.mesh,loop);if(length(direction)<0.5)return false;const auto shifted=shiftedLoop(&c.mesh,loop,direction);Point mid=center(c.mesh,shifted);auto midId=std::uint32_t(c.mesh.vertices.size());c.mesh.vertices.push_back(mid);for(std::size_t i=0;i<shifted.size();++i)c.mesh.faces.push_back({shifted[(i+1)%shifted.size()],shifted[i],midId});normalize(&c.mesh);if(!validateBooleanOperand(analyzeSource(c.mesh)).ok())return false;*added=int(loop.size()*3);*out=std::move(c.mesh);return true;
}
bool closeAnnularBoundary(Component c,PrintMesh*out,int*added)
{
    if(c.loops.size()!=2||!planar(c.mesh,c.loops[0])||!planar(c.mesh,c.loops[1]))return false;const auto&a=c.loops[0],&b=c.loops[1];if(a.size()!=b.size()||a.size()<3)return false;
    const Point ca=center(c.mesh,a),cb=center(c.mesh,b);if(length(sub(ca,cb))>ContactMm)return false;
    const Point direction=boundaryAttachmentDirection(c.mesh,a);if(length(direction)<0.5)return false;const auto sa=shiftedLoop(&c.mesh,a,direction),sb=shiftedLoop(&c.mesh,b,direction);
    for(int reverse=0;reverse<2;++reverse)for(std::size_t shift=0;shift<sb.size();++shift){PrintMesh candidate=c.mesh;auto bi=[&](std::size_t i){return sb[reverse?(shift+sb.size()-i)%sb.size():(shift+i)%sb.size()];};for(std::size_t i=0;i<sa.size();++i){auto an=sa[i],ax=sa[(i+1)%sa.size()],bn=bi(i),bx=bi(i+1);candidate.faces.push_back({ax,an,bn});candidate.faces.push_back({ax,bn,bx});}normalize(&candidate);if(validateBooleanOperand(analyzeSource(candidate)).ok()){*added=int(a.size()*6);*out=std::move(candidate);return true;}}
    return false;
}
bool closePlanarLoops(Component c,PrintMesh*out,int*added)
{
    int count=0;
    for(const auto&loop:c.loops){
        if(!planar(c.mesh,loop))return false;
        const Point mid=center(c.mesh,loop);
        const auto midId=std::uint32_t(c.mesh.vertices.size());
        c.mesh.vertices.push_back(mid);
        for(std::size_t i=0;i<loop.size();++i)c.mesh.faces.push_back({loop[(i+1)%loop.size()],loop[i],midId});
        count+=int(loop.size());
    }
    normalize(&c.mesh);
    *added=count;*out=std::move(c.mesh);return true;
}
}

LDrawSemanticOperandBuilder::Result LDrawSemanticOperandBuilder::build(const LDrawGeometry::LDrawLoadResult&loaded,const std::function<bool()>&cancellationRequested)
{
    Result r;if(!loaded.ok()||!loaded.sourceModel){r.diagnostics<<"No hierarchical LDraw source model is available.";return r;}const auto stitched=LDrawCertifiedInterfaceStitcher::stitch(loaded);const auto&effective=stitched.loadResult;r.stitchDiagnostics=stitched.diagnostics;r.diagnostics.append(stitched.diagnostics.messages);
    QElapsedTimer total,phase;total.start();phase.start();std::vector<FaceInfo>infos;r.source=welded(effective.mesh,&infos);r.sourceAnalysis=analyzeSource(r.source);r.sourceConversionAnalysisMs=phase.elapsed();
    if(!r.sourceAnalysis.finite||!r.sourceAnalysis.indicesValid||r.sourceAnalysis.resourceLimitExceeded){r.status=r.sourceAnalysis.resourceLimitExceeded?Status::ResourceLimitExceeded:Status::OperandValidationFailed;r.diagnostics<<"Source geometry is unsafe for semantic interpretation.";return r;}
    if(cancellationRequested&&cancellationRequested()){r.status=Status::Cancelled;r.diagnostics<<"Semantic preparation was cancelled after Source analysis.";return r;}
    phase.restart();auto groups=components(r.source,infos);r.semanticGroups=int(groups.size());
    r.approximateProvenanceBytes=effective.sourceModel->files.size()*qsizetype(sizeof(LDrawGeometry::SourceFileRecord))+effective.sourceModel->references.size()*qsizetype(sizeof(LDrawGeometry::ReferenceRecord))+effective.sourceModel->surfaces.size()*qsizetype(sizeof(LDrawGeometry::SurfaceRecord));
    if(groups.empty()||groups.size()>MaxGroups){r.status=Status::ResourceLimitExceeded;r.diagnostics<<"Semantic group limit was exceeded.";return r;}int loopCount=0;for(const auto&g:groups)loopCount+=int(g.loops.size());if(loopCount>MaxLoops){r.status=Status::ResourceLimitExceeded;r.diagnostics<<"Boundary loop limit was exceeded.";return r;}
    auto certified=[&](const Component&g,QStringList*files){QSet<int>ids;for(int ti:g.sourceTriangles){if(ti<0||ti>=effective.sourceModel->surfaces.size())return false;const auto&s=effective.sourceModel->surfaces[ti];if(!s.certified)return false;ids.insert(s.fileId);}for(int id:ids){if(id<0||id>=effective.sourceModel->files.size())return false;const auto&f=effective.sourceModel->files[id];if(f.classification==LDrawGeometry::SourceClassification::Unknown)return false;files->append(f.relativePath);}files->sort();return true;};
    int body=-1;double bodySpan=-1;for(int i=0;i<int(groups.size());++i){if(cancellationRequested&&cancellationRequested()){r.status=Status::Cancelled;r.diagnostics<<"Semantic preparation was cancelled during grouping.";return r;}auto a=analyzeSource(groups[i].mesh);if(!validatePreparedMesh(a,true).ok())continue;auto d=sub(a.bounds.maximum,a.bounds.minimum);double span=d.x*d.y*d.z;if(span>bodySpan){body=i;bodySpan=span;}}
    int passage=-1;
    if(body<0){
        for(auto&group:groups)repairCollinearSeams(&group);
        for(int pi=0;pi<int(groups.size())&&body<0;++pi){
            Point axis;const auto passageAnalysis=analyzeSource(groups[pi].mesh);QStringList passageFiles;
            if(passageAnalysis.signedVolume>=0.0||!circularPassage(groups[pi],&axis)||!certified(groups[pi],&passageFiles))continue;
            for(int si=0;si<int(groups.size());++si){if(si==pi||groups[si].loops.size()<2)continue;const auto shellAnalysis=analyzeSource(groups[si].mesh);QStringList shellFiles;
                if(!opensOnOppositeBounds(groups[pi],shellAnalysis.bounds,axis)||!hasMatchingOpenings(groups[si],groups[pi])||!certified(groups[si],&shellFiles))continue;
                PrintMesh closedShell,closedPassage;int shellAdded=0,passageAdded=0;
                const bool shellClosed=closePlanarLoops(groups[si],&closedShell,&shellAdded),passageClosed=closeRoundPassage(groups[pi],axis,&closedPassage,&passageAdded);
                if(!shellClosed||!passageClosed){r.diagnostics<<QString("Round passage candidate closure rejected (shellClosed=%1, passageClosed=%2).").arg(shellClosed).arg(passageClosed);continue;}
                groups[si].mesh=std::move(closedShell);groups[si].loops.clear();groups[pi].mesh=std::move(closedPassage);groups[pi].loops.clear();
                body=si;passage=pi;r.closureTriangles+=shellAdded+passageAdded;
                r.diagnostics<<QString("Certified round through-passage recognized structurally between opposed body openings (passage group %1, body group %2).").arg(pi).arg(si);break;
            }
        }
    }
    if(body<0){r.status=Status::OperandValidationFailed;r.diagnostics<<"No independently closed body/cavity operand or certified round through-passage body was found.";return r;}const MeshBounds bodyBounds=analyzeSource(groups[body].mesh).bounds;
    QVector<int>order;order<<body;if(passage>=0)order<<passage;for(int loopCount:{1,2})for(int i=0;i<int(groups.size());++i)if(i!=body&&i!=passage&&int(groups[i].loops.size())==loopCount)order<<i;
    for(int index:order){if(cancellationRequested&&cancellationRequested()){r.status=Status::Cancelled;r.diagnostics<<"Semantic preparation was cancelled between operands.";return r;}const auto&g=groups[index];SemanticOperand op;op.sourceMesh=g.mesh;op.confidence=SemanticConfidence::HighConfidence;QStringList files;if(!certified(g,&files)){r.status=Status::UncertifiedGeometry;r.diagnostics<<QString("Group %1 is uncertified or not official.").arg(index);return r;}op.sourceFiles=files;
        if(index==body){op.role=SemanticRole::PrimaryBody;op.feature=SemanticFeature::BodyOrCavity;op.closedMesh=g.mesh;normalize(&op.closedMesh);op.analysis=analyzeSource(op.closedMesh);}
        else if(index==passage){op.role=SemanticRole::SubtractivePassage;op.feature=SemanticFeature::RoundThroughPassage;op.closedMesh=g.mesh;normalize(&op.closedMesh);op.analysis=analyzeSource(op.closedMesh);}
        else {bool contacts=true;for(const auto&loop:g.loops)contacts=contacts&&inBounds(bodyBounds,center(g.mesh,loop),ContactMm);if(!contacts){r.status=Status::AmbiguousBoundary;r.diagnostics<<QString("Group %1 does not contact the body at its boundary.").arg(index);return r;}
            if(g.loops.size()==1){op.role=SemanticRole::AdditiveAttachment;op.feature=SemanticFeature::Stud;if(!closeSingle(g,&op.closedMesh,&op.closureTriangles,passage>=0)){r.status=Status::OperandClosureFailed;r.diagnostics<<QString("Single-loop closure failed for group %1.").arg(index);return r;}}
            else if(g.loops.size()==2){op.role=SemanticRole::HollowAdditiveAttachment;op.feature=SemanticFeature::Tube;if(!closeAnnularBoundary(g,&op.closedMesh,&op.closureTriangles)){r.status=Status::OperandClosureFailed;r.diagnostics<<QString("Nested-loop annular closure failed for group %1.").arg(index);return r;}}
            else {r.status=Status::UnsupportedBoundaryTopology;r.diagnostics<<QString("Group %1 has %2 boundary loops.").arg(index).arg(g.loops.size());return r;}op.analysis=analyzeSource(op.closedMesh);}
        const auto operandValidation=validateBooleanOperand(op.analysis);if(!operandValidation.ok()){r.status=Status::OperandValidationFailed;r.diagnostics<<QString("The %1 operand failed Boolean-operand validation: %2 (boundaryEdges=%3, components=%4, nonManifoldVertices=%5, selfIntersections=%6).").arg(roleName(op.role),QString::fromStdString(operandValidation.message)).arg(op.analysis.boundaryEdges).arg(op.analysis.connectedComponents).arg(op.analysis.nonManifoldVertices).arg(op.analysis.selfIntersections);return r;}r.closureTriangles+=op.closureTriangles;r.operands.push_back(std::move(op));}
    if(r.operands.size()>MaxOperands){r.status=Status::ResourceLimitExceeded;r.diagnostics<<"Semantic operand limit was exceeded.";return r;}r.semanticGenerationMs=phase.elapsed();r.status=Status::Ready;
    r.diagnostics<<QString("groups=%1 loops=%2 operands=%3 closureTriangles=%4 provenanceBytes~%5 semanticMs=%6 elapsedMs=%7")
        .arg(r.semanticGroups).arg(loopCount).arg(r.operands.size()).arg(r.closureTriangles).arg(r.approximateProvenanceBytes).arg(r.semanticGenerationMs).arg(total.elapsed());return r;
}
} // namespace PrintGeometry
