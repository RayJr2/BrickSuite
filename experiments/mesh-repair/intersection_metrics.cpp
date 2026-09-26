#include "intersection_metrics.h"
#include <CGAL/Exact_predicates_exact_constructions_kernel.h>
#include <CGAL/intersections.h>
IntersectionMetrics measureIntersections(const Mesh& mesh,const std::vector<std::pair<Mesh::Face_index,Mesh::Face_index>>& pairs)
{
    // Inexact intersection constructions are unreliable for these near-coplanar
    // contacts. Construct exact rationals from the actual double input coordinates.
    using Exact=CGAL::Exact_predicates_exact_constructions_kernel;
    const auto point=[&](auto v){const auto& p=mesh.point(v);return Exact::Point_3(p.x(),p.y(),p.z());};
    const auto triangle=[&](auto f){auto h=halfedge(f,mesh);return Exact::Triangle_3(
        point(source(h,mesh)),point(target(h,mesh)),point(target(next(h,mesh),mesh)));};
    IntersectionMetrics result;
    for(const auto& pair:pairs){
        if(pair.first==pair.second){++result.degenerate;continue;}
        const auto intersection=CGAL::intersection(triangle(pair.first),triangle(pair.second));
        if(!intersection){++result.absent;continue;}
        if(std::get_if<Exact::Point_3>(&*intersection))++result.points;
        else if(const auto* segment=std::get_if<Exact::Segment_3>(&*intersection)){
            ++result.segments;result.longest=std::max(result.longest,std::sqrt(CGAL::to_double(segment->squared_length())));
        }else ++result.areas;
    }
    return result;
}
