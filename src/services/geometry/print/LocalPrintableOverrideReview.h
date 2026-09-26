#pragma once
#include "SourceSurfaceQueryIndex.h"
#include "LDrawPrintGeometryBuilder.h"
#include <QElapsedTimer>

namespace PrintGeometry { namespace OverrideReview {
inline Point sub(Point a,Point b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
inline Point add(Point a,Point b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
inline Point mul(Point a,double b){return {a.x*b,a.y*b,a.z*b};}
inline double dot(Point a,Point b){return a.x*b.x+a.y*b.y+a.z*b.z;}
inline Point cross(Point a,Point b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
inline double norm(Point a){return std::sqrt(dot(a,a));}
inline Point unit(Point a){return mul(a,1/norm(a));}
inline double ray(Point p,Point d,const SourceSurfaceQueryIndex::Triangle&t){
    const auto e=sub(t.b,t.a),f=sub(t.c,t.a),h=cross(d,f);const double det=dot(e,h);
    if(std::abs(det)<1e-12)return -1;
    const auto s=sub(p,t.a),q=cross(s,e);const double u=dot(s,h)/det,v=dot(d,q)/det,z=dot(f,q)/det;
    return u>=-1e-9&&v>=-1e-9&&u+v<=1+1e-9&&z>1e-8?z:-1;
}
// Additional objective gate for explicit review only. Never grants source certification.
inline bool eligible(const LDrawGeometry::LDrawLoadResult& loaded,const PrintMesh& source,
                     const PrintMesh& repair,QString* diagnostic){
    QElapsedTimer timer;timer.start();std::uint64_t work=0;
    auto fail=[&](const QString& why){*diagnostic=why;return false;};
    if(source.faces.size()>2000)return fail("User review exceeds the 2,000-source-triangle limit.");
    const auto sourceAnalysis=analyzeSource(source);
    if(!sourceAnalysis.boundaryEdges&&!sourceAnalysis.selfIntersections)
        return fail("Source has no open/overlapping composition requiring user review.");
    const auto semantics=LDrawSemanticOperandBuilder::build(loaded);
    for(const auto&op:semantics.operands)if(!op.functionalFeatures.isEmpty())
        return fail("Recognized fit interfaces require strict correspondence; user review cannot waive them.");
    std::vector<SourceSurfaceQueryIndex::Triangle> src,dst;
    for(const auto&f:source.faces)src.push_back({source.vertices[f[0]],source.vertices[f[1]],source.vertices[f[2]]});
    for(const auto&f:repair.faces)dst.push_back({repair.vertices[f[0]],repair.vertices[f[1]],repair.vertices[f[2]]});
    SourceSurfaceQueryIndex index(dst,0,0,1,0,0);std::size_t samples=0,internal=0;
    for(const auto&t:src){
        const auto normal=unit(cross(sub(t.b,t.a),sub(t.c,t.a)));
        const int n=std::max(1,int(std::ceil(std::max({norm(sub(t.b,t.a)),norm(sub(t.c,t.a)),norm(sub(t.c,t.b))})/.5)));
        for(int i=0;i<=n;++i)for(int j=0;j<=n-i;++j){
            if(++samples>1000000||work>500000000||timer.elapsed()>30000)return fail("User-review proof exceeded bounded workload/time limits.");
            const auto p=add(t.a,add(mul(sub(t.b,t.a),double(i)/n),mul(sub(t.c,t.a),double(j)/n)));
            if(index.nearest(p,&work).squaredDistance<=.2*.2)continue;
            ++internal;
            // Closed repair containment: oriented solid angle plus independent ray parity.
            double omega=0;std::vector<double> hits[3];const Point dirs[3]={{1,.371,.129},{-.217,1,.493},{.317,-.281,1}};
            for(const auto&r:dst){
                auto a=sub(r.a,p),b=sub(r.b,p),c=sub(r.c,p);const double la=norm(a),lb=norm(b),lc=norm(c);
                omega+=2*std::atan2(dot(a,cross(b,c)),la*lb*lc+dot(a,b)*lc+dot(b,c)*la+dot(c,a)*lb);
                for(int k=0;k<3;++k){const double h=ray(p,dirs[k],r);if(h>0)hits[k].push_back(h);}
                work+=4;
            }
            if(std::abs(omega/(4*std::acos(-1.))-1)>1e-6)return fail("Source sample lies outside repaired material or containment is ambiguous.");
            for(auto& h:hits){std::sort(h.begin(),h.end());h.erase(std::unique(h.begin(),h.end(),[](double a,double b){return std::abs(a-b)<1e-7;}),h.end());if(h.size()%2!=1)return fail("Source containment ray tests disagree; user review unavailable.");}
            // A blocked ray across an intentional void is not evidence of
            // internal material. Require the first normal blocker to be a
            // material exit, not an opposing cavity wall (material entry).
            double nearest=std::numeric_limits<double>::infinity();bool materialExit=false;
            for(const auto&s:src){++work;const double h=ray(add(p,mul(normal,1e-5)),normal,s);
                if(h>0&&h<nearest){nearest=h;materialExit=dot(normal,cross(sub(s.b,s.a),sub(s.c,s.a)))>1e-9;}}
            if(!materialExit)return fail("Source normal reaches a void/interface or unresolved material side; protected opening loss cannot be waived.");
            // Protect source-reachable interfaces/openings. Blocked rays mean unresolved,
            // never certified internal. No repaired triangle receives fit ownership.
            const Point axis=std::abs(normal.x)<.8?Point{1,0,0}:Point{0,1,0};
            const auto u=unit(cross(normal,axis)),v=cross(normal,u);
            std::vector<Point> directions{normal};
            for(double cosine:{.95,.75,.5,.25})for(int k=0;k<16;++k){const double angle=k*2*std::acos(-1.)/16;directions.push_back(add(mul(normal,cosine),mul(add(mul(u,std::cos(angle)),mul(v,std::sin(angle))),std::sqrt(1-cosine*cosine))));}
            for(auto d:directions)for(double offset:{1e-6,1e-5}){
                bool blocked=false;for(const auto&s:src){++work;if(ray(add(p,mul(normal,offset)),d,s)>0){blocked=true;break;}}
                if(!blocked)return fail("An exposed source wall or protected opening/interface was removed; user acceptance is unavailable.");
            }
        }
    }
    *diagnostic=QStringLiteral("Reviewable — Source exposure correspondence unresolved. %1 source samples are inside the repaired solid; source visibility is blocked, but exposure correspondence is not certified. No demonstrated outside/exposed sample. Repaired exterior remains within 0.20 mm.").arg(internal);
    return true;
}
} }
