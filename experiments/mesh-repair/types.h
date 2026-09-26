#pragma once
#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/Surface_mesh.h>
#include <vector>
using Kernel=CGAL::Exact_predicates_inexact_constructions_kernel;
using Point=Kernel::Point_3;
using Mesh=CGAL::Surface_mesh<Point>;
using Polygons=std::vector<std::vector<std::size_t>>;
bool refine(std::vector<Point>& points,Polygons& polygons);
bool localRepair(Mesh& mesh);
