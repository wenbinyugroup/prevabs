#include <cstdlib>
#include <iostream>
#include <vector>
#include "clipper2/clipper.h"
#include "clipper2/utils/clipper.svg.h"
#include "clipper2/utils/clipper.svg.utils.h"

using namespace std;
using namespace Clipper2Lib;

void System(const std::string& filename)
{
#ifdef _WIN32
  system(filename.c_str());
#else
  system(("firefox " + filename).c_str());
#endif
}

int main() {

    // Define the open polyline as a Path of PointD (using double precision)
    PathsD polyline, solution;
    polyline.push_back(
        MakePathD(
            {
                10.0, 10.0,
                100.0, 200.0,
                200.0, 150.0,
                300.0, 300.0
            }
        )
    );

    solution = InflatePaths(polyline, 10.0, JoinType::Miter, EndType::Square);

    FillRule fr3 = FillRule::EvenOdd;
    SvgWriter svg;
    SvgAddSolution(svg, solution, fr3, false);
    SvgSaveToFile(svg, "solution_off.svg", 800, 600, 20);
    System("solution_off.svg");

    return 0;
}
