// Isolated experiment only. Generated patches never receive source ownership.
#include <geogram/basic/common.h>
#include <geogram/basic/command_line.h>
#include <geogram/basic/command_line_args.h>
#include <geogram/mesh/mesh.h>
#include <geogram/mesh/mesh_io.h>
#include <geogram/mesh/mesh_repair.h>
#include <geogram/mesh/mesh_fill_holes.h>
#include <geogram/mesh/mesh_surface_intersection.h>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>

using namespace GEO;

static void repair(Mesh& mesh, const std::string& mode, double epsilon) {
    mesh_repair(mesh, MESH_REPAIR_DEFAULT, epsilon);
    if (mode == "clean") return;
    if (mode == "fill-outer" || mode == "fill-outer-simplify") {
        // Explicitly generated faces; unlimited holes is an experiment, not policy.
        fill_holes(mesh, std::numeric_limits<double>::max());
    }
    {
        MeshSurfaceIntersection intersection(mesh);
        intersection.set_verbose(false);
        intersection.set_fine_verbose(false);
        intersection.set_detect_intersecting_neighbors(true);
        intersection.set_radial_sort(mode != "arrange");
        intersection.set_normalize(false);
        intersection.intersect();
        if (mode != "arrange") intersection.remove_internal_shells();
        if (mode == "fill-outer-simplify") intersection.simplify_coplanar_facets(0.0);
    }
    // After exact intersections, follow the upstream example's exact colocation.
    // Re-welding close but distinct intersection vertices would change topology.
    mesh_repair(mesh, MESH_REPAIR_DEFAULT, 0.0);
    if (mode == "outer-fill") {
        fill_holes(mesh, std::numeric_limits<double>::max());
        mesh_repair(mesh, MESH_REPAIR_DEFAULT, 0.0);
    }
}

static bool saveExactOff(const Mesh& mesh, const char* path) {
    // Preserve double coordinates; coarse ASCII formatting can create intersections.
    std::ofstream out(path);
    out << std::setprecision(17) << "OFF\n" << mesh.vertices.nb() << ' '
        << mesh.facets.nb() << " 0\n";
    for (index_t v = 0; v < mesh.vertices.nb(); ++v) {
        const double* p = mesh.vertices.point_ptr(v);
        out << p[0] << ' ' << p[1] << ' ' << p[2] << '\n';
    }
    for (index_t f = 0; f < mesh.facets.nb(); ++f) {
        out << mesh.facets.nb_vertices(f);
        for (index_t c = 0; c < mesh.facets.nb_vertices(f); ++c)
            out << ' ' << mesh.facets.vertex(f, c);
        out << '\n';
    }
    out.flush();
    return bool(out);
}

int main(int argc, char** argv) {
    initialize();
    CmdLine::import_arg_group("standard");
    CmdLine::import_arg_group("algo");
    // Fixed reproducible thread budget, independent of host core count.
    CmdLine::set_arg("sys:max_threads", 1);
    try {
        if (argc == 2 && std::string(argv[1]) == "--self-test") {
            Mesh mesh;
            for (const vec3& p : {vec3(0,0,0),vec3(1,0,0),vec3(0,1,0),vec3(0,0,1)})
                mesh.vertices.create_vertex(p.data());
            mesh.facets.create_triangle(0,2,1);
            mesh.facets.create_triangle(0,1,3);
            mesh.facets.create_triangle(1,2,3);
            mesh.facets.create_triangle(2,0,3);
            repair(mesh, "outer", 0.0);
            if (mesh.vertices.nb() != 4 || mesh.facets.nb() != 4) return 2;
            for (index_t c = 0; c < mesh.facet_corners.nb(); ++c)
                if (mesh.facet_corners.adjacent_facet(c) == NO_FACET) return 3;
            return 0;
        }
        if (argc != 5) {
            std::cerr << "input.off output.off clean|arrange|outer|fill-outer|outer-fill|fill-outer-simplify epsilon_mm\n";
            return 1;
        }
        const std::string mode(argv[3]);
        if (mode != "clean" && mode != "arrange" && mode != "outer" &&
            mode != "fill-outer" && mode != "outer-fill" && mode != "fill-outer-simplify") return 1;
        const double epsilon = std::stod(argv[4]);
        if (epsilon < 0 || !std::isfinite(epsilon)) return 1;
        Mesh mesh;
        if (!mesh_load(argv[1], mesh)) return 1;
        const auto start = std::chrono::steady_clock::now();
        repair(mesh, mode, epsilon);
        const auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
        if (!saveExactOff(mesh, argv[2])) return 1;
        std::cout << "{\"repair_seconds\":" << elapsed << ",\"vertices\":" << mesh.vertices.nb()
                  << ",\"faces\":" << mesh.facets.nb() << "}\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
