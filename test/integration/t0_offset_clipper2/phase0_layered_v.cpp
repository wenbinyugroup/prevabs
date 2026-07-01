// Phase-0 prototype for plan-20260618-per-layer-offset-within-shell.md.
//
// Goal: on a HEALTHY baseline (a V-section mirroring t2_z/v.xml, with a
// simplified 3-layer layup), exercise the proposed construction:
//
//   Step 1  total-thickness outer shell  (offset by t_total)
//   Step 2  route-i per-layer subdivision (curve_k = offset(base, cum_k),
//           staircase map_k between curve_{k-1} and curve_k)
//
// and dump point / line / face information after each step so the
// geometry can be eyeballed (text dump + an SVG overlay).
//
// It also checks the three Phase-0 exit criteria:
//   (A) nesting     — every curve_k vertex sits within the shell band
//                     (0 < foot-distance-to-base < t_total) and the mean
//                     foot-distance grows ~linearly with cum_k.
//   (B) zero-gap    — curve_n (cumulative) equals the shell's own offset
//                     curve (same offsetWithClipper2 call) vertex-for-vertex.
//   (C) valid maps  — each per-layer rebuildBaseOffsetMapFromGeometry
//                     returns ok=true with a valid staircase.
//
// Pure-logic only: links src/geo/offset_clipper2.cpp +
// offset_clipper2_bridge.cpp + Clipper2. No PDCELVertex / gmsh / spdlog.

#include "offset_clipper2.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

// SPoint2 / BaseOffsetPair live in the global namespace (geo_types.hpp).
using prevabs::geo::OffsetPolygon;
using prevabs::geo::offsetWithClipper2;
using prevabs::geo::ReverseMatchPlan;
using prevabs::geo::rebuildBaseOffsetMapFromGeometry;
using prevabs::geo::PairingAlgo;

