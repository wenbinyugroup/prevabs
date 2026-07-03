// compare_resample.cpp
// ---------------------------------------------------------------------------
// Visualise the open-offset resample difference between
//   OLD: foot-of-perpendicular rebuild (one point per base vertex, M == N)
//   NEW: insert-only / raw Clipper2 miter run (preserve corners, M may != N)
//
// For each base it writes two SVGs (base in grey, offset in colour with
// vertices marked) plus an overlay, so the geometric difference that drives
// the t9/t6 mesh regression (arc) and the Z corner fix (sharp corner) is
// directly visible.
//
//   compare_<name>_old.svg     foot-resample (old global behaviour)
//   compare_<name>_new.svg     raw miter run (new behaviour, current code)
//   compare_<name>_overlay.svg both, on the same base
//
// Links the offset core (offset_clipper2.cpp + bridge + Clipper2), same as
// the other t0_offset_clipper2 harnesses. No PDCEL / gmsh.
// ---------------------------------------------------------------------------

#include "offset_clipper2.hpp"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <limits>
#include <string>
#include <vector>

using namespace dcel;  // phase 0: DCEL types moved to namespace dcel

using prevabs::geo::OffsetPolygon;
using prevabs::geo::offsetWithClipper2;

#ifndef PHASE0_OUT_DIR
#define PHASE0_OUT_DIR "."
#endif

