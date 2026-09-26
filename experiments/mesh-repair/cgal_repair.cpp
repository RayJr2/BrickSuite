#include "types.h"
#include <CGAL/IO/polygon_soup_io.h>
#include <CGAL/boost/graph/IO/polygon_mesh_io.h>
#include <CGAL/Polygon_mesh_processing/repair_polygon_soup.h>
#include <CGAL/Polygon_mesh_processing/orient_polygon_soup.h>
#include <CGAL/Polygon_mesh_processing/polygon_soup_to_polygon_mesh.h>
#include <CGAL/Polygon_mesh_processing/stitch_borders.h>
#include <CGAL/Polygon_mesh_processing/repair.h>
#include <CGAL/Polygon_mesh_processing/triangulate_hole.h>
#include <CGAL/Polygon_mesh_processing/self_intersections.h>
#include <CGAL/Polygon_mesh_processing/border.h>
#include "intersection_metrics.h"
#include <chrono>
#include <fstream>
#include <iostream>
namespace PMP=CGAL::Polygon_mesh_processing;
int main(int argc,char** argv)
{
    if(argc==2 && std::string(argv[1])=="--self-test"){
        std::vector<Point> p{{0,0,0},{1,0,0},{0,1,0},{0,0,1},{0,0,0}};
        Polygons f{{0,2,1},{4,1,3},{1,2,3},{2,0,3},{0,2,1},{0,0,1}};
        PMP::repair_polygon_soup(p,f);PMP::orient_polygon_soup(p,f);
        Mesh m;PMP::polygon_soup_to_polygon_mesh(p,f,m);
        return CGAL::is_closed(m)&&m.number_of_faces()==4&&!PMP::does_self_intersect(m)?0:1;
    }
    if(argc!=4){std::cerr<<"input.off output.off clean|refine|fill|inspect\n";return 2;}
    const auto start=std::chrono::steady_clock::now();
    std::vector<Point> points;Polygons polygons;
    if(!CGAL::IO::read_polygon_soup(argv[1],points,polygons))return 1;
    const std::string mode=argv[3];
    bool refined=true;
    if(mode!="inspect")PMP::repair_polygon_soup(points,polygons);
    if(mode=="refine"||mode=="fill")refined=refine(points,polygons);
    if(mode!="inspect")PMP::repair_polygon_soup(points,polygons);
    const auto beforeOrientation=points.size();
    const bool orientable=PMP::orient_polygon_soup(points,polygons);
    Mesh mesh;PMP::polygon_soup_to_polygon_mesh(points,polygons,mesh);
    std::size_t stitched=0,filled=0,added=0;
    if(mode!="inspect"){stitched=PMP::stitch_borders(mesh);PMP::remove_isolated_vertices(mesh);}
    // Filling ALL remaining loops is deliberately an explicit, separately measured
    // experiment. It may cap intentional openings and never implies acceptance.
    if(mode=="fill"||mode=="fill-clean"||mode=="local"){
        std::vector<Mesh::Halfedge_index> loops;PMP::extract_boundary_cycles(mesh,std::back_inserter(loops));
        for(auto h:loops){if(!CGAL::is_border(h,mesh))continue;std::vector<Mesh::Face_index> patch;
            PMP::triangulate_hole(mesh,h,CGAL::parameters::face_output_iterator(std::back_inserter(patch)));
            if(!patch.empty())++filled;added+=patch.size();}
    }
    const bool localSucceeded=mode=="local"?localRepair(mesh):false;
    std::vector<std::pair<Mesh::Face_index,Mesh::Face_index>> intersections;
    PMP::self_intersections(mesh,std::back_inserter(intersections));
    const auto intersectionMetrics=measureIntersections(mesh,intersections);
    std::size_t boundaries=0;for(auto h:mesh.halfedges())if(CGAL::is_border(h,mesh))++boundaries;
    CGAL::IO::write_polygon_mesh(argv[2],mesh,CGAL::parameters::stream_precision(17));
    const double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
    std::cout<<"{\"vertices\":"<<mesh.number_of_vertices()<<",\"faces\":"<<mesh.number_of_faces()
        <<",\"boundary_edges\":"<<boundaries<<",\"self_intersection_pairs\":"<<intersections.size()
        <<",\"point_contact_pairs\":"<<intersectionMetrics.points<<",\"segment_intersection_pairs\":"<<intersectionMetrics.segments
        <<",\"coplanar_area_pairs\":"<<intersectionMetrics.areas<<",\"degenerate_pairs\":"<<intersectionMetrics.degenerate
        <<",\"predicate_pairs_without_exact_intersection\":"<<intersectionMetrics.absent
        <<",\"longest_intersection_segment_mm\":"<<intersectionMetrics.longest
        <<",\"orientation_split_vertices\":"<<points.size()-beforeOrientation
        <<",\"orientable_without_splitting\":"<<(orientable?"true":"false")
        <<",\"refinement_succeeded\":"<<(refined?"true":"false")
        <<",\"local_repair_succeeded\":"<<(localSucceeded?"true":"false")
        <<",\"stitched_edges\":"<<stitched<<",\"filled_loops\":"<<filled
        <<",\"new_patch_faces\":"<<added<<",\"milliseconds\":"<<ms<<"}\n";
}
