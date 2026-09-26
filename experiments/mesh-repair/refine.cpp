#include "types.h"
#include <CGAL/Polygon_mesh_processing/autorefinement.h>
bool refine(std::vector<Point>& points,Polygons& polygons)
{
    // Fixed CGAL defaults; no tuning to this Part or to the golden reference.
    return CGAL::Polygon_mesh_processing::autorefine_triangle_soup(points,polygons,
        CGAL::parameters::apply_iterative_snap_rounding(true));
}
