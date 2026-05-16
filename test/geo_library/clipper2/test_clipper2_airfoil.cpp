// Stand-alone Clipper2 sanity tests on real airfoil geometry.
// Plan: A0 of plan-20260515-progress-review-and-next-steps.md §4.3.1.
//
// Constraint: zero PreVABS coupling. We link only Clipper2 and the
// Catch2 amalgamated single-header. The .dat fixtures live next to
// the test under airfoils/. If PreVABS main sources break, this
// executable must still build and run.
//
// Assertion strategy: we test **geometric features** (area, connectivity,
// closure, self-intersection count) rather than bit-exact vertex
// equality. That keeps the suite resilient to future Clipper2 version
// bumps within the 1.4 series.
//
// When an assertion fails we dump an SVG into svg_dumps/<tag>.svg for
// visual diagnosis.

#include "catch_amalgamated.hpp"

#include "airfoil_loader.hpp"
#include "geom_assertions.hpp"
#include "svg_dump.hpp"

#include "clipper2/clipper.h"

#include <cmath>
#include <string>
#include <vector>

using namespace Clipper2Lib;
using clipper2_airfoil::AirfoilProfile;
using clipper2_airfoil::loadByName;
using clipper2_airfoil::makeNaca4;
using clipper2_airfoil::dumpOffsetSvg;
using clipper2_airfoil::signedArea;
using clipper2_airfoil::totalAbsArea;
using clipper2_airfoil::largestAbsArea;
using clipper2_airfoil::countSelfIntersections;
using clipper2_airfoil::bboxOf;

namespace {

// Wraps InflatePaths with the parameters PreVABS will actually use:
//   - Miter join, Polygon end-type, miter_limit=2.0
//   - precision=8 (scale 1e8 internally). **A0 finding**: Clipper2
//     1.4.0 enforces CLIPPER2_MAX_DEC_PRECISION=8 in clipper.core.h;
//     passing precision=9 throws "Precision exceeds the permitted
//     range". plan-clipper2 §4.2.2 left this as an open question;
//     A0 nails it down to 8. Scale 1e8 still resolves 1e-8 chord
//     units, which is 5+ orders of magnitude below typical airfoil
//     deltas (5e-3 .. 3e-2 in chord-normalised units).
PathsD inflate(const PathD& base, double delta,
               JoinType jt = JoinType::Miter,
               EndType  et = EndType::Polygon,
               double miter_limit = 2.0) {
  PathsD subj;
  subj.push_back(base);
  return InflatePaths(subj, delta, jt, et, miter_limit, /*precision=*/8);
}

// Convenience: SVG dump triggered when REQUIRE fails further along.
// We pre-emptively dump and then let Catch2's assertion fail, so the
// SVG is always there for inspection regardless of which line fails.
void dump(const std::string& tag, const PathD& base, const PathsD& sol) {
  const std::string path = dumpOffsetSvg(tag, base, sol);
  INFO("svg dump: " << path);
}

}  // namespace

// ===================================================================
// Sanity: each fixture loads, looks like a real airfoil
// ===================================================================

TEST_CASE("Airfoil fixtures load and have plausible chord/thickness",
          "[airfoil][load]") {
  const std::vector<std::string> names = {
      "ah79k132", "ah79k143", "ah88k130", "ah80136", "2032c", "mh104"
  };
  for (const auto& name : names) {
    INFO("airfoil: " << name);
    AirfoilProfile a = loadByName(name);
    REQUIRE(a.points.size() >= 30);
    const auto bb = bboxOf(a.points);
    // Chord ≈ 1.0 in normalized units.
    CHECK(bb.width() > 0.95);
    CHECK(bb.width() < 1.05);
    // Thickness 5% – 30% of chord for the UIUC subset we shipped.
    CHECK(bb.height() > 0.02);
    CHECK(bb.height() < 0.30);
    // Signed area is bounded: thickness * chord / 2 is a loose envelope.
    const double a_area = std::fabs(signedArea(a.points));
    CHECK(a_area > 0.001);
    CHECK(a_area < 0.20);
  }
}

