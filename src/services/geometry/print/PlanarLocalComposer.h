#pragma once

#include "PrintMeshAnalysis.h"
#include "SemanticOperand.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <map>
#include <queue>
#include <set>
#include <tuple>
#include <vector>

namespace PrintGeometry {

// A conservative exact-surface route for independent open attachments on
// rectangular planar body patches. Only the body's contact planes are
// retessellated; certified attachment surfaces remain source-derived. The
// caller must still enforce source coverage and final mesh/fidelity checks.
class PlanarLocalComposer {
public:
    struct Result {
        bool successful = false;
        PrintMesh mesh;
        QString diagnostic;
        int localRegions = 0;
    };

    static Result compose(const QVector<SemanticOperand>& operands)
    {
        Result result;
        if (operands.size() < 3 || operands.front().role != SemanticRole::PrimaryBody)
            return result;
        std::size_t sourceFaces=operands.front().closedMesh.faces.size();
        std::size_t sourceVertices=operands.front().closedMesh.vertices.size();
        for(int i=1;i<operands.size();++i){
            sourceFaces+=operands[i].sourceMesh.faces.size();
            sourceVertices+=operands[i].sourceMesh.vertices.size();
        }
        if(sourceFaces>12000||sourceVertices>16000){
            result.diagnostic=QStringLiteral("Local source surface budget exceeded.");return result;
        }
        if(operands.front().sourceMesh.faces.size()!=operands.front().closedMesh.faces.size()||
           analyzeSource(operands.front().sourceMesh).boundaryEdges!=0){
            result.diagnostic=QStringLiteral("Primary body is not an authoritative closed source surface.");return result;
        }

        std::vector<Contact> contacts;
        for (int i=1; i<operands.size(); ++i) {
            const auto& operand=operands[i];
            if (operand.role!=SemanticRole::AdditiveAttachment &&
                operand.role!=SemanticRole::HollowAdditiveAttachment) {result.diagnostic="Unsupported role";return result;}
            if (!validateBooleanOperand(operand.analysis).ok()) return result;
            auto loops=boundaryLoops(operand.sourceMesh);
            if (loops.size()<1 || loops.size()>2) {result.diagnostic=QString("Attachment %1 has %2 boundary loops").arg(i).arg(loops.size());return result;}
            Contact contact;contact.operand=i;
            contact.plane=loops.front().points.front().z;
            for (auto& loop:loops) {
                for (const auto& point:loop.points)
                    if (std::abs(point.z-contact.plane)>1e-5) return result;
                loop.cx=0;loop.cy=0;
                for (const auto& point:loop.points) {loop.cx+=point.x;loop.cy+=point.y;}
                loop.cx/=loop.points.size();loop.cy/=loop.points.size();
                for (const auto& point:loop.points)
                    loop.radius=std::max(loop.radius,std::hypot(point.x-loop.cx,point.y-loop.cy));
                if (loop.points.size()<8) return result;
            }
            std::sort(loops.begin(),loops.end(),[](const Loop&a,const Loop&b){return a.radius>b.radius;});
            contact.cx=loops.front().cx;contact.cy=loops.front().cy;
            for (const auto& loop:loops)
                if (std::hypot(loop.cx-contact.cx,loop.cy-contact.cy)>1e-4) return result;
            contact.half=loops.front().radius+0.2;
            for(const auto& vertex:operand.sourceMesh.vertices)
                if(std::abs(vertex.x-contact.cx)>contact.half+1e-5||
                   std::abs(vertex.y-contact.cy)>contact.half+1e-5){
                    result.diagnostic=QStringLiteral("Attachment extends beyond its local contact window.");return result;
                }
            contact.loops=std::move(loops);
            contacts.push_back(std::move(contact));
        }

        PrintMesh assembled=operands.front().closedMesh;
        std::set<std::size_t> replaced;
        std::vector<double> planes;
        for (const auto& contact:contacts)
            if (std::none_of(planes.begin(),planes.end(),[&](double z){return std::abs(z-contact.plane)<1e-5;}))
                planes.push_back(contact.plane);
        for (double plane:planes) {
            std::vector<const Contact*> onPlane;
            for (const auto& contact:contacts)
                if (std::abs(contact.plane-plane)<1e-5) onPlane.push_back(&contact);
            const auto patch=rectangle(assembled,plane,onPlane,replaced);
            if (!patch.valid) {result.diagnostic=QString("No certified planar rectangle at z=%1").arg(plane);return result;}
            for(const auto* contact:onPlane){
                bool outward=false;
                for(const auto& vertex:operands[contact->operand].sourceMesh.vertices){
                    const double offset=(vertex.z-plane)*patch.normal;
                    if(offset<-1e-5){result.diagnostic=QStringLiteral("Attachment crosses its supporting body plane.");return result;}
                    outward|=offset>1e-4;
                }
                if(!outward){result.diagnostic=QStringLiteral("Attachment does not extend outward from its body plane.");return result;}
            }
            for (int i=0;i<int(onPlane.size());++i) for (int j=0;j<i;++j) {
                const auto*a=onPlane[i],*b=onPlane[j];
                if (std::abs(a->cx-b->cx)<a->half+b->half+1e-5 &&
                    std::abs(a->cy-b->cy)<a->half+b->half+1e-5) return result;
            }
            std::vector<double> xs{patch.x0,patch.x1},ys{patch.y0,patch.y1};
            for (const auto* contact:onPlane) {
                if (contact->cx-contact->half<=patch.x0+1e-5 ||
                    contact->cx+contact->half>=patch.x1-1e-5 ||
                    contact->cy-contact->half<=patch.y0+1e-5 ||
                    contact->cy+contact->half>=patch.y1-1e-5) return result;
                xs.push_back(contact->cx-contact->half);xs.push_back(contact->cx+contact->half);
                ys.push_back(contact->cy-contact->half);ys.push_back(contact->cy+contact->half);
            }
            uniqueSorted(&xs);uniqueSorted(&ys);
            if (xs.size()*ys.size()>2000) {result.diagnostic=QStringLiteral("Local planar grid budget exceeded.");return result;}
            for (std::size_t y=0;y+1<ys.size();++y) for (std::size_t x=0;x+1<xs.size();++x) {
                const double x0=xs[x],x1=xs[x+1],y0=ys[y],y1=ys[y+1];
                const Contact* inside=nullptr;
                for (const auto* contact:onPlane)
                    if (std::abs((x0+x1)*.5-contact->cx)<contact->half-1e-5 &&
                        std::abs((y0+y1)*.5-contact->cy)<contact->half-1e-5) {
                        if (inside || std::abs(x0-(contact->cx-contact->half))>1e-5 ||
                            std::abs(x1-(contact->cx+contact->half))>1e-5 ||
                            std::abs(y0-(contact->cy-contact->half))>1e-5 ||
                            std::abs(y1-(contact->cy+contact->half))>1e-5) return result;
                        inside=contact;
                    }
                if (inside) {
                    if (!ringPatch(&assembled,*inside,x0,x1,y0,y1,patch.z,patch.normal)) return result;
                    ++result.localRegions;
                } else {
                    const auto a=point(&assembled,x0,y0,plane),b=point(&assembled,x1,y0,plane),
                        c=point(&assembled,x1,y1,plane),d=point(&assembled,x0,y1,plane);
                    face(&assembled,a,b,c,patch.normal);face(&assembled,a,c,d,patch.normal);
                }
            }
            replaced.insert(patch.first);replaced.insert(patch.second);
        }
        if(result.localRegions!=int(contacts.size()))return result;

        PrintMesh combined;
        combined.vertices=assembled.vertices;
        for(std::size_t i=0;i<assembled.faces.size();++i)
            if(!replaced.count(i))combined.faces.push_back(assembled.faces[i]);
        for(const auto& operand:operands) {
            if(operand.role==SemanticRole::PrimaryBody)continue;
            const auto offset=std::uint32_t(combined.vertices.size());
            combined.vertices.insert(combined.vertices.end(),operand.sourceMesh.vertices.begin(),operand.sourceMesh.vertices.end());
            for(auto triangle:operand.sourceMesh.faces) {
                for(auto& vertex:triangle)vertex+=offset;
                combined.faces.push_back(triangle);
            }
        }
        if(combined.faces.size()>30000||combined.vertices.size()>30000||
           combined.faces.size()*combined.vertices.size()>250000000){
            result.diagnostic=QStringLiteral("Local stitching workload budget exceeded.");return result;
        }
        weld(&combined);
        splitTJunctions(&combined);
        orient(&combined);
        const auto analysis=analyzeSource(combined);
        const auto validation=validatePreparedMesh(analysis);
        if(!validation.ok()) {
            result.diagnostic=QStringLiteral("Local planar composition failed: %1; faces=%2 boundaries=%3 components=%4 nonmanifold=%5")
                .arg(QString::fromStdString(validation.message)).arg(analysis.triangles).arg(analysis.boundaryEdges)
                .arg(analysis.connectedComponents).arg(analysis.nonManifoldEdges);
            return result;
        }
        result.mesh=std::move(combined);
        result.successful=true;
        result.diagnostic=QStringLiteral("Certified planar attachment boundaries were stitched through %1 disjoint local surface regions (%2 faces, %3 vertices).").arg(result.localRegions).arg(result.mesh.faces.size()).arg(result.mesh.vertices.size());
        return result;
    }

private:
    struct Loop { std::vector<Point> points; double cx=0, cy=0, radius=0; };
    struct Contact { int operand=0; double plane=0; std::vector<Loop> loops; double cx=0, cy=0, half=0; };
    struct Rectangle {bool valid=false;std::size_t first=0,second=0;double x0=0,x1=0,y0=0,y1=0,z=0,normal=0;};
    static void uniqueSorted(std::vector<double>*v)
    {
        std::sort(v->begin(),v->end());
        v->erase(std::unique(v->begin(),v->end(),[](double a,double b){return std::abs(a-b)<1e-4;}),v->end());
    }
    static std::vector<Loop> boundaryLoops(const PrintMesh&);
    static Rectangle rectangle(const PrintMesh&,double,const std::vector<const Contact*>&,
                               const std::set<std::size_t>&);
    static std::uint32_t point(PrintMesh*mesh,double x,double y,double z)
    {const auto index=std::uint32_t(mesh->vertices.size());mesh->vertices.push_back({x,y,z});return index;}
    static void face(PrintMesh*mesh,std::uint32_t a,std::uint32_t b,std::uint32_t c,double normal)
    {const auto&u=mesh->vertices[a],&v=mesh->vertices[b],&w=mesh->vertices[c];
     if(((v.x-u.x)*(w.y-u.y)-(v.y-u.y)*(w.x-u.x))*normal<0)std::swap(b,c);
     mesh->faces.push_back({a,b,c});}
    static bool ringPatch(PrintMesh*,const Contact&,double,double,double,double,double,double);
    static void weld(PrintMesh*);
    static void splitTJunctions(PrintMesh*);
    static void orient(PrintMesh*);
};

inline std::vector<PlanarLocalComposer::Loop> PlanarLocalComposer::boundaryLoops(const PrintMesh& mesh)
{
    using Edge=std::pair<std::uint32_t,std::uint32_t>;
    std::map<Edge,int> uses;
    for(const auto& f:mesh.faces)for(int k=0;k<3;++k){auto a=f[k],b=f[(k+1)%3];if(a>b)std::swap(a,b);++uses[{a,b}];}
    std::vector<std::vector<std::uint32_t>> adjacency(mesh.vertices.size());
    for(const auto& [edge,count]:uses)if(count==1){adjacency[edge.first].push_back(edge.second);adjacency[edge.second].push_back(edge.first);}
    else if(count!=2)return{};
    std::set<std::uint32_t> seen;
    std::vector<Loop> loops;
    for(std::uint32_t start=0;start<adjacency.size();++start){
        if(adjacency[start].empty()||seen.count(start))continue;
        Loop loop;auto current=start,previous=std::uint32_t(-1);
        do{
            if(current>=adjacency.size()||adjacency[current].size()!=2||seen.count(current))return{};
            seen.insert(current);loop.points.push_back(mesh.vertices[current]);
            const auto next=adjacency[current][0]==previous?adjacency[current][1]:adjacency[current][0];
            previous=current;current=next;
            if(loop.points.size()>mesh.vertices.size())return{};
        }while(current!=start);
        loops.push_back(std::move(loop));
    }
    return loops;
}

inline PlanarLocalComposer::Rectangle PlanarLocalComposer::rectangle(
    const PrintMesh&mesh,double z,const std::vector<const Contact*>&contacts,
    const std::set<std::size_t>&replaced)
{
    Rectangle answer;
    for(std::size_t i=0;i<mesh.faces.size();++i){
        if(replaced.count(i))continue;
        const auto& f=mesh.faces[i];
        if(std::any_of(f.begin(),f.end(),[&](auto v){return std::abs(mesh.vertices[v].z-z)>1e-5;}))continue;
        for(std::size_t j=i+1;j<mesh.faces.size();++j){
            if(replaced.count(j))continue;
            const auto& g=mesh.faces[j];
            if(std::any_of(g.begin(),g.end(),[&](auto v){return std::abs(mesh.vertices[v].z-z)>1e-5;}))continue;
            std::set<std::uint32_t> ids(f.begin(),f.end());ids.insert(g.begin(),g.end());
            if(ids.size()!=4)continue;
            std::vector<double> xs,ys;
            for(auto id:ids){xs.push_back(mesh.vertices[id].x);ys.push_back(mesh.vertices[id].y);}
            uniqueSorted(&xs);uniqueSorted(&ys);
            if(xs.size()!=2||ys.size()!=2)continue;
            bool corners=true;
            for(auto id:ids){const auto&p=mesh.vertices[id];
                corners&=(std::abs(p.x-xs.front())<1e-5||std::abs(p.x-xs.back())<1e-5)&&
                         (std::abs(p.y-ys.front())<1e-5||std::abs(p.y-ys.back())<1e-5);}
            if(!corners)continue;
            const auto&a=mesh.vertices[f[0]],&b=mesh.vertices[f[1]],&c=mesh.vertices[f[2]];
            const auto&d=mesh.vertices[g[0]],&e=mesh.vertices[g[1]],&h=mesh.vertices[g[2]];
            const double n1=(b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x);
            const double n2=(e.x-d.x)*(h.y-d.y)-(e.y-d.y)*(h.x-d.x);
            const double area=(xs.back()-xs.front())*(ys.back()-ys.front());
            if(n1*n2<=0||std::abs((std::abs(n1)+std::abs(n2))*0.5-area)>1e-4)continue;
            bool contains=true;
            for(const auto*contact:contacts)
                contains&=contact->cx-contact->half>xs.front()&&contact->cx+contact->half<xs.back()&&
                          contact->cy-contact->half>ys.front()&&contact->cy+contact->half<ys.back();
            if(!contains)continue;
            if(answer.valid)return{};
            answer={true,i,j,xs.front(),xs.back(),ys.front(),ys.back(),z,n1>0?1.0:-1.0};
        }
    }
    return answer;
}

inline bool PlanarLocalComposer::ringPatch(PrintMesh*mesh,const Contact&contact,
                                           double x0,double x1,double y0,double y1,double z,double normal)
{
    constexpr double pi=3.14159265358979323846;
    const auto&outer=contact.loops.front();
    std::vector<double> angles;
    const auto angle=[&](double x,double y){double a=std::atan2(y-contact.cy,x-contact.cx);return a<0?a+2*pi:a;};
    for(const auto&p:outer.points)angles.push_back(angle(p.x,p.y));
    for(const auto&p:std::array<Point,4>{{{x0,y0,z},{x1,y0,z},{x1,y1,z},{x0,y1,z}}})angles.push_back(angle(p.x,p.y));
    std::sort(angles.begin(),angles.end());
    angles.erase(std::unique(angles.begin(),angles.end(),[](double a,double b){return std::abs(a-b)<1e-3;}),angles.end());
    if(angles.size()>1&&angles.front()+2*pi-angles.back()<1e-3)angles.pop_back();
    if(angles.size()<8)return false;
    const auto cross=[](double ax,double ay,double bx,double by){return ax*by-ay*bx;};
    auto innerAt=[&](double a,Point*out){
        const double dx=std::cos(a),dy=std::sin(a);double best=1e100;
        for(std::size_t k=0;k<outer.points.size();++k){
            const auto&p=outer.points[k],&q=outer.points[(k+1)%outer.points.size()];
            const double ex=q.x-p.x,ey=q.y-p.y,px=p.x-contact.cx,py=p.y-contact.cy;
            const double den=cross(dx,dy,ex,ey);if(std::abs(den)<1e-12)continue;
            const double t=cross(px,py,ex,ey)/den,s=cross(px,py,dx,dy)/den;
            if(t>=0&&s>=-1e-6&&s<=1+1e-6)best=std::min(best,t);
        }
        if(best==1e100)return false;*out={contact.cx+best*dx,contact.cy+best*dy,z};return true;
    };
    std::vector<std::uint32_t> outside,inside;
    for(double a:angles){
        const double dx=std::cos(a),dy=std::sin(a);
        double t=1e100;
        if(dx>1e-12)t=std::min(t,(x1-contact.cx)/dx);
        if(dx<-1e-12)t=std::min(t,(x0-contact.cx)/dx);
        if(dy>1e-12)t=std::min(t,(y1-contact.cy)/dy);
        if(dy<-1e-12)t=std::min(t,(y0-contact.cy)/dy);
        Point inner;if(!innerAt(a,&inner)||t<=outer.radius-1e-5)return false;
        outside.push_back(point(mesh,contact.cx+t*dx,contact.cy+t*dy,z));
        inside.push_back(point(mesh,inner.x,inner.y,z));
    }
    for(std::size_t k=0;k<angles.size();++k){const auto n=(k+1)%angles.size();
        face(mesh,outside[k],outside[n],inside[n],normal);
        face(mesh,outside[k],inside[n],inside[k],normal);
    }
    if(contact.loops.size()==2){
        const auto&inner=contact.loops[1];
        std::vector<Point> points=inner.points;
        std::sort(points.begin(),points.end(),[&](const Point&a,const Point&b){return angle(a.x,a.y)<angle(b.x,b.y);});
        const auto center=point(mesh,contact.cx,contact.cy,z);
        for(std::size_t k=0;k<points.size();++k){const auto a=point(mesh,points[k].x,points[k].y,z);
            const auto&next=points[(k+1)%points.size()];const auto b=point(mesh,next.x,next.y,z);
            face(mesh,center,a,b,normal);}
    }
    return true;
}

inline void PlanarLocalComposer::weld(PrintMesh*mesh)
{
    using Key=std::tuple<long long,long long,long long>;
    std::map<Key,std::uint32_t> ids;
    std::vector<Point> vertices;
    std::vector<std::uint32_t> remap(mesh->vertices.size());
    for(std::size_t i=0;i<mesh->vertices.size();++i){const auto&p=mesh->vertices[i];
        const Key key{std::llround(p.x*1e5),std::llround(p.y*1e5),std::llround(p.z*1e5)};
        std::uint32_t match=std::uint32_t(-1);
        for(long long dx=-1;dx<=1&&match==std::uint32_t(-1);++dx)
            for(long long dy=-1;dy<=1&&match==std::uint32_t(-1);++dy)
                for(long long dz=-1;dz<=1;++dz){
                    const auto found=ids.find({std::get<0>(key)+dx,std::get<1>(key)+dy,std::get<2>(key)+dz});
                    if(found==ids.end())continue;
                    const auto&q=vertices[found->second];
                    if(std::abs(p.x-q.x)<1e-5&&std::abs(p.y-q.y)<1e-5&&std::abs(p.z-q.z)<1e-5){match=found->second;break;}
                }
        if(match!=std::uint32_t(-1))remap[i]=match;
        else{const auto id=std::uint32_t(vertices.size());ids.emplace(key,id);vertices.push_back(p);remap[i]=id;}
    }
    for(auto& f:mesh->faces)for(auto&v:f)v=remap[v];
    mesh->vertices=std::move(vertices);
}

inline void PlanarLocalComposer::splitTJunctions(PrintMesh*mesh)
{
    std::vector<Face> faces;faces.reserve(mesh->faces.size()*2);
    const auto originalVertices=mesh->vertices.size();
    for(const auto&f:mesh->faces){
        std::vector<std::uint32_t> polygon;
        for(int edge=0;edge<3;++edge){
            const auto a=f[edge],b=f[(edge+1)%3];polygon.push_back(a);
            const auto&pa=mesh->vertices[a],&pb=mesh->vertices[b];
            const double dx=pb.x-pa.x,dy=pb.y-pa.y,dz=pb.z-pa.z,length2=dx*dx+dy*dy+dz*dz;
            std::vector<std::pair<double,std::uint32_t>> splits;
            if(length2<1e-12)continue;
            for(std::uint32_t v=0;v<originalVertices;++v){
                if(v==a||v==b)continue;
                const auto&p=mesh->vertices[v];
                const double t=((p.x-pa.x)*dx+(p.y-pa.y)*dy+(p.z-pa.z)*dz)/length2;
                if(t<=1e-7||t>=1-1e-7)continue;
                const double ex=p.x-pa.x-t*dx,ey=p.y-pa.y-t*dy,ez=p.z-pa.z-t*dz;
                if(ex*ex+ey*ey+ez*ez<1e-12)splits.push_back({t,v});
            }
            std::sort(splits.begin(),splits.end());
            for(const auto&entry:splits)polygon.push_back(entry.second);
        }
        if(polygon.size()==3){faces.push_back(f);continue;}
        Point center;for(auto v:f){const auto&p=mesh->vertices[v];center.x+=p.x/3;center.y+=p.y/3;center.z+=p.z/3;}
        const auto id=std::uint32_t(mesh->vertices.size());mesh->vertices.push_back(center);
        for(std::size_t k=0;k<polygon.size();++k)faces.push_back({id,polygon[k],polygon[(k+1)%polygon.size()]});
    }
    mesh->faces=std::move(faces);
}

inline void PlanarLocalComposer::orient(PrintMesh*mesh)
{
    using Edge=std::pair<std::uint32_t,std::uint32_t>;
    struct Use{std::size_t face;bool forward;};
    std::map<Edge,std::vector<Use>> uses;
    for(std::size_t i=0;i<mesh->faces.size();++i)for(int k=0;k<3;++k){const auto&f=mesh->faces[i];
        auto a=f[k],b=f[(k+1)%3];uses[{std::min(a,b),std::max(a,b)}].push_back({i,a<b});}
    std::vector<bool> seen(mesh->faces.size()),flip(mesh->faces.size());
    for(std::size_t root=0;root<mesh->faces.size();++root){if(seen[root])continue;
        std::queue<std::size_t> q;q.push(root);seen[root]=true;
        while(!q.empty()){const auto i=q.front();q.pop();const auto f=mesh->faces[i];
            for(int k=0;k<3;++k){const auto a=f[k],b=f[(k+1)%3];const auto key=Edge{std::min(a,b),std::max(a,b)};
                for(const auto&use:uses[key])if(use.face!=i&&!seen[use.face]){
                    flip[use.face]=((a<b)!=flip[i])==use.forward;
                    seen[use.face]=true;q.push(use.face);
                }}
        }
    }
    for(std::size_t i=0;i<mesh->faces.size();++i)if(flip[i])std::swap(mesh->faces[i][1],mesh->faces[i][2]);
    if(analyzeSource(*mesh).signedVolume<0)for(auto&face:mesh->faces)std::swap(face[1],face[2]);
}

} // namespace PrintGeometry
