#include "types.h"
#include <CGAL/Polygon_mesh_processing/repair_self_intersections.h>
bool localRepair(Mesh& mesh)
{
    return CGAL::Polygon_mesh_processing::experimental::remove_self_intersections(mesh);
}
