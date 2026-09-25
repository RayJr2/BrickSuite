#pragma once

#include "PrintMesh.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numeric>
#include <vector>

namespace PrintGeometry {

// The source order is retained in ray buckets and used to resolve equal-distance
// nearest-point ties. The index changes only which exact triangle tests run.
class SourceSurfaceQueryIndex
{
public:
    struct Triangle { Point a,b,c; };
    struct Nearest { Point point; double squaredDistance=std::numeric_limits<double>::max(); int triangle=-1; };

    SourceSurfaceQueryIndex(const std::vector<Triangle>& triangles,
                            double originY,double originZ,double pitch,int ny,int nz)
        : m_triangles(triangles),m_all(triangles.size()),m_ny(ny),m_nz(nz)
    {
        std::iota(m_all.begin(),m_all.end(),0);
        m_order=m_all;
        const std::size_t rayCount=std::size_t(ny)*std::size_t(nz);
        if(rayCount<=250000)m_rayBuckets.resize(rayCount);
        std::size_t references=0;
        for(int i=0;!m_rayBuckets.empty()&&i<int(triangles.size());++i){
            const auto&t=triangles[std::size_t(i)];
            const double lowY=std::min({t.a.y,t.b.y,t.c.y}),highY=std::max({t.a.y,t.b.y,t.c.y});
            const double lowZ=std::min({t.a.z,t.b.z,t.c.z}),highZ=std::max({t.a.z,t.b.z,t.c.z});
            // yzIntersection accepts barycentric values down to -1e-10.
            // Include more than that possible geometric excursion in the bin.
            const double marginY=1e-8*std::max(1.0,highY-lowY);
            const double marginZ=1e-8*std::max(1.0,highZ-lowZ);
            const int firstY=std::max(0,int(std::ceil((lowY-marginY-originY)/pitch))-1);
            const int lastY=std::min(ny-1,int(std::floor((highY+marginY-originY)/pitch))+1);
            const int firstZ=std::max(0,int(std::ceil((lowZ-marginZ-originZ)/pitch))-1);
            const int lastZ=std::min(nz-1,int(std::floor((highZ+marginZ-originZ)/pitch))+1);
            if(firstY>lastY||firstZ>lastZ)continue;
            references+=std::size_t(lastY-firstY+1)*std::size_t(lastZ-firstZ+1);
            if(references>4000000){m_rayBuckets.clear();break;}
            for(int z=firstZ;z<=lastZ;++z)for(int y=firstY;y<=lastY;++y)
                m_rayBuckets[std::size_t(y)+std::size_t(ny)*std::size_t(z)].push_back(i);
        }
        m_rayReferences=m_rayBuckets.empty()?0:references;
        if(!triangles.empty())build(0,int(m_order.size()));
    }

    const std::vector<int>& rayCandidates(int y,int z) const
    {
        return m_rayBuckets.empty()?m_all:m_rayBuckets[std::size_t(y)+std::size_t(m_ny)*std::size_t(z)];
    }
    const std::vector<int>& allTriangles() const { return m_all; }
    std::size_t rayReferences() const { return m_rayReferences; }
    bool rayIndexed() const { return !m_rayBuckets.empty(); }

    Nearest nearest(const Point&p,std::uint64_t*exactTests=nullptr) const
    {
        Nearest best;
        if(!m_nodes.empty())search(0,p,&best,exactTests);
        return best;
    }
    Nearest exhaustiveNearest(const Point&p,std::uint64_t*exactTests=nullptr) const
    {
        Nearest best;
        for(int i:m_all)consider(i,p,&best,exactTests);
        return best;
    }

private:
    struct Bounds { Point low,high; };
    struct Node { Bounds bounds; int begin=0,end=0,left=-1,right=-1; };

