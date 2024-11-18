
#include <cstdlib>
#include "clipper2/clipper.h"
#include "../../Utils/clipper.svg.h"
#include "../../Utils/clipper.svg.utils.h"
#include "../../Utils/Timer.h"

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

void DoSimpleShapes() 
{

  //open path offsets 
  Paths64 op1, op2;

  FillRule fr2 = FillRule::EvenOdd;
  SvgWriter svg2;
  op1.push_back(MakePath("100,100, 20,20 180,20 180,180, 20,180"));
  op2 = InflatePaths(op1, 20, JoinType::Square, EndType::Square);
  SvgAddOpenSubject(svg2, op1, fr2, false);
  SvgAddSolution(svg2, Paths64ToPathsD(op2), fr2, false);

  op1 = TranslatePaths(op1, 250, 0);
  op2 = InflatePaths(op1, 20, JoinType::Miter, EndType::Butt, 5);
  SvgAddOpenSubject(svg2, op1, fr2, false);
  SvgAddSolution(svg2, Paths64ToPathsD(op2), fr2, false);

  op1 = TranslatePaths(op1, 250, 0);
  op2 = InflatePaths(op1, 20, JoinType::Round, EndType::Round);
  SvgAddOpenSubject(svg2, op1, fr2, false);
  SvgAddSolution(svg2, Paths64ToPathsD(op2), fr2, false);

  SvgSaveToFile(svg2, "open_paths.svg", 800, 600, 20);
  System("open_paths.svg");

  //triangle offset - with large miter
  Paths64 p, pp;
  p.push_back(MakePath("30, 150, 60, 350, 0, 350"));
  pp.insert(pp.end(), p.begin(), p.end());

  for (int i = 0; i < 5; ++i)
  {
    //note the relatively large miter limit set here (10)
    p = InflatePaths(p, 5, 
      JoinType::Miter, EndType::Polygon, 10);
    pp.insert(pp.end(), p.begin(), p.end());
  }

  //rectangle offset - both squared and rounded
  p.clear();
  p.push_back(MakePath("100,30, 340,30, 340,230, 100,230"));
  pp.insert(pp.end(), p.begin(), p.end());
  //nb: using the ClipperOffest class directly here to control 
  //different join types within the same offset operation
  ClipperOffset co;
  co.AddPaths(p, JoinType::Miter, EndType::Joined);
  p = TranslatePaths(p, 120, 100);
  pp.insert(pp.end(), p.begin(), p.end());
  co.AddPaths(p, JoinType::Round, EndType::Joined);
  p = co.Execute(20);
  pp.insert(pp.end(), p.begin(), p.end());

  FillRule fr = FillRule::EvenOdd;
  SvgWriter svg;
  SvgAddSolution(svg, Paths64ToPathsD(pp), fr, false);
  SvgSaveToFile(svg, "solution_off.svg", 800, 600, 20);
  System("solution_off.svg");
}

void DoRabbit()
{
  SvgReader svg_reader;
  svg_reader.LoadFromFile("./rabbit.svg");
  PathsD p = svg_reader.GetPaths();

  JoinType jt = JoinType::Round;
  PathsD solution(p);

  while (p.size())
  {
    //nb: don't forget to scale the delta offset too!
    p = InflatePaths(p, -2.5, jt, EndType::Polygon);
    //RamerDouglasPeucker - not essential but
    //speeds up the loop and also tidies up the result
    p = RamerDouglasPeucker(p, 0.025);
    solution.reserve(solution.size() + p.size());
    copy(p.begin(), p.end(), back_inserter(solution));
  }

  FillRule fr = FillRule::EvenOdd;
  SvgWriter svg;
  SvgAddSolution(svg, solution, fr, false); 
  SvgSaveToFile(svg, "solution_off2.svg", 450, 720, 0);
  System("solution_off2.svg");
}

int main(int argc, char* argv[])
{
  DoSimpleShapes();
  DoRabbit();
}
//---------------------------------------------------------------------------
