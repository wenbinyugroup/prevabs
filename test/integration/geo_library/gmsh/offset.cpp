#include <gmsh.h>
#include <vector>
#include <string>
#include <iostream>


int main() {
    gmsh::initialize();

    gmsh::model::add("offset");

    gmsh::logger::start();

    gmsh::model::occ::synchronize();

    // create geometry
    int p1 = gmsh::model::occ::addPoint(0, 0, 0);
    int p2 = gmsh::model::occ::addPoint(1, 0, 0);
    int p3 = gmsh::model::occ::addPoint(1, 1, 0);

    // meshing
    gmsh::model::mesh::generate(3);

    // write mesh
    gmsh::write("offset.msh");

    std::vector<std::string> log;
    gmsh::logger::get(log);
    std::cout << "Logger has recorded " << log.size() << " lines" << std::endl;
    gmsh::logger::stop();

    gmsh::finalize();

    return 0;
}