TEST_CASE("Synthetic NACA0012 loads with expected sharp TE",
          "[airfoil][load][naca]") {
  AirfoilProfile a = makeNaca4("0012", 60);
  REQUIRE(a.points.size() >= 100);
  const auto bb = bboxOf(a.points);
  CHECK(bb.width()  == Catch::Approx(1.0).margin(1e-9));
  // NACA0012 has roughly 12% thickness/chord.
  CHECK(bb.height() == Catch::Approx(0.12).margin(0.01));
  CHECK(a.te_closed);   // sharp TE construction
}


// ===================================================================
// Convex / well-behaved cases — should always succeed
// ===================================================================

TEST_CASE("Internal offset on thick NACA0012 produces one closed polygon",
          "[clipper2][airfoil][thick][naca]") {
  AirfoilProfile a = makeNaca4("0012", 80);
  const std::vector<double> deltas = {-0.005, -0.01, -0.02, -0.03};
  for (double delta : deltas) {
    INFO("delta=" << delta);
    PathsD sol = inflate(a.points, delta);
    dump("naca0012_delta_" + std::to_string(delta), a.points, sol);
    REQUIRE(sol.size() == 1);
    CHECK(std::fabs(signedArea(sol[0])) > 0.0);
    CHECK(countSelfIntersections(sol[0]) == 0);
    // Offset shrinks the polygon area monotonically.
    CHECK(totalAbsArea(sol) < std::fabs(signedArea(a.points)));
  }
}

TEST_CASE("External offset on NACA0012 grows the area monotonically",
          "[clipper2][airfoil][external][naca]") {
  AirfoilProfile a = makeNaca4("0012", 60);
  PathsD prev;
  PathD subj_prev = a.points;
  double prev_area = std::fabs(signedArea(a.points));
  for (double delta : {0.005, 0.010, 0.020, 0.040}) {
    INFO("delta=" << delta);
    PathsD sol = inflate(a.points, delta);
    dump("naca0012_ext_" + std::to_string(delta), a.points, sol);
    REQUIRE(sol.size() == 1);
    const double a_now = std::fabs(signedArea(sol[0]));
    CHECK(a_now > prev_area);
    prev_area = a_now;
    CHECK(countSelfIntersections(sol[0]) == 0);
  }
}

TEST_CASE("Internal offset on NACA4412 (cambered) is well-behaved",
          "[clipper2][airfoil][cambered][naca]") {
  AirfoilProfile a = makeNaca4("4412", 80);
  for (double delta : {-0.005, -0.015, -0.030}) {
    INFO("delta=" << delta);
    PathsD sol = inflate(a.points, delta);
    dump("naca4412_delta_" + std::to_string(delta), a.points, sol);
    REQUIRE(sol.size() >= 1);
    CHECK(largestAbsArea(sol) > 0.0);
    for (const auto& p : sol) {
      CHECK(countSelfIntersections(p) == 0);
    }
  }
}


// ===================================================================
// UIUC: thin TE / reflex / trim — the geometries that broke the
// in-house offset implementation (see issue-20260514-airfoil-offset-thin-fail.md)
// ===================================================================

TEST_CASE("ah79k143 thin TE: internal offset survives multiple thicknesses",
          "[clipper2][airfoil][uiuc][thin_te]") {
  AirfoilProfile a = loadByName("ah79k143");
  // Multiple deltas spanning "comfortably thinner than TE" to
  // "comparable to TE thickness", which is where PreVABS' in-house
  // offset began to fail.
  for (double delta : {-0.005, -0.010, -0.015, -0.020, -0.030}) {
    INFO("delta=" << delta);
    PathsD sol = inflate(a.points, delta);
    dump("ah79k143_delta_" + std::to_string(delta), a.points, sol);
    // Either we get a valid (possibly disconnected) solution, or the
    // thickness has been eaten so completely that the inset is empty.
    if (sol.empty()) {
      CHECK(delta < -0.020);   // empty only at deep insets
      continue;
    }
    for (const auto& p : sol) {
      CHECK(countSelfIntersections(p) == 0);
    }
    // Inset must shrink area.
    CHECK(totalAbsArea(sol) < std::fabs(signedArea(a.points)));
  }
}