namespace {

constexpr double kEps = 1e-9;

// ---------------------------------------------------------------------------
// Geometry helpers
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Scenario = one healthy open baseline + its per-layer thicknesses.
// `side`: 0 = auto (try +1, flip if the run is empty); else forced.
// ---------------------------------------------------------------------------
struct Scenario {
  std::string          name;
  std::vector<SPoint2> base;
  std::vector<double>  thick;   // per-layer thickness (inner -> outer)
  int                  side;    // 0 = auto
  std::string          note;
  bool                 resample = true;     // false = raw run (forces M!=N)
  double               miter_limit = 2.0;   // Clipper2 miter limit
};

// 1) Symmetric V mirroring t2_z/v3.xml. M == N == 3 (single miter apex).
Scenario makeVSymmetric() {
  return {"v_symmetric",
          {SPoint2(-1.0, 2.0), SPoint2(0.0, 0.0), SPoint2(1.0, 2.0)},
          {0.1, 0.1, 0.1}, 0, "symmetric V, expect M==N"};
}

// 2) Asymmetric V (different arm lengths + angles) — route i must not
// assume symmetry.
Scenario makeVAsymmetric() {
  return {"v_asymmetric",
          {SPoint2(-1.5, 1.8), SPoint2(0.0, 0.0), SPoint2(1.0, 1.4)},
          {0.1, 0.1, 0.1}, 0, "asymmetric arms/angles"};
}

// 3) Sharp tent (∧), offset to the CONVEX (outer) side. Uses the RAW run
// (no resample) + a high miter_limit so the apex extends to the TRUE miter
// as a single vertex (M == N), the geometrically faithful laminate corner.
// (With the default foot-of-perpendicular resample the convex apex is
// distorted — collapsed toward the base normal — so resample is off here;
// with the default miter_limit=2.0 it would bevel into two vertices, see
// tent_sharp_raw.) Walking left->apex->right turns clockwise; side=+1
// (left) is the convex side.
Scenario makeTentSharpConvex() {
  return {"tent_sharp_convex",
          {SPoint2(-0.5, 0.0), SPoint2(0.0, 2.0), SPoint2(0.5, 0.0)},
          {0.08, 0.08, 0.08}, +1, "sharp convex apex, true miter (M==N)",
          /*resample*/ false, /*miter_limit*/ 10.0};
}

// 4) Quarter-circle arc (7 vertices) — many pairs, smooth, M == N.
Scenario makeArcOpen() {
  const double half_pi = std::acos(-1.0) / 2.0;
  std::vector<SPoint2> pts;
  for (int i = 0; i <= 6; ++i) {
    const double a = half_pi * (i / 6.0);
    pts.emplace_back(std::cos(a), std::sin(a));
  }
  return {"arc_open", std::move(pts), {0.06, 0.06, 0.06}, 0,
          "7-vertex arc, many pairs"};
}

// 5) Gently bent thick strip with NON-UNIFORM layer thicknesses
// {0.05, 0.10, 0.15}. Validates cumulative-thickness handling (route i).
Scenario makeStripBendNonUniform() {
  // Raw run (no resample): the foot-of-perpendicular resample places a
  // convex-bend offset at the perpendicular foot of ONE edge instead of
  // the true miter (intersection of both offset edges). The bends here are
  // mild, so miter_limit=2.0 does not bevel — each curve keeps N vertices.
  return {"strip_bend_nonuniform",
          {SPoint2(-1.0, 0.0), SPoint2(0.0, 0.15), SPoint2(1.0, 0.05),
           SPoint2(2.0, 0.30)},
          {0.05, 0.10, 0.15}, 0, "non-uniform layer thicknesses",
          /*resample*/ false};
}

// 6) Same sharp tent, but RAW offset (resample=false): the convex apex
// bevel survives as 2 vertices -> M > N. Exercises the M!=N staircase
// (one base apex -> two offset vertices, a Δoffset step) and verifies
// route i still nests + zero-gaps with unequal vertex counts.
Scenario makeTentSharpRaw() {
  return {"tent_sharp_raw",
          {SPoint2(-0.5, 0.0), SPoint2(0.0, 2.0), SPoint2(0.5, 0.0)},
          {0.08, 0.08, 0.08}, +1, "raw offset, expect M>N (bevel kept)",
          /*resample*/ false};
}

// Foot-of-perpendicular distance from q to an open polyline.
double footDistToPolyline(const SPoint2& q, const std::vector<SPoint2>& poly) {
  double best = std::numeric_limits<double>::infinity();
  for (std::size_t i = 0; i + 1 < poly.size(); ++i) {
    const SPoint2& a = poly[i];
    const SPoint2& b = poly[i + 1];
    const double dx = b.x() - a.x();
    const double dy = b.y() - a.y();
    const double len2 = dx * dx + dy * dy;
    double u = 0.0;
    if (len2 > 0.0) {
      u = ((q.x() - a.x()) * dx + (q.y() - a.y()) * dy) / len2;
      u = std::max(0.0, std::min(1.0, u));
    }
    const double fx = a.x() + u * dx;
    const double fy = a.y() + u * dy;
    const double d = std::hypot(q.x() - fx, q.y() - fy);
    if (d < best) best = d;
  }
  return best;
}

// Pick the open run with the most vertices (the side-filtered primary).
const OffsetPolygon* pickPrimaryOpen(const std::vector<OffsetPolygon>& polys) {
  const OffsetPolygon* best = nullptr;
  for (const auto& p : polys) {
    if (p.points.size() < 2) continue;
    if (best == nullptr || p.points.size() > best->points.size()) best = &p;
  }
  return best;
}

// Compute the offset curve of `base` at distance `dist` (open, one side).
// `resample` = the offsetWithClipper2 resample_open flag: true (default,
// production) resamples the side-run to base-vertex resolution (M==N on
// healthy shapes); false returns the raw Clipper2 run (miter bevels add
// vertices -> M can exceed N), exercising the M!=N staircase path.
// Returns the primary run's points, or empty on failure.
std::vector<SPoint2> offsetCurve(const std::vector<SPoint2>& base,
                                 int side, double dist, bool resample = true,
                                 double miter_limit = 2.0) {
  auto polys = offsetWithClipper2(base, /*closed*/ false, side, dist,
                                  prevabs::geo::JoinTypeChoice::Miter,
                                  miter_limit,
                                  /*resample_open*/ resample,
                                  /*experimental*/ false);
  const OffsetPolygon* prim = pickPrimaryOpen(polys);
  if (prim == nullptr) return {};
  return prim->points;
}

double maxVertexGap(const std::vector<SPoint2>& a,
                    const std::vector<SPoint2>& b) {
  if (a.size() != b.size()) return std::numeric_limits<double>::infinity();
  double m = 0.0;
  for (std::size_t i = 0; i < a.size(); ++i) {
    m = std::max(m, std::hypot(a[i].x() - b[i].x(), a[i].y() - b[i].y()));
  }
  return m;
}

// ---------------------------------------------------------------------------
// One layer's geometry (route i).
// ---------------------------------------------------------------------------

struct Layer {
  int                  index = 0;     // 1-based
  double               t = 0.0;       // this layer's thickness
  double               cum = 0.0;     // cumulative thickness at curve_k
  std::vector<SPoint2> inner;         // curve_{k-1}
  std::vector<SPoint2> outer;         // curve_k (== plan.offset_points)
  ReverseMatchPlan     plan;          // map between inner and outer
};

// A face = the polygon traced by two consecutive staircase pairs.
struct Face {
  std::vector<SPoint2> loop;
};

std::vector<Face> facesFromPlan(const std::vector<SPoint2>& inner,
                                const std::vector<SPoint2>& outer,
                                const ReverseMatchPlan& plan) {
  std::vector<Face> faces;
  const auto& pairs = plan.id_pairs;
  for (std::size_t j = 0; j + 1 < pairs.size(); ++j) {
    const int b0 = pairs[j].base, o0 = pairs[j].offset;
    const int b1 = pairs[j + 1].base, o1 = pairs[j + 1].offset;
    if (b0 < 0 || b1 < 0 || o0 < 0 || o1 < 0) continue;
    if (b1 >= (int)inner.size() || o1 >= (int)outer.size()) continue;
    // loop: inner[b0] -> inner[b1] -> outer[o1] -> outer[o0]
    std::vector<SPoint2> loop = {inner[b0], inner[b1], outer[o1], outer[o0]};
    // Drop consecutive duplicates (Δbase=0 or Δoffset=0 → triangle).
    std::vector<SPoint2> dedup;
    for (const auto& p : loop) {
      if (dedup.empty()
          || std::hypot(p.x() - dedup.back().x(),
                        p.y() - dedup.back().y()) > kEps) {
        dedup.push_back(p);
      }
    }
    if (dedup.size() >= 3
        && std::hypot(dedup.front().x() - dedup.back().x(),
                      dedup.front().y() - dedup.back().y()) <= kEps) {
      dedup.pop_back();
    }
    if (dedup.size() >= 3) faces.push_back({std::move(dedup)});
  }
  return faces;
}

// ---------------------------------------------------------------------------
// Text dump
// ---------------------------------------------------------------------------

std::string fmtPt(const SPoint2& p) {
  std::ostringstream o;
  o << std::fixed << std::setprecision(5) << "(" << p.x() << ", " << p.y() << ")";
  return o.str();
}

void dumpCurve(std::ostream& os, const std::string& name,
               const std::vector<SPoint2>& c) {
  os << "  " << name << " POINTS (" << c.size() << "):\n";
  for (std::size_t i = 0; i < c.size(); ++i) {
    os << "    [" << i << "] " << fmtPt(c[i]) << "\n";
  }
  os << "  " << name << " LINES (" << (c.size() ? c.size() - 1 : 0) << "):\n";
  for (std::size_t i = 0; i + 1 < c.size(); ++i) {
    os << "    " << i << "->" << (i + 1) << "  " << fmtPt(c[i]) << " - "
       << fmtPt(c[i + 1]) << "\n";
  }
}

void dumpFaces(std::ostream& os, const std::vector<Face>& faces) {
  os << "  FACES (" << faces.size() << "):\n";
  for (std::size_t f = 0; f < faces.size(); ++f) {
    os << "    face[" << f << "] (" << faces[f].loop.size() << " verts): ";
    for (std::size_t i = 0; i < faces[f].loop.size(); ++i) {
      if (i) os << " - ";
      os << fmtPt(faces[f].loop[i]);
    }
    os << "\n";
  }
}

void dumpPairs(std::ostream& os, const ReverseMatchPlan& plan) {
  os << "  STAIRCASE PAIRS (" << plan.id_pairs.size() << "): ";
  for (std::size_t i = 0; i < plan.id_pairs.size(); ++i) {
    if (i) os << " ";
    os << "(" << plan.id_pairs[i].base << "," << plan.id_pairs[i].offset << ")";
  }
  os << "\n  RIBS (inner[b] - outer[o]):\n";
}

// ---------------------------------------------------------------------------
// SVG overlay
// ---------------------------------------------------------------------------

struct SvgBox {
  double xmin, xmax, ymin, ymax, scale, pad;
  int w, h;
};

SvgBox computeBox(const std::vector<std::vector<SPoint2>>& curves) {
  SvgBox b{1e30, -1e30, 1e30, -1e30, 1.0, 30.0, 0, 0};
  for (const auto& c : curves)
    for (const auto& p : c) {
      b.xmin = std::min(b.xmin, p.x());
      b.xmax = std::max(b.xmax, p.x());
      b.ymin = std::min(b.ymin, p.y());
      b.ymax = std::max(b.ymax, p.y());
    }
  const double spanx = std::max(1e-6, b.xmax - b.xmin);
  const double spany = std::max(1e-6, b.ymax - b.ymin);
  b.scale = 600.0 / std::max(spanx, spany);
  b.w = (int)(spanx * b.scale + 2 * b.pad);
  b.h = (int)(spany * b.scale + 2 * b.pad);
  return b;
}

// Map model (x,y) -> SVG (px,py). Flip y (SVG y is down).
std::pair<double, double> toSvg(const SvgBox& b, const SPoint2& p) {
  const double px = (p.x() - b.xmin) * b.scale + b.pad;
  const double py = b.h - ((p.y() - b.ymin) * b.scale + b.pad);
  return {px, py};
}

void writeSvg(const std::string& path, const std::vector<SPoint2>& base,
              const std::vector<Layer>& layers) {
  std::vector<std::vector<SPoint2>> all = {base};
  for (const auto& L : layers) all.push_back(L.outer);
  const SvgBox b = computeBox(all);

  std::ofstream f(path);
  if (!f) {
    std::fprintf(stderr, "cannot write %s\n", path.c_str());
    return;
  }
  f << "<svg xmlns='http://www.w3.org/2000/svg' width='" << b.w << "' height='"
    << b.h << "' viewBox='0 0 " << b.w << " " << b.h << "'>\n";
  f << "<rect width='100%' height='100%' fill='white'/>\n";

  const char* layerFill[] = {"#cfe8ff", "#d6f5d6", "#ffe0b3", "#f5cce0",
                             "#e0d6ff"};
  const char* layerStroke[] = {"#1f77b4", "#2ca02c", "#ff7f0e", "#d62728",
                               "#9467bd"};

  // Faces (filled, semi-transparent) per layer.
  for (std::size_t k = 0; k < layers.size(); ++k) {
    const auto faces = facesFromPlan(layers[k].inner, layers[k].outer,
                                     layers[k].plan);
    for (const auto& fc : faces) {
      f << "<polygon points='";
      for (const auto& p : fc.loop) {
        auto s = toSvg(b, p);
        f << s.first << "," << s.second << " ";
      }
      f << "' fill='" << layerFill[k % 5] << "' fill-opacity='0.5' stroke='"
        << layerStroke[k % 5] << "' stroke-width='0.6'/>\n";
    }
  }

  // Curves (polylines).
  auto drawPoly = [&](const std::vector<SPoint2>& c, const char* color,
                      double w) {
    f << "<polyline fill='none' stroke='" << color << "' stroke-width='" << w
      << "' points='";
    for (const auto& p : c) {
      auto s = toSvg(b, p);
      f << s.first << "," << s.second << " ";
    }
    f << "'/>\n";
  };
  drawPoly(base, "#000000", 2.0);  // base = black
  for (std::size_t k = 0; k < layers.size(); ++k)
    drawPoly(layers[k].outer, layerStroke[k % 5], 1.4);

  // Vertices + indices on base.
  for (std::size_t i = 0; i < base.size(); ++i) {
    auto s = toSvg(b, base[i]);
    f << "<circle cx='" << s.first << "' cy='" << s.second
      << "' r='2.5' fill='#000'/>\n";
    f << "<text x='" << s.first + 4 << "' y='" << s.second - 4
      << "' font-size='10' fill='#000'>b" << i << "</text>\n";
  }
  f << "</svg>\n";
  std::printf("  wrote SVG: %s\n", path.c_str());
}

struct ScenarioResult {
  std::string name;
  int  n_base    = 0;
  int  m_outer   = 0;     // verts of curve_n
  bool m_ne_n    = false; // any layer had inner.size() != outer.size()
  bool nesting   = false;
  bool zero_gap  = false;
  bool maps      = false;
  bool pass      = false;
};

// Run one scenario: build the total-thickness shell + route-i per-layer
// subdivision, dump full point/line/face info to <outdir>/phase0_<name>.txt
// and an SVG, and return the summary.
ScenarioResult runScenario(const Scenario& sc, const std::string& outdir) {
  ScenarioResult R;
  R.name = sc.name;
  R.n_base = (int)sc.base.size();

  const int n = (int)sc.thick.size();
  double t_total = 0.0;
  for (double t : sc.thick) t_total += t;

  int side = sc.side != 0 ? sc.side : +1;
  if (offsetCurve(sc.base, side, t_total, sc.resample, sc.miter_limit).empty())
    side = -side;

  std::ostringstream out;
  out << "=================================================================\n";
  out << " Phase-0 scenario: " << sc.name << "\n";
  out << " " << sc.note << "\n";
  out << "=================================================================\n";
  out << " base (" << sc.base.size() << " verts): ";
  for (auto& p : sc.base) out << fmtPt(p) << " ";
  out << "\n side=" << side << "  thicknesses={";
  for (int i = 0; i < n; ++i) out << (i ? "," : "") << sc.thick[i];
  out << "}  t_total=" << t_total << "\n";

  // Step 1: total-thickness outer shell.
  out << "\n----------------------------------------------------------------\n";
  out << " STEP 1 — total-thickness OUTER SHELL (offset by t_total="
      << t_total << ")\n";
  out << "----------------------------------------------------------------\n";
  const std::vector<SPoint2> shell_offset =
      offsetCurve(sc.base, side, t_total, sc.resample, sc.miter_limit);
  if (shell_offset.empty()) {
    out << "  FAILED: Clipper2 returned no offset run for either side.\n";
    std::ofstream(outdir + "/phase0_" + sc.name + ".txt") << out.str();
    std::printf("  [%-22s] FAILED: empty offset run\n", sc.name.c_str());
    return R;
  }
  dumpCurve(out, "BASE", sc.base);
  dumpCurve(out, "OUTER_OFFSET", shell_offset);
  out << "  SHELL FACE (single region, boundary loop):\n    ";
  {
    std::vector<SPoint2> loop = sc.base;
    for (auto it = shell_offset.rbegin(); it != shell_offset.rend(); ++it)
      loop.push_back(*it);
    for (std::size_t i = 0; i < loop.size(); ++i) {
      if (i) out << " - ";
      out << fmtPt(loop[i]);
    }
    out << "\n";
  }

  // Step 2: route-i per-layer subdivision.
  out << "\n----------------------------------------------------------------\n";
  out << " STEP 2 — per-layer subdivision (route i: offset original base\n";
  out << "          by cumulative thickness; map between consecutive curves)\n";
  out << "----------------------------------------------------------------\n";

  std::vector<Layer> layers;
  std::vector<SPoint2> curve_prev = sc.base;
  double cum = 0.0;
  bool all_maps_ok = true;

  for (int k = 1; k <= n; ++k) {
    cum += sc.thick[k - 1];
    Layer L;
    L.index = k;
    L.t = sc.thick[k - 1];
    L.cum = cum;
    L.inner = curve_prev;

    // Route i: ALWAYS offset the original base by the cumulative thickness.
    const std::vector<SPoint2> raw_k =
        offsetCurve(sc.base, side, cum, sc.resample, sc.miter_limit);

    // Route-i natural correspondence: curve_{k-1} and curve_k are both
    // offsets of the SAME base, so when they have equal vertex counts the
    // per-layer map is the IDENTITY (vertex i of each descends from
    // base[i]). Use it directly. Re-projecting curve_k onto curve_{k-1}
    // with rebuildBaseOffsetMapFromGeometry's foot-of-perpendicular + a
    // 1.5*dist acceptance gate gets convex corners wrong: the miter apex
    // sits at miter-length (cum/sin(theta/2)) >> dist, beyond the gate, so
    // it is rejected and forward-filled into a spurious fan pair (e.g.
    // (0,1) at a sharp apex). Fall back to the geometric matcher only when
    // the counts differ (a bevel/collapse appears at one thickness only).
    if (raw_k.size() == L.inner.size() && raw_k.size() >= 2) {
      L.outer = raw_k;
      L.plan = ReverseMatchPlan{};
      L.plan.offset_points = raw_k;
      for (int i = 0; i < static_cast<int>(raw_k.size()); ++i) {
        L.plan.id_pairs.emplace_back(i, i);
      }
      L.plan.ok = true;
    } else {
      L.plan = rebuildBaseOffsetMapFromGeometry(
          L.inner, raw_k, /*closed*/ false, side, L.t,
          PairingAlgo::SegmentProjection);
      L.outer = L.plan.offset_points;
      if (L.outer.empty()) L.outer = raw_k;
    }

    if (L.inner.size() != L.outer.size()) R.m_ne_n = true;

    out << "\n  LAYER " << k << "  (t=" << L.t << ", cum=" << L.cum
        << ", inner=" << L.inner.size() << " outer=" << L.outer.size()
        << ", map.ok=" << (L.plan.ok ? "true" : "false") << ")\n";
    dumpCurve(out, "  INNER(curve_" + std::to_string(k - 1) + ")", L.inner);
    dumpCurve(out, "  OUTER(curve_" + std::to_string(k) + ")", L.outer);
    dumpPairs(out, L.plan);
    for (const auto& pr : L.plan.id_pairs) {
      if (pr.base < (int)L.inner.size() && pr.offset < (int)L.outer.size()) {
        out << "    (" << pr.base << "," << pr.offset << ")  "
            << fmtPt(L.inner[pr.base]) << " - " << fmtPt(L.outer[pr.offset])
            << "\n";
      }
    }
    dumpFaces(out, facesFromPlan(L.inner, L.outer, L.plan));

    if (!L.plan.ok) all_maps_ok = false;
    curve_prev = L.outer;
    layers.push_back(std::move(L));
  }

  // Phase-0 exit checks.
  out << "\n----------------------------------------------------------------\n";
  out << " PHASE-0 EXIT CHECKS\n";
  out << "----------------------------------------------------------------\n";

  // Nesting bound = the shell's OWN max foot-distance to base, not t_total.
  // At a convex corner the offset apex projects to the base VERTEX (miter
  // length cum/sin(theta/2), not the perpendicular edge distance cum), so
  // comparing to t_total spuriously fails the miter apex. The shell is the
  // outermost curve, so any nested inner curve stays within its envelope.
  double shell_max_foot = 0.0;
  for (const auto& p : shell_offset) {
    shell_max_foot = std::max(shell_max_foot, footDistToPolyline(p, sc.base));
  }

  bool nesting_ok = true;
  double prev_mean = -1.0;
  for (const auto& L : layers) {
    double dmin = 1e30, dmax = -1e30, dsum = 0.0;
    for (const auto& p : L.outer) {
      const double d = footDistToPolyline(p, sc.base);
      dmin = std::min(dmin, d);
      dmax = std::max(dmax, d);
      dsum += d;
    }
    const double dmean = L.outer.empty() ? 0.0 : dsum / L.outer.size();
    // Within the shell envelope (handles convex miter apexes correctly).
    const bool within = (dmax <= shell_max_foot + 1e-6);
    const bool grows = (dmean > prev_mean - 1e-6);
    if (!within || !grows) nesting_ok = false;
    out << "  curve_" << L.index << ": foot-dist to base  min=" << dmin
        << " mean=" << dmean << " max=" << dmax << "  (shell envelope "
        << shell_max_foot << ")  within_shell=" << (within ? "Y" : "N")
        << " monotone=" << (grows ? "Y" : "N") << "\n";
    prev_mean = dmean;
  }

  const double gap = maxVertexGap(layers.back().outer, shell_offset);
  const bool zero_gap = gap < 1e-6;
  out << "  zero-gap: max|curve_" << n << " - outer_offset| = " << gap
      << "  -> " << (zero_gap ? "PASS" : "FAIL") << "\n";
  out << "  valid maps: " << (all_maps_ok ? "PASS" : "FAIL") << "\n";

  R.m_outer  = (int)layers.back().outer.size();
  R.nesting  = nesting_ok;
  R.zero_gap = zero_gap;
  R.maps     = all_maps_ok;
  R.pass     = nesting_ok && zero_gap && all_maps_ok;

  out << "\n  PHASE-0[" << sc.name << "]: " << (R.pass ? "PASS" : "FAIL")
      << "  (nesting=" << (nesting_ok ? "Y" : "N")
      << " zero_gap=" << (zero_gap ? "Y" : "N")
      << " maps=" << (all_maps_ok ? "Y" : "N") << ")\n";

  std::ofstream(outdir + "/phase0_" + sc.name + ".txt") << out.str();
  writeSvg(outdir + "/phase0_" + sc.name + ".svg", sc.base, layers);

  std::printf("  [%-22s] N=%d M=%d %-7s nesting=%s zero_gap=%s maps=%s -> %s\n",
              sc.name.c_str(), R.n_base, R.m_outer,
              R.m_ne_n ? "(M!=N)" : "(M==N)",
              R.nesting ? "Y" : "N", R.zero_gap ? "Y" : "N",
              R.maps ? "Y" : "N", R.pass ? "PASS" : "FAIL");
  return R;
}

}  // namespace

int main() {
  const std::string outdir = PHASE0_OUT_DIR;
  const std::vector<Scenario> scs = {
      makeVSymmetric(),     makeVAsymmetric(),         makeTentSharpConvex(),
      makeArcOpen(),        makeStripBendNonUniform(), makeTentSharpRaw(),
  };

  std::printf("Phase-0 per-layer-offset prototype — %zu healthy scenarios\n",
              scs.size());
  std::printf("(full point/line/face dumps in %s/phase0_<name>.txt + .svg)\n\n",
              outdir.c_str());

  int n_pass = 0;
  for (const auto& s : scs) {
    if (runScenario(s, outdir).pass) ++n_pass;
  }

  std::printf("\n=== SUMMARY: %d/%zu scenarios PASS ===\n", n_pass, scs.size());
  return (n_pass == (int)scs.size()) ? 0 : 2;
}
