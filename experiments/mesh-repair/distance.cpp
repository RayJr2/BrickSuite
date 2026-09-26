#include "types.h"
#include <CGAL/IO/polygon_soup_io.h>
#include <CGAL/Polygon_mesh_processing/orient_polygon_soup.h>
#include <CGAL/Polygon_mesh_processing/polygon_soup_to_polygon_mesh.h>
#include <CGAL/Polygon_mesh_processing/distance.h>
#include <iostream>
#include <iomanip>
Mesh read(const char* path)
{
    std::vector<Point> points;Polygons polygons;
    if(!CGAL::IO::read_polygon_soup(path,points,polygons))throw std::runtime_error("read failed");
    CGAL::Polygon_mesh_processing::orient_polygon_soup(points,polygons);
    Mesh mesh;CGAL::Polygon_mesh_processing::polygon_soup_to_polygon_mesh(points,polygons,mesh);return mesh;
}
int main(int argc,char** argv)
{
    if(argc!=3)return 2;
    const auto a=read(argv[1]),b=read(argv[2]);
    constexpr double error=0.001; // Fixed measurement precision, not acceptance tolerance.
    const double ab=CGAL::Polygon_mesh_processing::bounded_error_Hausdorff_distance<CGAL::Sequential_tag>(a,b,error);
    const double ba=CGAL::Polygon_mesh_processing::bounded_error_Hausdorff_distance<CGAL::Sequential_tag>(b,a,error);
    std::cout<<std::setprecision(12)<<"{\"a_to_b_mm\":"<<ab<<",\"b_to_a_mm\":"<<ba<<",\"algorithm_error_bound_mm\":"<<error<<"}\n";
}