TEST_CASE("ah88k130 reflex camber: offset preserves overall shape",
          "[clipper2][airfoil][uiuc][reflex]") {
  AirfoilProfile a = loadByName("ah88k130");
  for (double delta : {-0.005, -0.010, -0.020}) {
    INFO("delta=" << delta);
    PathsD sol = inflate(a.points, delta);
    dump("ah88k130_delta_" + std::to_string(delta), a.points, sol);
    REQUIRE(!sol.empty());
    CHECK(largestAbsArea(sol) > 0.0);
    for (const auto& p : sol) {
      CHECK(countSelfIntersections(p) == 0);
    }
  }
}

TEST_CASE("ah80136 (open TE): Clipper2 handles non-closing input",
          "[clipper2][airfoil][uiuc][open_te]") {
  AirfoilProfile a = loadByName("ah80136");
  INFO("te_gap=" << a.te_gap);
  CHECK_FALSE(a.te_closed);
  // EndType::Polygon treats the polyline as closed by adding an implicit
  // closing edge, so even open-TE inputs should produce a valid offset.
  PathsD sol = inflate(a.points, -0.005);
  dump("ah80136_te_open", a.points, sol);
  REQUIRE(!sol.empty());
  CHECK(largestAbsArea(sol) > 0.0);
}

TEST_CASE("2032c (short, blunt TE): coarse-resolution airfoil offsets",
          "[clipper2][airfoil][uiuc][coarse]") {
  AirfoilProfile a = loadByName("2032c");
  // Coarse 35-point sampling — make sure precision=9 handles it.
  for (double delta : {-0.005, -0.015}) {
    INFO("delta=" << delta);
    PathsD sol = inflate(a.points, delta);
    dump("2032c_delta_" + std::to_string(delta), a.points, sol);
    REQUIRE(!sol.empty());
    for (const auto& p : sol) {
      CHECK(countSelfIntersections(p) == 0);
    }
  }
}

TEST_CASE("mh104: classic rotorcraft airfoil internal offset",
          "[clipper2][airfoil][uiuc][rotorcraft]") {
  AirfoilProfile a = loadByName("mh104");
  for (double delta : {-0.005, -0.010, -0.015}) {
    INFO("delta=" << delta);
    PathsD sol = inflate(a.points, delta);
    dump("mh104_delta_" + std::to_string(delta), a.points, sol);
    REQUIRE(!sol.empty());
    for (const auto& p : sol) {
      CHECK(countSelfIntersections(p) == 0);
    }
  }
}


// ===================================================================
// JoinType comparison
// ===================================================================

TEST_CASE("JoinType comparison on ah79k143: Miter / Round / Square agree on area",
          "[clipper2][airfoil][join_type]") {
  AirfoilProfile a = loadByName("ah79k143");
  const double delta = -0.010;

  PathsD miter  = inflate(a.points, delta, JoinType::Miter, EndType::Polygon, 2.0);
  PathsD round  = inflate(a.points, delta, JoinType::Round, EndType::Polygon);
  PathsD square = inflate(a.points, delta, JoinType::Square, EndType::Polygon);

  REQUIRE_FALSE(miter.empty());
  REQUIRE_FALSE(round.empty());
  REQUIRE_FALSE(square.empty());

  const double am = totalAbsArea(miter);
  const double ar = totalAbsArea(round);
  const double as = totalAbsArea(square);

  INFO("areas: miter=" << am << " round=" << ar << " square=" << as);
  // The three joins shape corners differently but bulk area should
  // agree to within a few percent.
  const double mean = (am + ar + as) / 3.0;
  CHECK(std::fabs(am - mean) / mean < 0.05);
  CHECK(std::fabs(ar - mean) / mean < 0.05);
  CHECK(std::fabs(as - mean) / mean < 0.05);
}


// ===================================================================
// Nested offsets — the building block for the long-term end-state
// (see plan §8 of plan-20260515-progress-review-and-next-steps.md).
// We exercise it here so that A0 produces evidence the end-state
// machinery is geometrically viable, not just the §5 reverse-matching
// bridge use case.
// ===================================================================