namespace {

const OffsetPolygon* pickPrimary(const std::vector<OffsetPolygon>& polys) {
  const OffsetPolygon* best = nullptr;
  for (const auto& p : polys) {
    if (p.points.size() < 2) continue;
    if (best == nullptr || p.points.size() > best->points.size()) best = &p;
  }
  return best;
}

// NEW behaviour = current code with resample_open = true.
std::vector<SPoint2> offsetNew(const std::vector<SPoint2>& base, int side,
                               double dist) {
  auto polys = offsetWithClipper2(base, /*closed*/ false, side, dist,
                                  prevabs::geo::JoinTypeChoice::Miter, 2.0,
                                  /*resample_open*/ true, /*experimental*/ false);
  const OffsetPolygon* p = pickPrimary(polys);
  return p ? p->points : std::vector<SPoint2>{};
}

// Raw side-run (resample off) — the polyline the OLD code fed its
// foot-of-perpendicular rebuild.
std::vector<SPoint2> offsetRaw(const std::vector<SPoint2>& base, int side,
                               double dist) {
  auto polys = offsetWithClipper2(base, /*closed*/ false, side, dist,
                                  prevabs::geo::JoinTypeChoice::Miter, 2.0,
                                  /*resample_open*/ false, /*experimental*/ false);
  const OffsetPolygon* p = pickPrimary(polys);
  return p ? p->points : std::vector<SPoint2>{};
}

// OLD behaviour = foot-of-perpendicular of every base vertex onto the raw run
// (reproduces the replaced resample block: one point per base vertex).
std::vector<SPoint2> footResample(const std::vector<SPoint2>& raw,
                                  const std::vector<SPoint2>& base) {
  std::vector<SPoint2> out;
  if (raw.size() < 2) return out;
  for (const auto& q : base) {
    double best_d = std::numeric_limits<double>::infinity();
    SPoint2 best(0, 0);
    for (std::size_t s = 0; s + 1 < raw.size(); ++s) {
      const double ax = raw[s].x(), ay = raw[s].y();
      const double bx = raw[s + 1].x(), by = raw[s + 1].y();
      const double dx = bx - ax, dy = by - ay;
      const double len2 = dx * dx + dy * dy;
      double u = len2 > 0 ? ((q.x() - ax) * dx + (q.y() - ay) * dy) / len2 : 0;
      if (u < 0) u = 0; if (u > 1) u = 1;
      const double fx = ax + u * dx, fy = ay + u * dy;
      const double d = std::hypot(q.x() - fx, q.y() - fy);
      if (d < best_d) { best_d = d; best = SPoint2(fx, fy); }
    }
    if (out.empty() ||
        std::hypot(out.back().x() - best.x(), out.back().y() - best.y()) > 1e-12)
      out.push_back(best);
  }
  return out;
}

// pick the side (+1/-1) that yields a non-empty offset run
int autoSide(const std::vector<SPoint2>& base, double dist) {
  if (!offsetNew(base, +1, dist).empty()) return +1;
  return -1;
}

struct Box { double minx, miny, maxx, maxy, scale, W, H; };

Box computeBox(const std::vector<std::vector<SPoint2>>& curves) {
  Box b{1e30, 1e30, -1e30, -1e30, 1, 0, 0};
  for (const auto& c : curves)
    for (const auto& p : c) {
      b.minx = std::min(b.minx, p.x()); b.maxx = std::max(b.maxx, p.x());
      b.miny = std::min(b.miny, p.y()); b.maxy = std::max(b.maxy, p.y());
    }
  const double pad = 0.08 * std::max(b.maxx - b.minx, b.maxy - b.miny) + 0.05;
  b.minx -= pad; b.miny -= pad; b.maxx += pad; b.maxy += pad;
  const double w = b.maxx - b.minx, h = b.maxy - b.miny;
  b.scale = 700.0 / std::max(w, h);
  b.W = w * b.scale; b.H = h * b.scale;
  return b;
}

void polyline(std::ofstream& f, const Box& b, const std::vector<SPoint2>& c,
              const char* color, double width, bool dots) {
  auto X = [&](double x) { return (x - b.minx) * b.scale; };
  auto Y = [&](double y) { return b.H - (y - b.miny) * b.scale; };
  f << "<polyline fill='none' stroke='" << color << "' stroke-width='" << width
    << "' points='";
  for (const auto& p : c) f << X(p.x()) << "," << Y(p.y()) << " ";
  f << "'/>\n";
  if (dots)
    for (const auto& p : c)
      f << "<circle cx='" << X(p.x()) << "' cy='" << Y(p.y())
        << "' r='3' fill='" << color << "'/>\n";
}

void writeSvg(const std::string& path, const std::string& title,
              const std::vector<SPoint2>& base,
              const std::vector<SPoint2>& curve, const char* color) {
  Box b = computeBox({base, curve});
  std::ofstream f(path);
  f << "<svg xmlns='http://www.w3.org/2000/svg' width='" << (b.W + 20)
    << "' height='" << (b.H + 40) << "' font-family='sans-serif'>\n";
  f << "<rect width='100%' height='100%' fill='white'/>\n";
  f << "<text x='10' y='20' font-size='14'>" << title << "</text>\n";
  f << "<g transform='translate(10,30)'>\n";
  polyline(f, b, base, "#bbb", 1.2, true);
  polyline(f, b, curve, color, 2.0, true);
  f << "</g>\n</svg>\n";
}

void writeOverlay(const std::string& path, const std::string& title,
                  const std::vector<SPoint2>& base,
                  const std::vector<SPoint2>& oldc,
                  const std::vector<SPoint2>& newc) {
  Box b = computeBox({base, oldc, newc});
  std::ofstream f(path);
  f << "<svg xmlns='http://www.w3.org/2000/svg' width='" << (b.W + 20)
    << "' height='" << (b.H + 56) << "' font-family='sans-serif'>\n";
  f << "<rect width='100%' height='100%' fill='white'/>\n";
  f << "<text x='10' y='20' font-size='14'>" << title << "</text>\n";
  f << "<text x='10' y='38' font-size='12' fill='#1f77b4'>old foot-resample</text>"
    << "<text x='160' y='38' font-size='12' fill='#d62728'>new raw-miter</text>\n";
  f << "<g transform='translate(10,46)'>\n";
  polyline(f, b, base, "#bbb", 1.2, true);
  polyline(f, b, oldc, "#1f77b4", 2.0, true);
  polyline(f, b, newc, "#d62728", 2.0, true);
  f << "</g>\n</svg>\n";
}

void runCase(const std::string& name, const std::vector<SPoint2>& base,
             double dist) {
  const int side = autoSide(base, dist);
  const std::vector<SPoint2> raw = offsetRaw(base, side, dist);
  const std::vector<SPoint2> oldc = footResample(raw, base);
  const std::vector<SPoint2> newc = offsetNew(base, side, dist);
  const std::string dir = PHASE0_OUT_DIR;
  char buf[128];
  std::snprintf(buf, sizeof(buf),
                "%s: base N=%zu  old(foot) M=%zu  new(raw-miter) M=%zu  dist=%.3g",
                name.c_str(), base.size(), oldc.size(), newc.size(), dist);
  std::printf("%s\n", buf);
  writeSvg(dir + "/compare_" + name + "_old.svg",
           name + " OLD foot-resample (M=" + std::to_string(oldc.size()) + ")",
           base, oldc, "#1f77b4");
  writeSvg(dir + "/compare_" + name + "_new.svg",
           name + " NEW raw-miter (M=" + std::to_string(newc.size()) + ")",
           base, newc, "#d62728");
  writeOverlay(dir + "/compare_" + name + "_overlay.svg", buf, base, oldc, newc);
}

// quarter arc, radius r, `steps` degrees per facet, centred at origin
std::vector<SPoint2> makeArc(double r, double step_deg) {
  std::vector<SPoint2> v;
  for (double a = 0.0; a <= 90.0 + 1e-9; a += step_deg) {
    const double rad = a * 3.141592653589793 / 180.0;
    v.emplace_back(r * std::cos(rad), r * std::sin(rad));
  }
  return v;
}

}  // namespace

int main() {
  std::printf("=== compare_resample: old foot vs new raw-miter ===\n");
  std::printf("out dir: %s\n", PHASE0_OUT_DIR);

  // 1) The regressing case: a fine arc (mirrors circle_param_layup_range's
  //    5deg sub-arc segments). New keeps Clipper2's miter run; old smooths to
  //    feet -> the positional/count difference that breaks Gmsh meshing.
  runCase("arc_fine_5deg", makeArc(2.0, 5.0), 0.25);

  // 2) Coarser arc — makes the per-vertex miter-vs-foot offset visible.
  runCase("arc_coarse_15deg", makeArc(2.0, 15.0), 0.30);

  // 3) The Z-style sharp corner the change is meant to FIX: old foot collapses
  //    the convex corner, new keeps the clean miter apex.
  runCase("sharp_corner",
          {SPoint2(-0.5, 0.5), SPoint2(0.0, 0.5), SPoint2(0.0, -0.5),
           SPoint2(0.5, -0.5)},
          0.07);

  std::printf("\nSVGs written to %s\n", PHASE0_OUT_DIR);
  return 0;
}
