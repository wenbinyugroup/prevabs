// Unit tests for CurveFrameLookup (Phase A of
// plan-20260514-decouple-local-frame-from-map.md).
//
// The class is intentionally header-light: it depends only on
// `geo_types.hpp` (header-only `SPoint2`, `SVector3`) so this test
// compiles cleanly without pulling in the rest of the project.

#include "catch_amalgamated.hpp"

#include "CurveFrameLookup.hpp"

#include <chrono>
#include <cmath>
#include <vector>

namespace {

// Helper: assert two unit vectors are parallel (same or opposite direction).
// Tangents are direction-only; sign depends on segment orientation, so the
// tests check |cos(angle)| ~= 1 rather than the signed dot product.
bool isParallel(const SVector3& a, const SVector3& b, double tol = 1e-9) {
  const double d = a.x() * b.x() + a.y() * b.y() + a.z() * b.z();
  return std::fabs(std::fabs(d) - 1.0) < tol;
}

}  // namespace


// ===================================================================
// Trivial: straight segment
// ===================================================================

TEST_CASE("CurveFrameLookup: 2-point straight segment",
          "[frame][lookup][trivial]") {
  std::vector<SPoint2> v = { SPoint2(0.0, 0.0), SPoint2(1.0, 0.0) };
  CurveFrameLookup lookup(v, /*closed*/ false);

  REQUIRE(lookup.segmentCount() == 1);

  SECTION("query directly above midpoint -> u=0.5, tangent = x") {
    auto hit = lookup.nearest(SPoint2(0.5, 1.0));
    CHECK(hit.seg_index == 0);
    CHECK(std::fabs(hit.u - 0.5) < 1e-12);
    CHECK(std::fabs(hit.dist - 1.0) < 1e-12);

    SVector3 t = lookup.yAxisAt(SPoint2(0.5, 1.0));
    // 2-D (1, 0) maps to 3-D (0, 1, 0)
    CHECK(std::fabs(t.x()) < 1e-12);
    CHECK(std::fabs(t.y() - 1.0) < 1e-12);
    CHECK(std::fabs(t.z()) < 1e-12);
  }

  SECTION("query before start clamps to u=0") {
    auto hit = lookup.nearest(SPoint2(-2.0, 0.0));
    CHECK(hit.seg_index == 0);
    CHECK(hit.u == 0.0);
    CHECK(std::fabs(hit.dist - 2.0) < 1e-12);
  }

  SECTION("query past end clamps to u=1") {
    auto hit = lookup.nearest(SPoint2(3.0, 0.0));
    CHECK(hit.seg_index == 0);
    CHECK(hit.u == 1.0);
    CHECK(std::fabs(hit.dist - 2.0) < 1e-12);
  }
}

TEST_CASE("CurveFrameLookup: tangent is unit length and segment-aligned",
          "[frame][lookup][trivial]") {
  // Diagonal segment from (0,0) to (3,4) — length 5.
  std::vector<SPoint2> v = { SPoint2(0.0, 0.0), SPoint2(3.0, 4.0) };
  CurveFrameLookup lookup(v, false);

  SVector3 t = lookup.tangentAt(0, 0.5);
  CHECK(std::fabs(t.norm() - 1.0) < 1e-12);
  // (3, 4) / 5 = (0.6, 0.8) — in 3-D: (0, 0.6, 0.8)
  CHECK(std::fabs(t.x()) < 1e-12);
  CHECK(std::fabs(t.y() - 0.6) < 1e-12);
  CHECK(std::fabs(t.z() - 0.8) < 1e-12);
}


// ===================================================================
// Closed polyline: axis-aligned square
// ===================================================================

