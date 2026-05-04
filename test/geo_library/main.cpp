#include <iostream>
#include <vector>
#include <cmath>
#include <clipper2/clipper.h>
#include "clipper2/utils/clipper.svg.h"
#include "clipper2/utils/clipper.svg.utils.h"
#include <gmsh.h>

using namespace Clipper2Lib;

void System(const std::string& filename)
{
#ifdef _WIN32
  system(filename.c_str());
#else
  system(("firefox " + filename).c_str());
#endif
}

// Function to calculate the tangent direction of a line segment
std::pair<double, double> calculateTangent(const PathD& baseCurve, size_t index) {
    if (index >= baseCurve.size() - 1) {
        throw std::out_of_range("Index out of range for tangent calculation");
    }
    double dx = baseCurve[index + 1].x - baseCurve[index].x;
    double dy = baseCurve[index + 1].y - baseCurve[index].y;
    double length = std::sqrt(dx * dx + dy * dy);
    return {dx / length, dy / length};
}

// Function to calculate the normal direction (outward normal)
std::pair<double, double> calculateNormal(const std::pair<double, double>& tangent) {
    return {-tangent.second, tangent.first}; // Rotate tangent by 90 degrees
}

// Function to calculate the centroid of a 2D element
std::pair<double, double> calculateElementCentroid(const std::vector<std::size_t>& nodeTags, 
                                                  const std::vector<double>& nodeCoords) {
    double centroidX = 0.0, centroidY = 0.0;
    size_t numNodes = nodeTags.size();

    for (size_t i = 0; i < numNodes; ++i) {
        centroidX += nodeCoords[3 * i];     // x-coordinate
        centroidY += nodeCoords[3 * i + 1]; // y-coordinate
    }

    centroidX /= numNodes;
    centroidY /= numNodes;

    return {centroidX, centroidY};
}

int main(int argc, char **argv) {
    // Step 1: User provides a list of (x, y) coordinates to create a base curve
    std::vector<std::pair<double, double>> userPoints = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
    PathD baseCurve;
    for (const auto& point : userPoints) {
        baseCurve.push_back(PointD(point.first, point.second));
    }

    // Step 2: User provides an offset distance
    double offsetDistance = 0.1;
    PathsD offsetCurves = InflatePaths(PathsD{baseCurve}, offsetDistance, JoinType::Round, EndType::Butt, 2, 4, 0.0001);

    FillRule fr3 = FillRule::EvenOdd;
    SvgWriter svg;
    PathsD base_curves;
    base_curves.push_back(baseCurve);
    SvgAddOpenSubject(svg, base_curves, fr3, false);
    SvgAddSolution(svg, offsetCurves, fr3, false);
    SvgSaveToFile(svg, "test.svg", 800, 600, 20);
    System("test.svg");

    if (offsetCurves.empty()) {
        std::cerr << "Offset operation failed!" << std::endl;
        return 1;
    }
    PathD offsetCurve = offsetCurves[0];

    // Step 3: Link the corresponding ends of the two curves to create a closed polygon
    PathD polygon = baseCurve;
    polygon.insert(polygon.end(), offsetCurve.rbegin(), offsetCurve.rend());

    // Step 4: Mesh the polygon using GMSH
    gmsh::initialize(argc, argv);
    gmsh::model::add("2DGeometry");

    // Add the polygon as a GMSH curve loop
    std::vector<int> pointTags;
    for (const auto& point : polygon) {
        int tag = gmsh::model::geo::addPoint(point.x, point.y, 0);
        pointTags.push_back(tag);
    }

    std::vector<int> lineTags;
    for (size_t i = 0; i < pointTags.size(); ++i) {
        int tag = gmsh::model::geo::addLine(pointTags[i], pointTags[(i + 1) % pointTags.size()]);
        lineTags.push_back(tag);
    }

    int curveLoopTag = gmsh::model::geo::addCurveLoop(lineTags);
    int surfaceTag = gmsh::model::geo::addPlaneSurface({curveLoopTag});

    gmsh::model::geo::synchronize();
    gmsh::write("test.geo_unrolled");

    gmsh::model::mesh::generate(2);

    // Step 5: Calculate local orientation for each element
    std::vector<std::size_t> elementTags, elementNodeTags;
    // std::vector<double> elementCenters;
    gmsh::model::mesh::getElementsByType(2, elementTags, elementNodeTags);

    for (size_t i = 0; i < elementTags.size(); ++i) {
        std::vector<double> nodes_coords, param_coords;

        std::vector<std::size_t> nodeTags;
        std::vector<double> nodeCoords, nodeParams;
        gmsh::model::mesh::getNodes(
            nodeTags, nodeCoords, nodeParams, 2, elementTags[i]);


        auto centroid = calculateElementCentroid(nodeTags, nodeCoords);

        double x = centroid.first;
        double y = centroid.second;

        // Find the closest line segment on the base curve
        size_t closestSegment = 0;
        double minDistance = std::numeric_limits<double>::max();
        for (size_t j = 0; j < baseCurve.size() - 1; ++j) {
            double x1 = baseCurve[j].x;
            double y1 = baseCurve[j].y;
            double x2 = baseCurve[j + 1].x;
            double y2 = baseCurve[j + 1].y;

            double dx = x2 - x1;
            double dy = y2 - y1;
            double lengthSquared = dx * dx + dy * dy;
            double t = std::max(0.0, std::min(1.0, ((x - x1) * dx + (y - y1) * dy) / lengthSquared));
            double projX = x1 + t * dx;
            double projY = y1 + t * dy;
            double distance = std::sqrt((x - projX) * (x - projX) + (y - projY) * (y - projY));

            if (distance < minDistance) {
                minDistance = distance;
                closestSegment = j;
            }
        }

        // Calculate local x2 (tangent) and x1 (normal) axes
        auto tangent = calculateTangent(baseCurve, closestSegment);
        auto normal = calculateNormal(tangent);

        std::cout << "Element " << elementTags[i] << ":\n";
        std::cout << "  Local x2 (tangent): (" << tangent.first << ", " << tangent.second << ")\n";
        std::cout << "  Local x1 (normal): (" << normal.first << ", " << normal.second << ")\n";
        std::cout << "  Local x3 (cross product): (0, 0, 1)\n";
    }

    gmsh::write("test.msh");

    // Visualize the mesh in GMSH
    // gmsh::fltk::run();
    gmsh::finalize();

    return 0;
}