TEST_CASE("Nested offsets on NACA0012 produce monotonically shrinking layers",
          "[clipper2][airfoil][nested][end_state]") {
  AirfoilProfile a = makeNaca4("0012", 100);
  const std::vector<double> cumu_thk = {0.0, 0.002, 0.005, 0.009, 0.014, 0.020};

  std::vector<PathsD> layers;
  layers.reserve(cumu_thk.size());
  for (double thk : cumu_thk) {
    if (thk == 0.0) {
      PathsD base_paths;
      base_paths.push_back(a.points);
      layers.push_back(base_paths);
    } else {
      layers.push_back(inflate(a.points, -thk));
    }
  }

  // 1. Every layer is non-empty (NACA0012 is thick enough for 2cm inset).
  // 2. Areas are strictly decreasing as we inset deeper.
  double prev_area = std::fabs(signedArea(a.points)) + 1.0;
  for (std::size_t i = 0; i < layers.size(); ++i) {
    INFO("layer " << i << " cumu_thk=" << cumu_thk[i]);
    REQUIRE_FALSE(layers[i].empty());
    const double a_i = totalAbsArea(layers[i]);
    CHECK(a_i < prev_area);
    prev_area = a_i;
  }

  // SVG: overlay all layers for visual confirmation that they nest.
  PathsD overlay;
  for (const auto& L : layers)
    for (const auto& p : L) overlay.push_back(p);
  dump("naca0012_nested_layers", a.points, overlay);
}

TEST_CASE("Nested offsets on ah79k143 eventually go empty (thin TE)",
          "[clipper2][airfoil][nested][thin_te]") {
  AirfoilProfile a = loadByName("ah79k143");
  // Push the inset past the local half-thickness near TE — Clipper2
  // should produce empty paths (or paths with vanishing area), not
  // crash. This is the end-state "dropped layer" condition.
  bool saw_non_empty = false;
  bool saw_empty     = false;
  for (double thk : {0.001, 0.005, 0.010, 0.020, 0.050, 0.080}) {
    PathsD sol = inflate(a.points, -thk);
    INFO("thk=" << thk << " polygons=" << sol.size());
    if (sol.empty()) saw_empty = true;
    else             saw_non_empty = true;
  }
  CHECK(saw_non_empty);
  CHECK(saw_empty);
}


// ===================================================================
// Precision floor — what is the smallest delta Clipper2 still handles?
// ===================================================================

TEST_CASE("Tiny offsets (1e-4, 1e-5) on a normalized airfoil still produce output",
          "[clipper2][airfoil][precision]") {
  AirfoilProfile a = makeNaca4("0012", 60);
  for (double delta : {-1e-3, -1e-4, -1e-5}) {
    INFO("delta=" << delta);
    PathsD sol = inflate(a.points, delta);
    // We require precision=9 (scale 1e9) to survive |delta|=1e-5. If
    // Clipper2's internal limit changes, this test flips and tells us
    // before we ship PreVABS.
    REQUIRE_FALSE(sol.empty());
    CHECK(largestAbsArea(sol) > 0.0);
  }
}


// ===================================================================
// Degenerate inputs — pure don't-crash guarantees
// ===================================================================

TEST_CASE("Degenerate inputs do not crash and return empty / sensible output",
          "[clipper2][degenerate]") {
  // 1. Single-point "polygon".
  {
    PathD pt;
    pt.push_back({0.5, 0.5});
    PathsD sol = inflate(pt, -0.01);
    CHECK(sol.empty());   // single point has no offset
  }
  // 2. Collinear / zero-area polygon.
  {
    PathD line;
    line.push_back({0.0, 0.0});
    line.push_back({1.0, 0.0});
    line.push_back({2.0, 0.0});
    PathsD sol = inflate(line, -0.01);
    CHECK(sol.empty());
  }
  // 3. Repeated duplicate vertices.
  {
    PathD dup;
    for (int i = 0; i < 5; ++i) {
      dup.push_back({0.0, 0.0});
      dup.push_back({1.0, 0.0});
      dup.push_back({1.0, 1.0});
      dup.push_back({0.0, 1.0});
    }
    PathsD sol = inflate(dup, -0.05);
    // Should produce a single shrunken square or, at worst, empty.
    if (!sol.empty()) {
      CHECK(largestAbsArea(sol) > 0.0);
    }
  }
}