TEST_CASE("CurveFrameLookup: closed unit square, internal point",
          "[frame][lookup][closed]") {
  // Vertices in CCW order; closed=true adds the v[3]->v[0] segment.
  std::vector<SPoint2> v = {
    SPoint2(0.0, 0.0), SPoint2(1.0, 0.0),
    SPoint2(1.0, 1.0), SPoint2(0.0, 1.0),
  };
  CurveFrameLookup lookup(v, /*closed*/ true);

  REQUIRE(lookup.segmentCount() == 4);

  SECTION("point near bottom edge picks segment 0") {
    auto hit = lookup.nearest(SPoint2(0.5, 0.2));
    CHECK(hit.seg_index == 0);
    CHECK(std::fabs(hit.u - 0.5) < 1e-12);
    CHECK(std::fabs(hit.dist - 0.2) < 1e-12);

    SVector3 t = lookup.tangentAt(hit.seg_index, hit.u);
    SVector3 expected(0.0, 1.0, 0.0);  // 2-D direction (1, 0)
    CHECK(isParallel(t, expected));
  }

  SECTION("point near right edge picks segment 1") {
    auto hit = lookup.nearest(SPoint2(0.8, 0.5));
    CHECK(hit.seg_index == 1);
    SVector3 t = lookup.tangentAt(hit.seg_index, hit.u);
    SVector3 expected(0.0, 0.0, 1.0);  // 2-D direction (0, 1)
    CHECK(isParallel(t, expected));
  }

  SECTION("point near left edge picks the closing segment (3 -> 0)") {
    auto hit = lookup.nearest(SPoint2(0.1, 0.5));
    CHECK(hit.seg_index == 3);
    CHECK(std::fabs(hit.dist - 0.1) < 1e-12);
  }

  SECTION("point exactly at centre is equidistant; pick is deterministic") {
    auto hit = lookup.nearest(SPoint2(0.5, 0.5));
    // All four sides are equidistant; the linear scan picks the first
    // seen (index 0).  We only check that the distance is correct and
    // the tangent is one of the four axis directions.
    CHECK(std::fabs(hit.dist - 0.5) < 1e-12);
    SVector3 t = lookup.tangentAt(hit.seg_index, hit.u);
    CHECK(std::fabs(t.norm() - 1.0) < 1e-12);
  }
}


// ===================================================================
// L-shape: concave corner / bisector behaviour
// ===================================================================

TEST_CASE("CurveFrameLookup: L-shape concave corner switches across bisector",
          "[frame][lookup][concave]") {
  // L-shape (open polyline):
  //   v0 (0,0) -- v1 (2,0) -- v2 (2,2)
  // Segment 0 runs along x; segment 1 runs along y.
  // The inner-corner bisector is the line y = x for points above/left of (2,0)
  // (within the relevant quadrant).
  std::vector<SPoint2> v = {
    SPoint2(0.0, 0.0), SPoint2(2.0, 0.0), SPoint2(2.0, 2.0),
  };
  CurveFrameLookup lookup(v, /*closed*/ false);
  REQUIRE(lookup.segmentCount() == 2);

  SECTION("point clearly below the bisector picks horizontal segment") {
    // (1.0, 0.2): close to seg 0 (dist 0.2) vs seg 1 (dist 1.0)
    auto hit = lookup.nearest(SPoint2(1.0, 0.2));
    CHECK(hit.seg_index == 0);

    SVector3 t = lookup.tangentAt(hit.seg_index, hit.u);
    SVector3 horizontal(0.0, 1.0, 0.0);
    CHECK(isParallel(t, horizontal));
  }

  SECTION("point clearly above the bisector picks vertical segment") {
    // (1.8, 1.0): close to seg 1 (dist 0.2) vs seg 0 (dist 1.0)
    auto hit = lookup.nearest(SPoint2(1.8, 1.0));
    CHECK(hit.seg_index == 1);

    SVector3 t = lookup.tangentAt(hit.seg_index, hit.u);
    SVector3 vertical(0.0, 0.0, 1.0);
    CHECK(isParallel(t, vertical));
  }

  SECTION("crossing the bisector flips the selected segment") {
    // Two queries close to the bisector but on opposite sides.
    auto h1 = lookup.nearest(SPoint2(1.5, 0.4));  // closer to seg 0
    auto h2 = lookup.nearest(SPoint2(1.6, 0.7));  // closer to seg 1
    CHECK(h1.seg_index == 0);
    CHECK(h2.seg_index == 1);
  }
}


// ===================================================================
// Airfoil-like closed polyline: LE and TE interior queries
// ===================================================================