    static double component(const Point&p,int axis){return axis==0?p.x:axis==1?p.y:p.z;}
    static Point sub(const Point&a,const Point&b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
    static Point add(const Point&a,const Point&b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
    static Point mul(const Point&a,double s){return {a.x*s,a.y*s,a.z*s};}
    static double dot(const Point&a,const Point&b){return a.x*b.x+a.y*b.y+a.z*b.z;}
    static double distance2(const Point&a,const Point&b){return dot(sub(a,b),sub(a,b));}
    static Point closestPoint(const Point&p,const Triangle&t)
    {
        const auto&a=t.a,&b=t.b,&c=t.c;
        const auto ab=sub(b,a),ac=sub(c,a),ap=sub(p,a);const double d1=dot(ab,ap),d2=dot(ac,ap);if(d1<=0.0&&d2<=0.0)return a;
        const auto bp=sub(p,b);const double d3=dot(ab,bp),d4=dot(ac,bp);if(d3>=0.0&&d4<=d3)return b;
        const double vc=d1*d4-d3*d2;if(vc<=0.0&&d1>=0.0&&d3<=0.0)return add(a,mul(ab,d1/(d1-d3)));
        const auto cp=sub(p,c);const double d5=dot(ab,cp),d6=dot(ac,cp);if(d6>=0.0&&d5<=d6)return c;
        const double vb=d5*d2-d1*d6;if(vb<=0.0&&d2>=0.0&&d6<=0.0)return add(a,mul(ac,d2/(d2-d6)));
        const double va=d3*d6-d5*d4;if(va<=0.0&&(d4-d3)>=0.0&&(d5-d6)>=0.0){const auto bc=sub(c,b);return add(b,mul(bc,(d4-d3)/((d4-d3)+(d5-d6))));}
        const double denominator=1.0/(va+vb+vc),v=vb*denominator,w=vc*denominator;return add(a,add(mul(ab,v),mul(ac,w)));
    }
    static Bounds bounds(const Triangle&t)
    {
        return {{std::min({t.a.x,t.b.x,t.c.x}),std::min({t.a.y,t.b.y,t.c.y}),std::min({t.a.z,t.b.z,t.c.z})},
                {std::max({t.a.x,t.b.x,t.c.x}),std::max({t.a.y,t.b.y,t.c.y}),std::max({t.a.z,t.b.z,t.c.z})}};
    }
    static double boxDistance2(const Bounds&b,const Point&p)
    {
        double sum=0.0;
        for(int axis=0;axis<3;++axis){const double v=component(p,axis),low=component(b.low,axis),high=component(b.high,axis);
            const double d=v<low?low-v:v>high?v-high:0.0;sum+=d*d;}
        return sum;
    }
    int build(int begin,int end)
    {
        Bounds box=bounds(m_triangles[std::size_t(m_order[std::size_t(begin)])]);
        for(int i=begin+1;i<end;++i){const auto b=bounds(m_triangles[std::size_t(m_order[std::size_t(i)])]);
            box.low={std::min(box.low.x,b.low.x),std::min(box.low.y,b.low.y),std::min(box.low.z,b.low.z)};
            box.high={std::max(box.high.x,b.high.x),std::max(box.high.y,b.high.y),std::max(box.high.z,b.high.z)};}
        const int index=int(m_nodes.size());m_nodes.push_back({box,begin,end});
        if(end-begin<=8)return index;
        const double spans[3]={box.high.x-box.low.x,box.high.y-box.low.y,box.high.z-box.low.z};
        const int axis=int(std::max_element(spans,spans+3)-spans);
        std::stable_sort(m_order.begin()+begin,m_order.begin()+end,[&](int a,int b){
            const auto&ta=m_triangles[std::size_t(a)],&tb=m_triangles[std::size_t(b)];
            const double ca=component(ta.a,axis)+component(ta.b,axis)+component(ta.c,axis);
            const double cb=component(tb.a,axis)+component(tb.b,axis)+component(tb.c,axis);
            return ca<cb||(ca==cb&&a<b);
        });
        const int middle=begin+(end-begin)/2;
        const int left=build(begin,middle),right=build(middle,end);
        m_nodes[std::size_t(index)].left=left;m_nodes[std::size_t(index)].right=right;
        return index;
    }
    void consider(int i,const Point&p,Nearest*best,std::uint64_t*exactTests) const
    {
        if(exactTests)++*exactTests;
        const auto point=closestPoint(p,m_triangles[std::size_t(i)]);
        const double squared=distance2(p,point);
        if(squared<best->squaredDistance||(squared==best->squaredDistance&&i<best->triangle))
            *best={point,squared,i};
    }
    void search(int index,const Point&p,Nearest*best,std::uint64_t*exactTests) const
    {
        const auto&node=m_nodes[std::size_t(index)];
        // A conservative comparison prevents roundoff at a box boundary from
        // excluding an exactly tied or marginally nearer source triangle.
        if(boxDistance2(node.bounds,p)>best->squaredDistance+
           1e-12*std::max(1.0,best->squaredDistance))return;
        if(node.left<0){for(int i=node.begin;i<node.end;++i)consider(m_order[std::size_t(i)],p,best,exactTests);return;}
        const double left=boxDistance2(m_nodes[std::size_t(node.left)].bounds,p);
        const double right=boxDistance2(m_nodes[std::size_t(node.right)].bounds,p);
        if(left<=right){search(node.left,p,best,exactTests);search(node.right,p,best,exactTests);}
        else{search(node.right,p,best,exactTests);search(node.left,p,best,exactTests);}
    }

    const std::vector<Triangle>&m_triangles;
    std::vector<int>m_all;
    std::vector<int>m_order;
    int m_ny=0,m_nz=0;
    std::vector<std::vector<int>>m_rayBuckets;
    std::size_t m_rayReferences=0;
    std::vector<Node>m_nodes;
};

} // namespace PrintGeometry