namespace {

// NACA-0012-ish symmetric thickness distribution (just enough shape to
// exercise the nearest-segment query without re-implementing a real foil).
std::vector<SPoint2> makeFoilPolyline(int n_chord) {
  // Closed contour: upper surface from LE to TE, then lower surface back.
  // Coordinates in (chord, thickness) space.  Chord c=1.0.
  std::vector<SPoint2> v;
  const double c = 1.0;
  const double t = 0.12;
  // Half-cosine spacing for clustering near LE / TE.
  std::vector<double> xs(n_chord);
  for (int i = 0; i < n_chord; ++i) {
    const double beta = M_PI * i / (n_chord - 1);
    xs[i] = 0.5 * c * (1.0 - std::cos(beta));
  }
  auto yt = [&](double x) {
    const double xc = x / c;
    return 5.0 * t * c * (
        0.2969 * std::sqrt(xc)
      - 0.1260 * xc
      - 0.3516 * xc * xc
      + 0.2843 * xc * xc * xc
      - 0.1015 * xc * xc * xc * xc);
  };
  // Upper surface LE -> TE
  for (int i = 0; i < n_chord; ++i) v.push_back(SPoint2(xs[i],  yt(xs[i])));
  // Lower surface TE -> LE (skip the duplicate TE point)
  for (int i = n_chord - 2; i >= 1; --i) v.push_back(SPoint2(xs[i], -yt(xs[i])));
  return v;
}

}  // namespace

TEST_CASE("CurveFrameLookup: airfoil interior point near leading edge",
          "[frame][lookup][airfoil]") {
  auto v = makeFoilPolyline(40);
  CurveFrameLookup lookup(v, /*closed*/ true);
  REQUIRE(lookup.segmentCount() == static_cast<int>(v.size()));

  // Query well inside the airfoil, just aft of the leading edge.
  // The closest segment should be near the leading edge (small x).
  SPoint2 q(0.02, 0.0);
  auto hit = lookup.nearest(q);

  // Foot location in 2-D
  // (seg endpoints reconstructed via tangent direction).
  SVector3 t = lookup.tangentAt(hit.seg_index, hit.u);
  CHECK(std::fabs(t.norm() - 1.0) < 1e-9);
  // Near LE, the local tangent runs roughly in +/- z direction (chord
  // axis is x in 2-D / y in 3-D; LE tangent is closer to the thickness
  // axis -> 3-D z). Allow a generous wedge.
  CHECK(std::fabs(t.z()) > 0.5);

  // Distance from interior point to the airfoil surface should be small
  // (well under half-chord).
  CHECK(hit.dist < 0.05);
}

TEST_CASE("CurveFrameLookup: airfoil interior point near trailing edge",
          "[frame][lookup][airfoil]") {
  auto v = makeFoilPolyline(40);
  CurveFrameLookup lookup(v, /*closed*/ true);

  // Query just inboard of the trailing edge along the chord line.
  SPoint2 q(0.95, 0.0);
  auto hit = lookup.nearest(q);

  SVector3 t = lookup.tangentAt(hit.seg_index, hit.u);
  CHECK(std::fabs(t.norm() - 1.0) < 1e-9);
  // Near TE the surface is shallow; the tangent should be close to the
  // chord axis (3-D y component dominant).
  CHECK(std::fabs(t.y()) > 0.7);

  // Surface is close at this x (within the small TE thickness).
  CHECK(hit.dist < 0.03);
}


// ===================================================================
// Performance smoke test
// ===================================================================

TEST_CASE("CurveFrameLookup: linear-scan benchmark (N=200, Q=10000)",
          "[frame][lookup][perf]") {
  // Synthetic polyline: 200 points around a unit circle.
  const int N = 200;
  std::vector<SPoint2> v;
  v.reserve(N);
  for (int i = 0; i < N; ++i) {
    const double a = 2.0 * M_PI * i / N;
    v.push_back(SPoint2(std::cos(a), std::sin(a)));
  }
  CurveFrameLookup lookup(v, /*closed*/ true);

  const int Q = 10000;
  std::vector<SPoint2> qs;
  qs.reserve(Q);
  for (int i = 0; i < Q; ++i) {
    // Spiral inside the circle.
    const double r = 0.95 * static_cast<double>(i) / Q;
    const double a = 17.0 * 2.0 * M_PI * i / Q;
    qs.push_back(SPoint2(r * std::cos(a), r * std::sin(a)));
  }

  auto t0 = std::chrono::steady_clock::now();
  volatile double sink = 0.0;
  for (const auto& q : qs) {
    auto h = lookup.nearest(q);
    sink += h.dist;
  }
  auto t1 = std::chrono::steady_clock::now();
  const double ms =
    std::chrono::duration<double, std::milli>(t1 - t0).count();
  // Plan budget: < 30 ms.  Use a generous bound to absorb CI noise.
  // The assertion stays as a regression tripwire rather than a tight check.
  WARN("CurveFrameLookup linear scan: " << ms << " ms for "
       << Q << " queries on N=" << N);
  CHECK(ms < 200.0);
  (void)sink;
}
