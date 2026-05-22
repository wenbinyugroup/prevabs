// Unit tests for offsetWithClipper2 (Stage B of plan-20260514-
// clipper2-offset-backend.md).
//
// Scope: the wrapper's added value over raw Clipper2 InflatePaths.
// Specifically:
//   - PreVABS-side I/O contract (SPoint2 in yz, side ∈ {-1,+1},
//     base_is_closed semantics)
//   - source attribution (`OffsetVertexSource`): in-segment vs the
//     -1 "join-inserted" case
//   - defensive trailing-duplicate handling for closed inputs
//   - disconnected / empty edge cases
//
// Raw Clipper2 geometry correctness on real airfoils is covered by
// the standalone A0 suite at test/geo_library/clipper2/
// (test_clipper2_airfoil).

#include "catch_amalgamated.hpp"

#include "offset_clipper2.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

using prevabs::geo::OffsetPolygon;
using prevabs::geo::OffsetVertexSource;
using prevabs::geo::offsetWithClipper2;
using prevabs::geo::ReverseMatchPlan;
using prevabs::geo::planReverseMatch;
using prevabs::geo::planReverseMatchByNearest;
using prevabs::geo::rebuildBaseOffsetMapFromGeometry;

namespace {

double polygonAbsArea(const std::vector<SPoint2>& p) {
  if (p.size() < 3) return 0.0;
  double a = 0.0;
  const std::size_t n = p.size();
  for (std::size_t i = 0; i < n; ++i) {
    const auto& p0 = p[i];
    const auto& p1 = p[(i + 1) % n];
    a += p0.x() * p1.y() - p1.x() * p0.y();
  }
  return std::fabs(0.5 * a);
}

int countSourcesUnattributed(const OffsetPolygon& op) {
  int c = 0;
  for (const auto& s : op.sources) {
    if (s.base_seg < 0) ++c;
  }
  return c;
}

int countSourcesAttributed(const OffsetPolygon& op) {
  return static_cast<int>(op.sources.size()) - countSourcesUnattributed(op);
}

}  // namespace


// ===================================================================
// Closed convex polygon — basic source attribution
// ===================================================================

TEST_CASE("offsetWithClipper2: closed CCW unit square, internal offset",
          "[offset_clipper2][closed][convex]") {
  // 4-vertex CCW square. side = -1 → inward, dist = 0.1 → 0.8×0.8 inset.
  const std::vector<SPoint2> base = {
      SPoint2(0.0, 0.0),
      SPoint2(1.0, 0.0),
      SPoint2(1.0, 1.0),
      SPoint2(0.0, 1.0),
  };

  auto out = offsetWithClipper2(base, /*closed*/ true, /*side*/ -1, 0.1);
  REQUIRE(out.size() == 1);
  const auto& op = out[0];

  CHECK(op.is_closed);
  CHECK(op.points.size() == op.sources.size());

  // The 0.8×0.8 inset has area 0.64.
  CHECK(polygonAbsArea(op.points) == Catch::Approx(0.64).margin(1e-6));

  // For a 90° miter corner, miter distance from input corner =
  // dist * sqrt(2) ≈ 0.141. Under the 1.5*dist = 0.15 source-radius
  // gate every output vertex should still get a base_seg.
  CHECK(countSourcesUnattributed(op) == 0);

  // base_seg should always fall in [0, 4). Strict per-segment coverage
  // is not asserted because corner output vertices sit equidistant
  // from two adjacent input segments; the linear-scan tie-breaker
  // picks the lower index, leaving one segment with no attributed
  // vertex. The wrapper's contract is "in range or -1", not "every
  // input segment is sourced".
  std::vector<int> seen(4, 0);
  for (const auto& s : op.sources) {
    REQUIRE(s.base_seg >= 0);
    REQUIRE(s.base_seg < 4);
    seen[s.base_seg] = 1;
  }
  CHECK(std::count(seen.begin(), seen.end(), 1) >= 2);
}

TEST_CASE("offsetWithClipper2: closed CCW unit square, external offset",
          "[offset_clipper2][closed][convex]") {
  const std::vector<SPoint2> base = {
      SPoint2(0.0, 0.0), SPoint2(1.0, 0.0),
      SPoint2(1.0, 1.0), SPoint2(0.0, 1.0),
  };
  auto out = offsetWithClipper2(base, true, /*side*/ +1, 0.1);
  REQUIRE(out.size() == 1);
  // 1.2×1.2 with corner roundings; area between 1.44 and 1.45.
  const double a = polygonAbsArea(out[0].points);
  CHECK(a > 1.43);
  CHECK(a < 1.46);
}


// ===================================================================
// Sharp acute corner — at least one source = -1 (miter join vertex)
// ===================================================================

TEST_CASE("offsetWithClipper2: rhombus apex external offset produces "
          "source = -1 at the miter join vertex",
          "[offset_clipper2][closed][miter][source_neg1]") {
  // Rhombus chosen so the top/bottom corners have interior angle
  // ≈ 74°. External miter distance there = dist/sin(37°) ≈ 1.66*dist,
  // i.e. inside the (1.5*dist, miter_limit*dist=2*dist) sweet spot:
  // not clipped to a bevel by miter_limit=2, but beyond the 1.5*dist
  // source-attribution radius. So those two output vertices should
  // be flagged with base_seg = -1.
  const std::vector<SPoint2> base = {
      SPoint2( 0.00,  0.50),   // top apex, interior angle ≈ 74°
      SPoint2( 0.35,  0.00),   // right, interior angle ≈ 106°
      SPoint2( 0.00, -0.50),   // bottom apex, ≈ 74°
      SPoint2(-0.35,  0.00),   // left, ≈ 106°
  };
  auto out = offsetWithClipper2(base, true, /*side*/ +1, 0.01);
  REQUIRE(out.size() == 1);
  const auto& op = out[0];

  // Expect at least two unattributed vertices (top + bottom apex
  // miter points). Right/left side miters are inside 1.5*dist.
  CHECK(countSourcesUnattributed(op) >= 2);
  // ...but the majority of output vertices should still be attributed
  // (one per non-corner stretch).
  CHECK(countSourcesAttributed(op) >= 2);
}


// ===================================================================
// Open polyline — side semantics + EndType::Butt
// ===================================================================

// ===================================================================
// Open polyline — Phase 1: Butt-wrap single-side filtering
// ===================================================================
//
// After plan-20260518-open-polyline-butt-side-filter.md Phase 1, open
// inputs return a one-sided OPEN polyline (is_closed = false). The
// Butt wrap is post-filtered by `side` according to sign(t × d) of
// each wrap vertex against the closest base segment.
//
//   side = +1 → left  of base (t × d > 0)
//   side = -1 → right of base (t × d < 0)
//
// Each filtered run walks in base direction (P_0 → P_{N-1}), so
// base_seg in `sources` is non-decreasing.
// ===================================================================

namespace {

// Verify base_seg sequence is non-decreasing along the run.
bool baseSegNonDecreasing(const OffsetPolygon& op) {
  int last = -1;
  for (const auto& s : op.sources) {
    if (s.base_seg < 0) continue;
    if (s.base_seg < last) return false;
    last = s.base_seg;
  }
  return true;
}

// Verify every vertex lies on the requested side of the base
// (computed via the same t × d sign convention the filter uses).
bool allOnSide(const OffsetPolygon& op,
               const std::vector<SPoint2>& base,
               int side) {
  for (std::size_t k = 0; k < op.points.size(); ++k) {
    const int seg = op.sources[k].base_seg;
    if (seg < 0) continue;
    const double tx = base[seg + 1].x() - base[seg].x();
    const double ty = base[seg + 1].y() - base[seg].y();
    const double u  = op.sources[k].base_u;
    const double fx = base[seg].x() + u * tx;
    const double fy = base[seg].y() + u * ty;
    const double dx = op.points[k].x() - fx;
    const double dy = op.points[k].y() - fy;
    const double cross = tx * dy - ty * dx;
    if (side > 0 && cross <= 0.0) return false;
    if (side < 0 && cross >= 0.0) return false;
  }
  return true;
}

}  // namespace


// O1 — Straight horizontal open polyline, two segments. side = +1
// (left = +y direction). Output should be a flat polyline at y = +dist
// with at least 3 vertices (one per base vertex plus possibly Butt-cap
// extras at the ends).
TEST_CASE("offsetWithClipper2: open 2-segment straight base, side=+1",
          "[offset_clipper2][open][side]") {
  const std::vector<SPoint2> base = {
      SPoint2(0.0, 0.0), SPoint2(1.0, 0.0), SPoint2(2.0, 0.0),
  };
  const double dist = 0.05;

  auto out = offsetWithClipper2(base, /*closed*/ false, +1, dist);
  REQUIRE(out.size() == 1);
  const auto& op = out[0];

  CHECK_FALSE(op.is_closed);
  // Clipper2 collapses collinear vertices: a 3-vertex straight base
  // produces a 2-vertex left side (the Butt-cap endpoints at P_0 and
  // P_{N-1}).
  CHECK(op.points.size() >= 2);
  CHECK(op.points.size() == op.sources.size());

  for (const auto& p : op.points) {
    CHECK(p.y() == Catch::Approx(+dist).margin(1e-9));
  }
  CHECK(allOnSide(op, base, +1));
  CHECK(baseSegNonDecreasing(op));
  CHECK(op.points.front().x() == Catch::Approx(0.0).margin(1e-9));
  CHECK(op.points.back().x()  == Catch::Approx(2.0).margin(1e-9));
}


// O2 — Same base, side = -1 (right = -y direction).
TEST_CASE("offsetWithClipper2: open 2-segment straight base, side=-1",
          "[offset_clipper2][open][side]") {
  const std::vector<SPoint2> base = {
      SPoint2(0.0, 0.0), SPoint2(1.0, 0.0), SPoint2(2.0, 0.0),
  };
  const double dist = 0.05;

  auto out = offsetWithClipper2(base, false, -1, dist);
  REQUIRE(out.size() == 1);
  const auto& op = out[0];

  CHECK_FALSE(op.is_closed);
  CHECK(op.points.size() >= 2);
  for (const auto& p : op.points) {
    CHECK(p.y() == Catch::Approx(-dist).margin(1e-9));
  }
  CHECK(allOnSide(op, base, -1));
  CHECK(baseSegNonDecreasing(op));
  CHECK(op.points.front().x() == Catch::Approx(0.0).margin(1e-9));
  CHECK(op.points.back().x()  == Catch::Approx(2.0).margin(1e-9));
}


// O3 — L-shape, inward-convex side (side = +1). Convex side of the
// "L" picks up a single miter corner at the bend.
TEST_CASE("offsetWithClipper2: open L-shape, convex (left) side",
          "[offset_clipper2][open][corner]") {
  const std::vector<SPoint2> base = {
      SPoint2(0.0, 0.0), SPoint2(1.0, 0.0), SPoint2(1.0, 1.0),
  };
  const double dist = 0.1;

  auto out = offsetWithClipper2(base, false, +1, dist);
  REQUIRE(out.size() == 1);
  const auto& op = out[0];

  CHECK_FALSE(op.is_closed);
  CHECK(op.points.size() >= 3);
  CHECK(allOnSide(op, base, +1));
  CHECK(baseSegNonDecreasing(op));

  // Endpoints sit on the +y axis at x=0 (P_0 left-cap) and the +x axis
  // at y=1 (P_{N-1} left-cap). The "left" of a CCW L-shape vector is
  // outward of the bend (upper-left direction).
  // After Butt cap at P_0 (segment 0 tangent = +x), the left end-cap
  // point is at (0, +dist). After Butt cap at P_{N-1} (segment 1
  // tangent = +y), the left end-cap point is at (-dist, +1).
  // Note: the order may be either (0,+dist) at front / (-dist,+1) at
  // back, or vice versa, depending on Butt-cap traversal.
}


// O4 — L-shape, concave (right) side. The right side traces the
// inside corner of the L; Clipper2 inserts a snipped or limited-miter
// vertex there.
TEST_CASE("offsetWithClipper2: open L-shape, concave (right) side",
          "[offset_clipper2][open][corner]") {
  const std::vector<SPoint2> base = {
      SPoint2(0.0, 0.0), SPoint2(1.0, 0.0), SPoint2(1.0, 1.0),
  };
  const double dist = 0.1;

  auto out = offsetWithClipper2(base, false, -1, dist);
  REQUIRE(out.size() == 1);
  const auto& op = out[0];

  CHECK_FALSE(op.is_closed);
  CHECK(op.points.size() >= 3);
  CHECK(allOnSide(op, base, -1));
  CHECK(baseSegNonDecreasing(op));
}


// O5 — Smooth-ish open arc (10 segments along a quarter circle).
// Verifies both sides give a clean single run with monotone base_seg.
TEST_CASE("offsetWithClipper2: open quarter-arc, both sides",
          "[offset_clipper2][open][arc]") {
  // 11 vertices along the unit quarter circle (CCW from +x to +y).
  std::vector<SPoint2> base;
  const int N = 11;
  const double half_pi = std::acos(-1.0) * 0.5;
  for (int i = 0; i < N; ++i) {
    const double t = half_pi * static_cast<double>(i)
                     / static_cast<double>(N - 1);
    base.emplace_back(std::cos(t), std::sin(t));
  }
  const double dist = 0.05;

  for (int side : {+1, -1}) {
    auto out = offsetWithClipper2(base, false, side, dist);
    REQUIRE(out.size() >= 1);
    REQUIRE(out[0].points.size() >= static_cast<std::size_t>(N - 1));
    CHECK_FALSE(out[0].is_closed);
    CHECK(allOnSide(out[0], base, side));
    CHECK(baseSegNonDecreasing(out[0]));
  }
}


// O6 — Validate the two sides are geometric mirrors: each side's
// vertex count should match, and every left point should have a
// corresponding right point at the reflected position across the base.
TEST_CASE("offsetWithClipper2: open base — both sides are reflections",
          "[offset_clipper2][open][mirror]") {
  const std::vector<SPoint2> base = {
      SPoint2(0.0, 0.0), SPoint2(0.4, 0.0),
      SPoint2(0.8, 0.0), SPoint2(1.2, 0.0),
  };
  const double dist = 0.07;

  auto left  = offsetWithClipper2(base, false, +1, dist);
  auto right = offsetWithClipper2(base, false, -1, dist);
  REQUIRE(left.size() == 1);
  REQUIRE(right.size() == 1);
  REQUIRE(left[0].points.size() == right[0].points.size());

  // For a straight base on the x-axis, the right side is the left side
  // reflected through y = 0.
  for (std::size_t k = 0; k < left[0].points.size(); ++k) {
    CHECK(left[0].points[k].x()
          == Catch::Approx(right[0].points[k].x()).margin(1e-9));
    CHECK(left[0].points[k].y()
          == Catch::Approx(-right[0].points[k].y()).margin(1e-9));
  }
}


// O7 — Endpoint alignment: the first/last filtered vertex must lie
// near the offset of the corresponding base endpoint (within dist
// along the base-normal direction, ≤ tiny tangential drift from the
// Butt cap).
TEST_CASE("offsetWithClipper2: open base — endpoints align with caps",
          "[offset_clipper2][open][endpoints]") {
  const std::vector<SPoint2> base = {
      SPoint2(0.0, 0.0), SPoint2(1.0, 0.0), SPoint2(2.0, 0.0),
  };
  const double dist = 0.05;

  auto out = offsetWithClipper2(base, false, +1, dist);
  REQUIRE(out.size() == 1);
  const auto& op = out[0];

  // First vertex projects to a foot near P_0 (base_u close to 0 on
  // segment 0).
  REQUIRE(op.sources.front().base_seg == 0);
  CHECK(op.sources.front().base_u == Catch::Approx(0.0).margin(1e-6));

  // Last vertex projects to a foot near P_{N-1} (base_u close to 1
  // on the last segment).
  const int last_seg = static_cast<int>(base.size()) - 2;
  REQUIRE(op.sources.back().base_seg == last_seg);
  CHECK(op.sources.back().base_u == Catch::Approx(1.0).margin(1e-6));
}


// O8 — Closed-branch sanity check: the old closed-input contract is
// untouched (this guards against the open-branch refactor accidentally
// breaking closed inputs).
TEST_CASE("offsetWithClipper2: closed input unchanged by open-branch "
          "refactor",
          "[offset_clipper2][closed][regression]") {
  const std::vector<SPoint2> base = {
      SPoint2(0.0, 0.0), SPoint2(1.0, 0.0),
      SPoint2(1.0, 1.0), SPoint2(0.0, 1.0),
  };
  auto out = offsetWithClipper2(base, /*closed*/ true, -1, 0.1);
  REQUIRE(out.size() == 1);
  CHECK(out[0].is_closed);
  CHECK(polygonAbsArea(out[0].points) == Catch::Approx(0.64).margin(1e-6));
}


// ===================================================================
// Closed input with trailing duplicate (Baseline convention)
// ===================================================================

TEST_CASE("offsetWithClipper2: closed input with trailing duplicate "
          "vertex is handled defensively",
          "[offset_clipper2][closed][defensive]") {
  // PreVABS' PBaseLine commonly stores closed curves with
  // base.front() == base.back() (same vertex pointer). The wrapper
  // must drop the duplicate so Clipper2 does not see a zero-length
  // closing segment.
  const std::vector<SPoint2> base_no_dup = {
      SPoint2(0.0, 0.0), SPoint2(1.0, 0.0),
      SPoint2(1.0, 1.0), SPoint2(0.0, 1.0),
  };
  std::vector<SPoint2> base_dup = base_no_dup;
  base_dup.push_back(SPoint2(0.0, 0.0));   // trailing duplicate

  auto a = offsetWithClipper2(base_no_dup, true, -1, 0.1);
  auto b = offsetWithClipper2(base_dup,    true, -1, 0.1);

  REQUIRE(a.size() == 1);
  REQUIRE(b.size() == 1);
  CHECK(polygonAbsArea(a[0].points)
        == Catch::Approx(polygonAbsArea(b[0].points)).margin(1e-9));
  CHECK(a[0].points.size() == b[0].points.size());
}


// ===================================================================
// Degenerate inputs
// ===================================================================

TEST_CASE("offsetWithClipper2: degenerate inputs return empty",
          "[offset_clipper2][degenerate]") {
  std::vector<OffsetPolygon> out;

  SECTION("empty base") {
    out = offsetWithClipper2({}, true, -1, 0.1);
    CHECK(out.empty());
  }
  SECTION("single-point base") {
    out = offsetWithClipper2({SPoint2(0.5, 0.5)}, true, -1, 0.1);
    CHECK(out.empty());
  }
  SECTION("non-positive dist") {
    const std::vector<SPoint2> sq = {
        SPoint2(0, 0), SPoint2(1, 0), SPoint2(1, 1), SPoint2(0, 1)};
    CHECK(offsetWithClipper2(sq, true, -1,  0.0).empty());
    CHECK(offsetWithClipper2(sq, true, -1, -0.1).empty());
  }
}


// ===================================================================
// Inset eating the input completely → empty result, no crash
// ===================================================================

TEST_CASE("offsetWithClipper2: oversized inward offset returns empty",
          "[offset_clipper2][closed][empty]") {
  const std::vector<SPoint2> base = {
      SPoint2(0.0, 0.0), SPoint2(1.0, 0.0),
      SPoint2(1.0, 0.02), SPoint2(0.0, 0.02),
  };
  // Half-thickness is 0.01; an inward offset of 0.05 eats it whole.
  auto out = offsetWithClipper2(base, true, -1, 0.05);
  CHECK(out.empty());
}


// ===================================================================
// Concave L-shape — sources should cover all five input segments
// ===================================================================

TEST_CASE("offsetWithClipper2: concave L-shape — all vertices attributed",
          "[offset_clipper2][closed][concave]") {
  // CCW-oriented L:
  //   (0,0) -> (2,0) -> (2,1) -> (1,1) -> (1,2) -> (0,2) -> close
  const std::vector<SPoint2> base = {
      SPoint2(0.0, 0.0), SPoint2(2.0, 0.0),
      SPoint2(2.0, 1.0), SPoint2(1.0, 1.0),
      SPoint2(1.0, 2.0), SPoint2(0.0, 2.0),
  };
  auto out = offsetWithClipper2(base, true, /*side*/ -1, 0.1);
  REQUIRE(out.size() == 1);
  const auto& op = out[0];

  // No miters here are wider than 1.5*dist (the L's external corners
  // are 90°, miter = sqrt(2)*dist ≈ 1.41*dist; the concave inner
  // corner produces a join vertex at 0.1414 from one segment, still
  // attributed). Every output vertex must have a valid source.
  CHECK(countSourcesAttributed(op) == static_cast<int>(op.sources.size()));

  // Per-segment coverage is fragile due to corner-vertex tie-breaking
  // (each corner vertex sits at distance `dist` from two adjacent
  // segments and the linear-scan picks the lower index). Assert
  // "majority" coverage instead.
  std::vector<int> seen(base.size(), 0);
  for (const auto& s : op.sources) seen[s.base_seg] = 1;
  CHECK(std::count(seen.begin(), seen.end(), 1) >= 4);
}


// ===================================================================
// Disconnected: pinching a thin C-shape produces 2 output polygons
// ===================================================================

TEST_CASE("offsetWithClipper2: thin pinch produces multiple "
          "disconnected polygons",
          "[offset_clipper2][closed][disconnected]") {
  // Dumbbell-shaped contour: two 1.0×0.4 lobes connected by a 0.04-thick
  // bridge. Inset 0.03 pinches the bridge to disconnection.
  // CCW outer boundary, all coordinates in chord units.
  const std::vector<SPoint2> base = {
      // left lobe bottom
      SPoint2(0.0,  0.0), SPoint2(0.4,  0.0),
      // bridge bottom
      SPoint2(0.4, -0.02), SPoint2(0.6, -0.02),
      // right lobe bottom
      SPoint2(0.6,  0.0), SPoint2(1.0,  0.0),
      SPoint2(1.0,  0.4),
      // right lobe top
      SPoint2(0.6,  0.4),
      // bridge top
      SPoint2(0.6,  0.02), SPoint2(0.4,  0.02),
      // left lobe top
      SPoint2(0.4,  0.4), SPoint2(0.0,  0.4),
  };
  auto out = offsetWithClipper2(base, true, /*side*/ -1, 0.03);
  REQUIRE(out.size() >= 2);
  for (const auto& op : out) {
    // Both pieces should still have valid source attribution for
    // most of their vertices (interior loops produced by the inset).
    CHECK(op.is_closed);
    CHECK(op.points.size() == op.sources.size());
    CHECK(op.points.size() >= 3);
  }
}


// ===================================================================
// Stage C: planReverseMatch — pure-logic reverse-match bridge.
//
// Scope: the SPoint2-only layer that consumes an OffsetPolygon and
// produces a BaseOffsetMap satisfying the staircase invariant
// declared in geo_types.hpp. PDCELVertex wrapping is exercised
// end-to-end in the integration tests after Stage D wires this into
// offset.cpp; the unit-test suite here focuses on monotone repair,
// rotation, dropped-range detection, and primary-polygon picking.
// ===================================================================

namespace {

// Helpers used by Stage C tests.

bool isValidStaircase(const BaseOffsetMap& m) {
  if (m.empty()) return false;
  for (std::size_t i = 0; i < m.size(); ++i) {
    if (m[i].base < 0 || m[i].offset < 0) return false;
    if (i == 0) continue;
    const int db = m[i].base   - m[i - 1].base;
    const int dd = m[i].offset - m[i - 1].offset;
    if (db < 0 || dd < 0 || db > 1 || dd > 1) return false;
  }
  return true;
}

// Synthetic OffsetPolygon builder: each entry is (x, y, base_seg,
// base_u). base_seg = -1 marks an unattributed join vertex.
OffsetPolygon makePolygon(
    const std::vector<std::tuple<double, double, int, double>>& vs) {
  OffsetPolygon p;
  p.is_closed = true;
  p.points.reserve(vs.size());
  p.sources.reserve(vs.size());
  for (const auto& t : vs) {
    p.points.emplace_back(std::get<0>(t), std::get<1>(t));
    p.sources.push_back({std::get<2>(t), std::get<3>(t)});
  }
  return p;
}

// Open variant of the synthetic builder (is_closed = false). Used by
// Phase 2 bridge tests that drive planReverseMatch with hand-crafted
// open runs.
OffsetPolygon makeOpenPolygon(
    const std::vector<std::tuple<double, double, int, double>>& vs) {
  OffsetPolygon p = makePolygon(vs);
  p.is_closed = false;
  return p;
}

}  // namespace


// -------------------------------------------------------------------
// Clean closed square — the canonical clean-staircase shape.
// -------------------------------------------------------------------

TEST_CASE("planReverseMatch: closed square — clean staircase",
          "[offset_clipper2][bridge][closed]") {
  // base: 4 distinct vertices of the unit square.
  const std::vector<SPoint2> base = {
      SPoint2(0.0, 0.0), SPoint2(1.0, 0.0),
      SPoint2(1.0, 1.0), SPoint2(0.0, 1.0),
  };
  // Synthetic offset polygon — one offset vertex near each base
  // vertex (u=0.5 → rounds up to the segment's end). This is the
  // attribution pattern Clipper2 produces at sharp 90° corners.
  OffsetPolygon prim = makePolygon({
      // (point coords irrelevant for the bridge — only sources matter)
      {0.1, 0.1, /*seg*/ 0, /*u*/ 0.5},  // -> base 1
      {0.9, 0.1, /*seg*/ 1, /*u*/ 0.5},  // -> base 2
      {0.9, 0.9, /*seg*/ 2, /*u*/ 0.5},  // -> base 3
      {0.1, 0.9, /*seg*/ 3, /*u*/ 0.5},  // -> base 0 (mod 4)
  });

  const auto plan = planReverseMatch(
      base, /*closed*/ true, /*side*/ -1, 0.1, {prim});
  REQUIRE(plan.ok);
  CHECK(plan.closed);

  CHECK(isValidStaircase(plan.id_pairs));
  // First entry pinned at base 0; last entry closes the loop.
  REQUIRE(plan.id_pairs.size() >= 2);
  CHECK(plan.id_pairs.front().base   == 0);
  CHECK(plan.id_pairs.front().offset == 0);
  CHECK(plan.id_pairs.back().base    == 4);
  CHECK(plan.id_pairs.back().offset  == 4);
  CHECK(plan.dropped_base_ranges_lo.empty());
  CHECK(plan.dropped_base_ranges_hi.empty());
  CHECK(plan.offset_points.size() == 4);
}


// -------------------------------------------------------------------
// Rotation — when Clipper2's polygon starts mid-loop, the bridge
// must rotate so the staircase begins at base index 0.
// -------------------------------------------------------------------

TEST_CASE("planReverseMatch: closed square — input rotated, output anchored",
          "[offset_clipper2][bridge][closed][rotation]") {
  const std::vector<SPoint2> base = {
      SPoint2(0.0, 0.0), SPoint2(1.0, 0.0),
      SPoint2(1.0, 1.0), SPoint2(0.0, 1.0),
  };
  // Same vertices as the previous test but Clipper2 started the
  // walk one vertex late (its choice of start vertex is
  // implementation-defined). Use u = 0.7 so the attribution
  // `seg + (u > 0.5 ? 1 : 0)` rounds to the "next" base vertex,
  // making the cand_base sequence [3, 0, 1, 2] and forcing a
  // rotation.
  OffsetPolygon prim = makePolygon({
      {0.9, 0.9, /*seg*/ 2, /*u*/ 0.7},  // -> base 3
      {0.1, 0.9, /*seg*/ 3, /*u*/ 0.7},  // -> base 0 (mod 4)
      {0.1, 0.1, /*seg*/ 0, /*u*/ 0.7},  // -> base 1
      {0.9, 0.1, /*seg*/ 1, /*u*/ 0.7},  // -> base 2
  });

  const auto plan = planReverseMatch(base, true, -1, 0.1, {prim});
  REQUIRE(plan.ok);
  CHECK(isValidStaircase(plan.id_pairs));
  CHECK(plan.id_pairs.front().base   == 0);
  CHECK(plan.id_pairs.front().offset == 0);
  CHECK(plan.id_pairs.back().base    == 4);
  CHECK(plan.id_pairs.back().offset  == 4);
  CHECK(plan.dropped_base_ranges_lo.empty());

  // After rotation, offset_points[0] should be the vertex that
  // attributed to base index 0 — i.e. (0.1, 0.9).
  CHECK(plan.offset_points[0].x() == Catch::Approx(0.1));
  CHECK(plan.offset_points[0].y() == Catch::Approx(0.9));
}


// -------------------------------------------------------------------
// Dropped base range — Clipper2 ate base index 2.
// -------------------------------------------------------------------

TEST_CASE("planReverseMatch: dropped base range emerges from index skip",
          "[offset_clipper2][bridge][closed][dropped]") {
  // 5-vertex base. Offset has 4 vertices: base index 2 is skipped.
  // Expected: dropped range = [2, 2], staircase still valid.
  const std::vector<SPoint2> base = {
      SPoint2(0.0, 0.0), SPoint2(1.0, 0.0),
      SPoint2(1.5, 0.5), SPoint2(1.0, 1.0),
      SPoint2(0.0, 1.0),
  };
  // 4 attributed vertices skipping a target at base 2. u = 0.7 so
  // attribution rounds to seg + 1.
  OffsetPolygon prim = makePolygon({
      {0.1, 0.1, /*seg*/ 0, /*u*/ 0.7},  // -> base 1
      // (no base-2 vertex)
      {1.0, 0.6, /*seg*/ 2, /*u*/ 0.7},  // -> base 3
      {0.5, 0.9, /*seg*/ 3, /*u*/ 0.7},  // -> base 4
      {0.1, 0.5, /*seg*/ 4, /*u*/ 0.7},  // -> base 0 (mod 5)
  });

  const auto plan = planReverseMatch(base, true, -1, 0.1, {prim});
  REQUIRE(plan.ok);
  CHECK(isValidStaircase(plan.id_pairs));

  // Dropped range must include base index 2.
  REQUIRE(plan.dropped_base_ranges_lo.size()
          == plan.dropped_base_ranges_hi.size());
  REQUIRE(!plan.dropped_base_ranges_lo.empty());
  bool found_2 = false;
  for (std::size_t i = 0; i < plan.dropped_base_ranges_lo.size(); ++i) {
    const int lo = plan.dropped_base_ranges_lo[i];
    const int hi = plan.dropped_base_ranges_hi[i];
    if (lo <= 2 && 2 <= hi) found_2 = true;
  }
  CHECK(found_2);

  // The dropped base index 2 must still appear in id_pairs (so
  // PSegment::build stays in-bounds).
  bool base_2_present = false;
  for (const auto& p : plan.id_pairs) {
    if (p.base == 2) { base_2_present = true; break; }
  }
  CHECK(base_2_present);
}


// -------------------------------------------------------------------
// Disconnected polygons — bridge picks the largest, drops the rest.
// -------------------------------------------------------------------

TEST_CASE("planReverseMatch: largest polygon picked from disconnected set",
          "[offset_clipper2][bridge][disconnected]") {
  const std::vector<SPoint2> base = {
      SPoint2(0.0, 0.0), SPoint2(1.0, 0.0),
      SPoint2(1.0, 1.0), SPoint2(0.0, 1.0),
  };
  OffsetPolygon big = makePolygon({
      {0.1, 0.1, 0, 0.5},
      {0.9, 0.1, 1, 0.5},
      {0.9, 0.9, 2, 0.5},
      {0.1, 0.9, 3, 0.5},
  });
  // Tiny pinch-piece on the side: 0.01 x 0.01 — area 1e-4, vs the
  // big square's area 0.64.
  OffsetPolygon small = makePolygon({
      {1.50, 0.50, -1, 0.0},
      {1.51, 0.50, -1, 0.0},
      {1.51, 0.51, -1, 0.0},
      {1.50, 0.51, -1, 0.0},
  });

  const auto plan = planReverseMatch(
      base, true, -1, 0.1, {small, big});
  REQUIRE(plan.ok);
  CHECK(plan.offset_points.size() == 4);
  // Offset points must come from the big square, not the tiny piece.
  for (const auto& p : plan.offset_points) {
    CHECK(p.x() >= 0.0);
    CHECK(p.x() <= 1.0);
    CHECK(p.y() >= 0.0);
    CHECK(p.y() <= 1.0);
  }

  // Stage F multi-branch diagnostics.
  CHECK(plan.primary_polygon_area == Catch::Approx(0.64).margin(1e-6));
  REQUIRE(plan.dropped_polygon_areas.size() == 1);
  CHECK(plan.dropped_polygon_areas[0] == Catch::Approx(1e-4).margin(1e-9));
}


// -------------------------------------------------------------------
// Stage F centroid tie-break: when two polygons are within 5% of the
// best area, the one whose centroid is closer to the base bbox
// centroid wins. The largest-only picker would pick by index order;
// this test asserts the tie-break is deterministic and geometry-aware.
// -------------------------------------------------------------------

TEST_CASE("planReverseMatch: centroid tie-break selects polygon "
          "closest to base bbox centroid",
          "[offset_clipper2][bridge][disconnected][tiebreak]") {
  // Base bbox centroid is at (0, 0).
  const std::vector<SPoint2> base = {
      SPoint2(-1.0, -1.0), SPoint2( 1.0, -1.0),
      SPoint2( 1.0,  1.0), SPoint2(-1.0,  1.0),
  };

  // Polygon A: 1.0 × 1.0 square centred at (10, 10). Area 1.0.
  // Far from the base centroid.
  OffsetPolygon a = makePolygon({
      { 9.5,  9.5, -1, 0.0},
      {10.5,  9.5, -1, 0.0},
      {10.5, 10.5, -1, 0.0},
      { 9.5, 10.5, -1, 0.0},
  });
  // Polygon B: 1.0 × 1.0 square centred at (0, 0). Same area.
  // Distance 0 to base centroid → wins tie-break. Real attribution
  // sources so the staircase build can proceed off this polygon.
  OffsetPolygon b = makePolygon({
      {-0.5, -0.5, 0, 0.5},
      { 0.5, -0.5, 1, 0.5},
      { 0.5,  0.5, 2, 0.5},
      {-0.5,  0.5, 3, 0.5},
  });

  // Feed in (a, b) order. Without centroid tie-break, the leader pass
  // would pick a (first match at best_area); with the Stage F tie-
  // break, b wins.
  const auto plan = planReverseMatch(base, true, -1, 1.0, {a, b});
  REQUIRE(plan.ok);
  // The kept polygon's vertices must be near the base centroid (b),
  // NOT at (10, 10) (a).
  for (const auto& p : plan.offset_points) {
    CHECK(std::fabs(p.x()) <= 1.0);
    CHECK(std::fabs(p.y()) <= 1.0);
  }
  REQUIRE(plan.dropped_polygon_areas.size() == 1);
  CHECK(plan.primary_polygon_area
        == Catch::Approx(plan.dropped_polygon_areas[0]).margin(1e-9));
}


// -------------------------------------------------------------------
// Empty / degenerate inputs — bridge stays defensive, ok=false.
// -------------------------------------------------------------------

TEST_CASE("planReverseMatch: empty polygon list returns ok=false",
          "[offset_clipper2][bridge][degenerate]") {
  const std::vector<SPoint2> base = {
      SPoint2(0, 0), SPoint2(1, 0), SPoint2(0, 1)};
  const auto plan = planReverseMatch(base, true, -1, 0.1, {});
  CHECK_FALSE(plan.ok);
}

TEST_CASE("planReverseMatch: fully unattributed sources return ok=false",
          "[offset_clipper2][bridge][degenerate]") {
  const std::vector<SPoint2> base = {
      SPoint2(0, 0), SPoint2(1, 0), SPoint2(0, 1)};
  OffsetPolygon p = makePolygon({
      {0.5, 0.5, -1, 0.0},
      {0.6, 0.5, -1, 0.0},
      {0.5, 0.6, -1, 0.0},
  });
  const auto plan = planReverseMatch(base, true, -1, 0.1, {p});
  CHECK_FALSE(plan.ok);
}


// -------------------------------------------------------------------
// End-to-end: feed offsetWithClipper2's real output through the
// bridge for the closed square — both halves of the Stage B/C
// pipeline cooperate.
// -------------------------------------------------------------------

TEST_CASE("planReverseMatch: end-to-end with offsetWithClipper2 on a "
          "closed square",
          "[offset_clipper2][bridge][closed][e2e]") {
  const std::vector<SPoint2> base = {
      SPoint2(0.0, 0.0), SPoint2(1.0, 0.0),
      SPoint2(1.0, 1.0), SPoint2(0.0, 1.0),
  };
  auto polys = offsetWithClipper2(base, /*closed*/ true,
                                  /*side*/ -1, 0.1);
  REQUIRE(polys.size() == 1);

  const auto plan = planReverseMatch(
      base, /*closed*/ true, /*side*/ -1, 0.1, polys);
  REQUIRE(plan.ok);
  CHECK(isValidStaircase(plan.id_pairs));
  CHECK(plan.id_pairs.front().base   == 0);
  CHECK(plan.id_pairs.front().offset == 0);
  CHECK(plan.id_pairs.back().base
        == static_cast<int>(base.size()));
  CHECK(plan.dropped_base_ranges_lo.empty());
}


// -------------------------------------------------------------------
// Stage G: pseudo-airfoil pipeline coverage (plan-20260514 §8.1).
//
// A 10-vertex closed CCW airfoil-like contour with a TE cusp at
// (1, 0) and gentle camber. Two scenarios:
//   (a) small inward offset (dist = 0.005): every base vertex has
//       h ≫ dist, expect a clean staircase, no dropped ranges, and
//       offset_points well inside the contour.
//   (b) thick inward offset (dist = 0.04): the TE region has
//       h ≲ dist, expect a valid staircase + non-empty
//       dropped_base_ranges_* near the TE indices.
// Both branches go through offsetWithClipper2 + planReverseMatch
// (Stage B + Stage C) so a regression in either layer is caught.
// -------------------------------------------------------------------

namespace {

std::vector<SPoint2> pseudoAirfoilCCW() {
  // CCW around the contour: TE → top → LE → bottom → TE.
  // Chord 1.0, max half-thickness 0.05, TE cusp at (1, 0).
  return {
      SPoint2(1.00,  0.00),   // 0  TE
      SPoint2(0.90,  0.025),  // 1
      SPoint2(0.70,  0.05),   // 2
      SPoint2(0.40,  0.05),   // 3
      SPoint2(0.10,  0.03),   // 4
      SPoint2(0.00,  0.00),   // 5  LE
      SPoint2(0.10, -0.03),   // 6
      SPoint2(0.40, -0.05),   // 7
      SPoint2(0.70, -0.05),   // 8
      SPoint2(0.90, -0.025),  // 9
  };
}

}  // namespace

TEST_CASE("planReverseMatch: pseudo-airfoil + small dist — clean staircase",
          "[offset_clipper2][bridge][closed][airfoil][e2e]") {
  const auto base = pseudoAirfoilCCW();
  const double dist = 0.005;

  auto polys = offsetWithClipper2(base, /*closed*/ true,
                                  /*side*/ -1, dist);
  REQUIRE(polys.size() == 1);

  const auto plan = planReverseMatch(base, true, -1, dist, polys);
  REQUIRE(plan.ok);
  CHECK(isValidStaircase(plan.id_pairs));
  CHECK(plan.id_pairs.front().base   == 0);
  CHECK(plan.id_pairs.front().offset == 0);
  CHECK(plan.id_pairs.back().base
        == static_cast<int>(base.size()));
  // Small dist should leave the TE intact even with the cusp.
  CHECK(plan.dropped_base_ranges_lo.empty());
  CHECK(plan.dropped_base_ranges_hi.empty());
  CHECK(plan.dropped_polygon_areas.empty());
}

TEST_CASE("planReverseMatch: pseudo-airfoil + thick dist — TE region "
          "dropped, staircase still valid",
          "[offset_clipper2][bridge][closed][airfoil][thin][e2e]") {
  const auto base = pseudoAirfoilCCW();
  const double dist = 0.04;  // ~80% of max half-thickness → eats TE.

  auto polys = offsetWithClipper2(base, true, -1, dist);
  REQUIRE(!polys.empty());

  const auto plan = planReverseMatch(base, true, -1, dist, polys);
  REQUIRE(plan.ok);
  CHECK(isValidStaircase(plan.id_pairs));

  // For a TE-cusp airfoil at dist ≈ 80% half-thickness, Clipper2
  // necessarily eats the region around the cusp. The bridge must
  // report this via dropped_base_ranges (anchorClosed prepends a
  // range starting at base 0, or buildIdPairs records a jump).
  REQUIRE(plan.dropped_base_ranges_lo.size()
          == plan.dropped_base_ranges_hi.size());
  CHECK_FALSE(plan.dropped_base_ranges_lo.empty());

  // First entry must still be at (0, 0) — Stage C's anchorClosed
  // guarantees the staircase starts at base index 0 for closed
  // inputs, even when base 0 (the TE cusp) was geometrically dropped.
  CHECK(plan.id_pairs.front().base   == 0);
  CHECK(plan.id_pairs.front().offset == 0);
}


// -------------------------------------------------------------------
// Stage G: staircase invariant across noise perturbation. Tiny
// floating-point jitter at every base vertex must not break the
// staircase produced by Stage C — proves the source-attribution
// tolerances are robust to Clipper2's precision=8 grid.
// -------------------------------------------------------------------

TEST_CASE("planReverseMatch: small numeric noise preserves staircase",
          "[offset_clipper2][bridge][closed][invariant][robust]") {
  // 8-vertex octagon-ish curve perturbed by ±1e-8 per coordinate.
  // 1e-8 is the Clipper2 1.4.0 grid resolution at precision=8.
  std::vector<SPoint2> base = {
      SPoint2( 1.00 + 1e-8,  0.00         ),
      SPoint2( 0.70         ,  0.70 - 2e-8),
      SPoint2( 0.00 - 1e-8,  1.00         ),
      SPoint2(-0.70         ,  0.70 + 1e-8),
      SPoint2(-1.00         ,  0.00 - 1e-8),
      SPoint2(-0.70 + 2e-8, -0.70         ),
      SPoint2( 0.00         , -1.00 + 1e-8),
      SPoint2( 0.70 - 1e-8, -0.70         ),
  };

  auto polys = offsetWithClipper2(base, true, -1, 0.05);
  REQUIRE(polys.size() == 1);
  const auto plan = planReverseMatch(base, true, -1, 0.05, polys);
  REQUIRE(plan.ok);
  CHECK(isValidStaircase(plan.id_pairs));
  CHECK(plan.id_pairs.front().base   == 0);
  CHECK(plan.id_pairs.back().base
        == static_cast<int>(base.size()));
}


// ===================================================================
// Phase 2 — planReverseMatch open-input branch
//
// Open inputs:
//   - Primary picking uses base_seg span (not area).
//   - No rotation (the Phase 1 extractor orients the run already).
//   - No wrap pair at the staircase tail (`(N-1, M-1)` instead of
//     `(N, M)`).
//   - anchorAtZero still applies if the run anchor doesn't reach base[0].
// ===================================================================

// O-Bridge-1 — Clean staircase for a 4-vertex open base + 4-vertex open
// run, one offset vertex per base vertex.
TEST_CASE("planReverseMatch: open polyline — clean staircase",
          "[offset_clipper2][bridge][open]") {
  const std::vector<SPoint2> base = {
      SPoint2(0.0, 0.0), SPoint2(1.0, 0.0),
      SPoint2(2.0, 0.0), SPoint2(3.0, 0.0),
  };
  // One offset vertex anchored at each base vertex via segments
  // 0/1/2 with u rounded to {0, 1, 1, 1} so cand_base = [0, 2, 3, 3].
  // For a strict 1-per-base correspondence we use u alternating so
  // attributeOne maps to [0, 1, 2, 3].
  OffsetPolygon prim = makeOpenPolygon({
      {0.0, 0.05, /*seg*/ 0, /*u*/ 0.0},  // -> base 0
      {1.0, 0.05, /*seg*/ 0, /*u*/ 1.0},  // -> base 1
      {2.0, 0.05, /*seg*/ 1, /*u*/ 1.0},  // -> base 2
      {3.0, 0.05, /*seg*/ 2, /*u*/ 1.0},  // -> base 3
  });

  const auto plan = planReverseMatch(
      base, /*closed*/ false, /*side*/ +1, 0.05, {prim});
  REQUIRE(plan.ok);
  CHECK_FALSE(plan.closed);
  CHECK(isValidStaircase(plan.id_pairs));

  // Open-staircase endpoints: (0, 0) and (N-1, M-1) — NO wrap pair.
  REQUIRE(plan.id_pairs.size() == 4);
  CHECK(plan.id_pairs.front().base   == 0);
  CHECK(plan.id_pairs.front().offset == 0);
  CHECK(plan.id_pairs.back().base    == 3);   // N-1
  CHECK(plan.id_pairs.back().offset  == 3);   // M-1

  CHECK(plan.dropped_base_ranges_lo.empty());
  CHECK(plan.dropped_base_ranges_hi.empty());
  CHECK(plan.offset_points.size() == 4);

  // Diagnostics: open branch leaves area fields at zero/empty.
  CHECK(plan.primary_polygon_area == 0.0);
  CHECK(plan.dropped_polygon_areas.empty());
}

// O-Bridge-2 — A skipped base segment surfaces as a dropped range.
TEST_CASE("planReverseMatch: open polyline — dropped base range from skip",
          "[offset_clipper2][bridge][open][dropped]") {
  // 5 base vertices, 3 offset vertices: skip seg 1 entirely so cand
  // jumps from base 0 → base 3 (2 base indices dropped).
  const std::vector<SPoint2> base = {
      SPoint2(0.0, 0.0), SPoint2(1.0, 0.0), SPoint2(2.0, 0.0),
      SPoint2(3.0, 0.0), SPoint2(4.0, 0.0),
  };
  OffsetPolygon prim = makeOpenPolygon({
      {0.0, 0.05, /*seg*/ 0, /*u*/ 0.0},  // -> base 0
      {3.0, 0.05, /*seg*/ 2, /*u*/ 1.0},  // -> base 3
      {4.0, 0.05, /*seg*/ 3, /*u*/ 1.0},  // -> base 4
  });

  const auto plan = planReverseMatch(
      base, false, +1, 0.05, {prim});
  REQUIRE(plan.ok);
  CHECK(isValidStaircase(plan.id_pairs));

  // Front pinned at (0,0), back at (N-1, M-1) = (4, 2).
  CHECK(plan.id_pairs.front().base   == 0);
  CHECK(plan.id_pairs.front().offset == 0);
  CHECK(plan.id_pairs.back().base    == 4);
  CHECK(plan.id_pairs.back().offset  == 2);

  // Forward-fill inserts (1, 0) and (2, 0) — base 1 and 2 share
  // offset index 0. The skip is recorded as a dropped range [1, 2].
  REQUIRE(plan.dropped_base_ranges_lo.size() == 1);
  CHECK(plan.dropped_base_ranges_lo[0] == 1);
  CHECK(plan.dropped_base_ranges_hi[0] == 2);
}

// O-Bridge-3 — pickPrimaryOpen selects the run with the widest
// base_seg span when multiple runs come in.
TEST_CASE("planReverseMatch: open polyline — widest base_seg span wins",
          "[offset_clipper2][bridge][open][pick]") {
  const std::vector<SPoint2> base = {
      SPoint2(0.0, 0.0), SPoint2(1.0, 0.0), SPoint2(2.0, 0.0),
      SPoint2(3.0, 0.0), SPoint2(4.0, 0.0),
  };
  // Narrow run: spans only segments 0..1 (span = 1).
  OffsetPolygon narrow = makeOpenPolygon({
      {0.0, 0.05, /*seg*/ 0, /*u*/ 0.0},
      {1.0, 0.05, /*seg*/ 0, /*u*/ 1.0},
      {2.0, 0.05, /*seg*/ 1, /*u*/ 1.0},
  });
  // Wide run: spans segments 0..3 (span = 3) — should win.
  OffsetPolygon wide = makeOpenPolygon({
      {0.0, 0.05, /*seg*/ 0, /*u*/ 0.0},
      {2.0, 0.05, /*seg*/ 1, /*u*/ 1.0},
      {4.0, 0.05, /*seg*/ 3, /*u*/ 1.0},
  });

  const auto plan = planReverseMatch(
      base, false, +1, 0.05, {narrow, wide});
  REQUIRE(plan.ok);
  // The wide run produces 3 offset_points; narrow would produce 3 too
  // but with different base_seg coverage, so check the staircase
  // reaches base[N-1] = 4.
  CHECK(plan.id_pairs.back().base   == 4);
  CHECK(plan.id_pairs.back().offset == 2);   // wide.points.size() - 1
}

// O-Bridge-4 — End-to-end: feed offsetWithClipper2's open output into
// planReverseMatch and check the full pipeline closes cleanly.
TEST_CASE("planReverseMatch: open L-shape end-to-end with Phase 1 core",
          "[offset_clipper2][bridge][open][e2e]") {
  // 3-vertex L-shape; Phase 1 produces a one-sided open polyline.
  const std::vector<SPoint2> base = {
      SPoint2(0.0, 0.0), SPoint2(1.0, 0.0), SPoint2(1.0, 1.0),
  };
  const double dist = 0.1;

  // Both sides — independently.
  for (int side : {+1, -1}) {
    auto polys = offsetWithClipper2(base, /*closed*/ false, side, dist);
    REQUIRE(polys.size() >= 1);
    const auto plan = planReverseMatch(
        base, /*closed*/ false, side, dist, polys);
    REQUIRE(plan.ok);
    CHECK_FALSE(plan.closed);
    CHECK(isValidStaircase(plan.id_pairs));
    // (0, 0) at the head, (N-1, M-1) at the tail.
    CHECK(plan.id_pairs.front().base   == 0);
    CHECK(plan.id_pairs.front().offset == 0);
    CHECK(plan.id_pairs.back().base    == 2);  // N-1
    CHECK(plan.id_pairs.back().offset
          == static_cast<int>(plan.offset_points.size()) - 1);
  }
}


// ===================================================================
// planReverseMatchByNearest — point-to-point nearest pairing variant.
// See issue-20260520-base-offset-nearest-pairing.md.
//
// Same signature as planReverseMatch. The differences this suite
// exercises:
//   - On clean cases the staircase is structurally identical to
//     planReverseMatch (sanity / regression).
//   - On a sharp-fold base (the MH104 TE failure mode), the new
//     algorithm produces a valid monotone staircase where the
//     segment-projection + u-midpoint round would mis-assign.
//   - On open inputs, attribution is point-to-point, so the
//     forward-pass + reverse-pass single-switch invariant holds.
// ===================================================================

namespace {

// Build an OffsetPolygon where the per-vertex `points` is the raw
// geometry. `srcs` populates `sources` parallel to `points` — needed
// by `pickPrimaryOpen` for open inputs (which selects by base_seg
// span). Closed inputs ignore `sources` (pickPrimary uses signed area)
// so callers may pass an empty list.
OffsetPolygon makeNearestPolygon(
    const std::vector<SPoint2>& pts,
    bool closed,
    const std::vector<prevabs::geo::OffsetVertexSource>& srcs = {}) {
  OffsetPolygon p;
  p.is_closed = closed;
  p.points    = pts;
  if (srcs.empty()) {
    p.sources.assign(pts.size(),
                     prevabs::geo::OffsetVertexSource{-1, 0.0});
  } else {
    p.sources = srcs;
  }
  p.resampled.assign(pts.size(), false);
  return p;
}

}  // namespace


// -------------------------------------------------------------------
// N-Closed-1 — closed square: structurally same staircase as the
// legacy bridge on a clean case.
// -------------------------------------------------------------------

TEST_CASE("planReverseMatchByNearest: closed square — clean staircase",
          "[offset_clipper2][bridge][nearest][closed]") {
  const std::vector<SPoint2> base = {
      SPoint2(0.0, 0.0), SPoint2(1.0, 0.0),
      SPoint2(1.0, 1.0), SPoint2(0.0, 1.0),
  };
  // 4 offset vertices near each base corner — a 0.9×0.9 inset
  // contour with one offset point per base vertex.
  OffsetPolygon prim = makeNearestPolygon({
      SPoint2(0.05, 0.05), SPoint2(0.95, 0.05),
      SPoint2(0.95, 0.95), SPoint2(0.05, 0.95),
  }, /*closed*/ true);

  const auto plan = planReverseMatchByNearest(
      base, /*closed*/ true, /*side*/ -1, /*dist*/ 0.1, {prim});
  REQUIRE(plan.ok);
  CHECK(plan.closed);
  CHECK(isValidStaircase(plan.id_pairs));

  REQUIRE(plan.id_pairs.size() >= 2);
  CHECK(plan.id_pairs.front().base   == 0);
  CHECK(plan.id_pairs.front().offset == 0);
  CHECK(plan.id_pairs.back().base    == 4);  // wrap (N, M)
  CHECK(plan.id_pairs.back().offset  == 4);
  CHECK(plan.dropped_base_ranges_lo.empty());
  CHECK(plan.dropped_base_ranges_hi.empty());
  CHECK(plan.offset_points.size() == 4);
}


// -------------------------------------------------------------------
// N-Closed-2 — input rotated → anchor at base 0.
// -------------------------------------------------------------------

TEST_CASE("planReverseMatchByNearest: closed square — rotated input "
          "anchored at base 0",
          "[offset_clipper2][bridge][nearest][closed][rotation]") {
  const std::vector<SPoint2> base = {
      SPoint2(0.0, 0.0), SPoint2(1.0, 0.0),
      SPoint2(1.0, 1.0), SPoint2(0.0, 1.0),
  };
  // Same vertices but starting walk at the corner near base[3].
  OffsetPolygon prim = makeNearestPolygon({
      SPoint2(0.05, 0.95),   // closest to base[3]
      SPoint2(0.05, 0.05),   // closest to base[0]
      SPoint2(0.95, 0.05),   // closest to base[1]
      SPoint2(0.95, 0.95),   // closest to base[2]
  }, /*closed*/ true);

  const auto plan = planReverseMatchByNearest(base, true, -1, 0.1, {prim});
  REQUIRE(plan.ok);
  CHECK(isValidStaircase(plan.id_pairs));

  CHECK(plan.id_pairs.front().base   == 0);
  CHECK(plan.id_pairs.front().offset == 0);
  CHECK(plan.id_pairs.back().base    == 4);
  CHECK(plan.id_pairs.back().offset  == 4);

  // After rotation, offset_points[0] is the one closest to base[0]
  // — i.e. (0.05, 0.05).
  CHECK(plan.offset_points[0].x() == Catch::Approx(0.05));
  CHECK(plan.offset_points[0].y() == Catch::Approx(0.05));
}


// -------------------------------------------------------------------
// N-Open-1 — open polyline, 1:1 clean pairing.
// -------------------------------------------------------------------

TEST_CASE("planReverseMatchByNearest: open polyline — clean staircase",
          "[offset_clipper2][bridge][nearest][open]") {
  const std::vector<SPoint2> base = {
      SPoint2(0.0, 0.0), SPoint2(1.0, 0.0),
      SPoint2(2.0, 0.0), SPoint2(3.0, 0.0),
  };
  OffsetPolygon prim = makeNearestPolygon(
      {SPoint2(0.0, 0.05), SPoint2(1.0, 0.05),
       SPoint2(2.0, 0.05), SPoint2(3.0, 0.05)},
      /*closed*/ false,
      // Populate sources so pickPrimaryOpen sees a base_seg span.
      {{0, 0.0}, {0, 1.0}, {1, 1.0}, {2, 1.0}});

  const auto plan = planReverseMatchByNearest(
      base, /*closed*/ false, /*side*/ +1, /*dist*/ 0.05, {prim});
  REQUIRE(plan.ok);
  CHECK_FALSE(plan.closed);
  CHECK(isValidStaircase(plan.id_pairs));

  // Open: (0, 0) → (N-1, M-1) — NO wrap pair.
  REQUIRE(plan.id_pairs.size() == 4);
  CHECK(plan.id_pairs.front().base   == 0);
  CHECK(plan.id_pairs.front().offset == 0);
  CHECK(plan.id_pairs.back().base    == 3);
  CHECK(plan.id_pairs.back().offset  == 3);
  CHECK(plan.dropped_base_ranges_lo.empty());
}


// -------------------------------------------------------------------
// N-Open-2 — open polyline with M > N: extra offset vertex between
// base[1] and base[2] must be assigned to one side without breaking
// the staircase (this is the M ≠ N case the algorithm targets).
// -------------------------------------------------------------------

TEST_CASE("planReverseMatchByNearest: open polyline — M > N pairing "
          "via reverse pass",
          "[offset_clipper2][bridge][nearest][open]") {
  const std::vector<SPoint2> base = {
      SPoint2(0.0, 0.0), SPoint2(1.0, 0.0),
      SPoint2(2.0, 0.0), SPoint2(3.0, 0.0),
  };
  // 5 offset vertices: one extra near x = 1.4 (slightly closer to
  // base[1] than to base[2], so the reverse pass should attribute it
  // to base[1]).
  OffsetPolygon prim = makeNearestPolygon(
      {SPoint2(0.0, 0.05), SPoint2(1.0, 0.05),
       SPoint2(1.4, 0.05), SPoint2(2.0, 0.05),
       SPoint2(3.0, 0.05)},
      /*closed*/ false,
      {{0, 0.0}, {0, 1.0}, {1, 0.4}, {1, 1.0}, {2, 1.0}});

  const auto plan = planReverseMatchByNearest(
      base, /*closed*/ false, /*side*/ +1, /*dist*/ 0.05, {prim});
  REQUIRE(plan.ok);
  CHECK(isValidStaircase(plan.id_pairs));

  // Endpoints pinned.
  CHECK(plan.id_pairs.front().base   == 0);
  CHECK(plan.id_pairs.front().offset == 0);
  CHECK(plan.id_pairs.back().base    == 3);
  CHECK(plan.id_pairs.back().offset  == 4);

  // Inspect the staircase: the extra offset (index 2) should pair with
  // base 1 — the (1, 1) and (1, 2) pair must coexist before stepping
  // to base 2.
  bool saw_b1_o1 = false;
  bool saw_b1_o2 = false;
  for (const auto& pr : plan.id_pairs) {
    if (pr.base == 1 && pr.offset == 1) saw_b1_o1 = true;
    if (pr.base == 1 && pr.offset == 2) saw_b1_o2 = true;
  }
  CHECK(saw_b1_o1);
  CHECK(saw_b1_o2);
}


// -------------------------------------------------------------------
// N-Sharp — MH104-TE-like fold. A closed shape where two base segments
// meet at a near-180° apex with a near-collinear back-fold. Under the
// legacy `attributeSource` + `attributeOne` midpoint round, a small
// perturbation at the apex can flip the assignment. The
// point-to-point nearest variant must produce a valid monotone
// staircase regardless.
// -------------------------------------------------------------------

TEST_CASE("planReverseMatchByNearest: sharp fold near apex — valid "
          "staircase",
          "[offset_clipper2][bridge][nearest][closed][sharp]") {
  // 6-vertex closed wedge: a long thin lozenge with the right tip
  // squeezed to a near-zero opening (MH104-TE caricature).
  //          (-1, 0.2)
  //                  *---* (0.5, 0.2)
  //                 /      \
  //   (-1, 0.0) *           * (1.0, 0.0)  <-- TE apex
  //                 \      /
  //                  *---* (0.5, -0.2)
  //          (-1, -0.2)
  const std::vector<SPoint2> base = {
      SPoint2(-1.0,  0.0),
      SPoint2(-1.0,  0.2),
      SPoint2( 0.5,  0.2),
      SPoint2( 1.0,  0.0),
      SPoint2( 0.5, -0.2),
      SPoint2(-1.0, -0.2),
  };

  // Drive through the real Clipper2 backend so the test exercises the
  // realistic vertex layout (raw join points, miter / bevel insertions).
  const double dist = 0.02;
  auto polys = offsetWithClipper2(base, /*closed*/ true, /*side*/ -1, dist);
  REQUIRE(!polys.empty());

  const auto plan = planReverseMatchByNearest(
      base, /*closed*/ true, /*side*/ -1, dist, polys);
  REQUIRE(plan.ok);
  CHECK(isValidStaircase(plan.id_pairs));

  // Front anchor pinned at base 0; closed wrap at the tail.
  CHECK(plan.id_pairs.front().base   == 0);
  CHECK(plan.id_pairs.front().offset == 0);
  CHECK(plan.id_pairs.back().base
        == static_cast<int>(base.size()));
  CHECK(plan.id_pairs.back().offset
        == static_cast<int>(plan.offset_points.size()));

  // Every distinct base index in [0, N-1] must appear at least once
  // in the staircase — even when it lands in a forward-fill / dropped
  // range, `buildIdPairs` emits an explicit pair for it.
  std::vector<int> seen(base.size(), 0);
  for (const auto& pr : plan.id_pairs) {
    if (pr.base >= 0 && pr.base < static_cast<int>(base.size())) {
      seen[pr.base] = 1;
    }
  }
  for (std::size_t i = 0; i < base.size(); ++i) {
    INFO("base index " << i << " missing from staircase");
    CHECK(seen[i] == 1);
  }
}


// -------------------------------------------------------------------
// N-Open-Sharp — MH104-TE open baseline: the TE in mh104_te_only.xml
// is two near-collinear segments meeting at a sharp pinch on the TE
// side. Drive an open polyline with this geometry through
// offsetWithClipper2 + planReverseMatchByNearest and assert the
// staircase holds end-to-end.
// -------------------------------------------------------------------

TEST_CASE("planReverseMatchByNearest: open MH104-TE-like polyline — "
          "valid staircase",
          "[offset_clipper2][bridge][nearest][open][sharp]") {
  // Sharp open V — apex at the origin, arms 1 unit long. The apex
  // angle is tight enough that the legacy midpoint round on
  // post-resample foot-distances is the failure mode we're avoiding.
  const std::vector<SPoint2> base = {
      SPoint2(-1.0,  0.02),
      SPoint2(-0.5,  0.01),
      SPoint2( 0.0,  0.0 ),  // apex
      SPoint2(-0.5, -0.01),
      SPoint2(-1.0, -0.02),
  };
  const double dist = 0.005;

  for (int side : {+1, -1}) {
    auto polys =
        offsetWithClipper2(base, /*closed*/ false, side, dist);
    REQUIRE(!polys.empty());

    const auto plan = planReverseMatchByNearest(
        base, /*closed*/ false, side, dist, polys);
    REQUIRE(plan.ok);
    CHECK(isValidStaircase(plan.id_pairs));

    // Open endpoints: (0, 0) at the head, (N-1, M-1) at the tail.
    CHECK(plan.id_pairs.front().base   == 0);
    CHECK(plan.id_pairs.front().offset == 0);
    CHECK(plan.id_pairs.back().base
          == static_cast<int>(base.size()) - 1);
    CHECK(plan.id_pairs.back().offset
          == static_cast<int>(plan.offset_points.size()) - 1);
  }
}


// -------------------------------------------------------------------
// N-Guards — empty / degenerate inputs return ok=false (no crash).
// -------------------------------------------------------------------

TEST_CASE("planReverseMatchByNearest: empty polygon list returns ok=false",
          "[offset_clipper2][bridge][nearest][guard]") {
  const std::vector<SPoint2> base = {
      SPoint2(0.0, 0.0), SPoint2(1.0, 0.0),
      SPoint2(1.0, 1.0), SPoint2(0.0, 1.0),
  };
  const auto plan = planReverseMatchByNearest(base, true, -1, 0.1, {});
  CHECK_FALSE(plan.ok);
}

TEST_CASE("planReverseMatchByNearest: zero dist returns ok=false",
          "[offset_clipper2][bridge][nearest][guard]") {
  const std::vector<SPoint2> base = {
      SPoint2(0.0, 0.0), SPoint2(1.0, 0.0),
      SPoint2(1.0, 1.0), SPoint2(0.0, 1.0),
  };
  OffsetPolygon prim = makeNearestPolygon({
      SPoint2(0.05, 0.05), SPoint2(0.95, 0.05),
      SPoint2(0.95, 0.95), SPoint2(0.05, 0.95),
  }, /*closed*/ true);
  // dist == 0 makes the validity gate `> 1.5 * dist` reject every
  // offset vertex; the function must early-out with ok=false rather
  // than divide-by-zero.
  const auto plan = planReverseMatchByNearest(base, true, -1, 0.0, {prim});
  CHECK_FALSE(plan.ok);
}

TEST_CASE("planReverseMatchByNearest: all offsets outside gate returns "
          "ok=false",
          "[offset_clipper2][bridge][nearest][guard]") {
  const std::vector<SPoint2> base = {
      SPoint2(0.0, 0.0), SPoint2(1.0, 0.0),
      SPoint2(1.0, 1.0), SPoint2(0.0, 1.0),
  };
  // Offset points placed far away (foot-distance >> 1.5 * dist).
  OffsetPolygon prim = makeNearestPolygon({
      SPoint2(100.0, 100.0), SPoint2(101.0, 100.0),
      SPoint2(101.0, 101.0), SPoint2(100.0, 101.0),
  }, /*closed*/ true);
  const auto plan = planReverseMatchByNearest(base, true, -1, 0.1, {prim});
  CHECK_FALSE(plan.ok);
}


// ===================================================================
// rebuildBaseOffsetMapFromGeometry — plan-20260522 Phase 1.
//
// Scope: a thin wrapper that synthesizes a single OffsetPolygon from
// an externally supplied base/offset polyline pair (computing per-
// vertex sources via foot-of-perpendicular onto the base) and runs
// planReverseMatchByNearest on it. The contract: when fed the same
// (base, offset.points) the function must produce a ReverseMatchPlan
// with id_pairs identical to planReverseMatchByNearest's. When fed a
// truncated base (simulating a join-trim) it must produce a staircase
// that drops the leading pairs cleanly.
// ===================================================================

namespace {

bool idPairsEqual(const BaseOffsetMap& a, const BaseOffsetMap& b) {
  if (a.size() != b.size()) return false;
  for (std::size_t i = 0; i < a.size(); ++i) {
    if (a[i].base != b[i].base || a[i].offset != b[i].offset) return false;
  }
  return true;
}

}  // namespace


// -------------------------------------------------------------------
// Parity (closed square) — rebuilt staircase matches direct call.
// -------------------------------------------------------------------

TEST_CASE("rebuildBaseOffsetMapFromGeometry: closed square parity with "
          "planReverseMatchByNearest",
          "[offset_clipper2][bridge][derive][closed]") {
  const std::vector<SPoint2> base = {
      SPoint2(0.0, 0.0), SPoint2(1.0, 0.0),
      SPoint2(1.0, 1.0), SPoint2(0.0, 1.0),
  };
  const double dist = 0.1;

  auto polys = offsetWithClipper2(base, /*closed*/ true, /*side*/ -1, dist);
  REQUIRE(polys.size() == 1);

  const auto baseline =
      planReverseMatchByNearest(base, true, -1, dist, polys);
  REQUIRE(baseline.ok);

  // Feed the same offset polyline through the geometry-derived path.
  const auto rebuilt = rebuildBaseOffsetMapFromGeometry(
      base, polys[0].points, /*closed*/ true, /*side*/ -1, dist);
  REQUIRE(rebuilt.ok);

  CHECK(idPairsEqual(rebuilt.id_pairs, baseline.id_pairs));
  CHECK(rebuilt.closed == baseline.closed);
  CHECK(rebuilt.dropped_base_ranges_lo == baseline.dropped_base_ranges_lo);
  CHECK(rebuilt.dropped_base_ranges_hi == baseline.dropped_base_ranges_hi);
  CHECK(rebuilt.offset_points.size() == baseline.offset_points.size());
}


// -------------------------------------------------------------------
// Parity (pseudo-airfoil, small dist) — clean staircase reproduced.
// -------------------------------------------------------------------

TEST_CASE("rebuildBaseOffsetMapFromGeometry: pseudo-airfoil small dist "
          "parity with planReverseMatchByNearest",
          "[offset_clipper2][bridge][derive][closed][airfoil]") {
  const auto base = pseudoAirfoilCCW();
  const double dist = 0.005;

  auto polys = offsetWithClipper2(base, /*closed*/ true, -1, dist);
  REQUIRE(polys.size() == 1);

  const auto baseline =
      planReverseMatchByNearest(base, true, -1, dist, polys);
  REQUIRE(baseline.ok);

  const auto rebuilt = rebuildBaseOffsetMapFromGeometry(
      base, polys[0].points, true, -1, dist);
  REQUIRE(rebuilt.ok);

  CHECK(idPairsEqual(rebuilt.id_pairs, baseline.id_pairs));
  CHECK(rebuilt.dropped_base_ranges_lo.empty());
  CHECK(rebuilt.dropped_base_ranges_hi.empty());
}


// -------------------------------------------------------------------
// Parity (pseudo-airfoil, thick dist) — dropped ranges reproduced.
// -------------------------------------------------------------------

TEST_CASE("rebuildBaseOffsetMapFromGeometry: pseudo-airfoil thick dist "
          "reproduces dropped ranges",
          "[offset_clipper2][bridge][derive][closed][airfoil][thin]") {
  const auto base = pseudoAirfoilCCW();
  const double dist = 0.04;

  auto polys = offsetWithClipper2(base, /*closed*/ true, -1, dist);
  REQUIRE(polys.size() >= 1);

  const auto baseline =
      planReverseMatchByNearest(base, true, -1, dist, polys);
  REQUIRE(baseline.ok);

  const auto rebuilt = rebuildBaseOffsetMapFromGeometry(
      base, polys[0].points, true, -1, dist);
  REQUIRE(rebuilt.ok);

  CHECK(idPairsEqual(rebuilt.id_pairs, baseline.id_pairs));
  CHECK(rebuilt.dropped_base_ranges_lo == baseline.dropped_base_ranges_lo);
  CHECK(rebuilt.dropped_base_ranges_hi == baseline.dropped_base_ranges_hi);
  // Sanity: thick-dist should actually have produced a non-empty drop.
  CHECK_FALSE(baseline.dropped_base_ranges_lo.empty());
}


// -------------------------------------------------------------------
// Parity (open L-shape) — both sides agree with direct call.
// -------------------------------------------------------------------

TEST_CASE("rebuildBaseOffsetMapFromGeometry: open L-shape parity",
          "[offset_clipper2][bridge][derive][open]") {
  const std::vector<SPoint2> base = {
      SPoint2(0.0, 0.0), SPoint2(1.0, 0.0), SPoint2(1.0, 1.0),
  };
  const double dist = 0.1;

  for (int side : {+1, -1}) {
    auto polys = offsetWithClipper2(base, /*closed*/ false, side, dist);
    REQUIRE(polys.size() >= 1);

    const auto baseline =
        planReverseMatchByNearest(base, false, side, dist, polys);
    REQUIRE(baseline.ok);

    // Pick whichever polygon planReverseMatchByNearest would have
    // chosen — for the open branch this is the widest-base_seg-span
    // run. The geometry-derived path synthesizes a single polygon
    // from the polyline we hand it, so we must hand it the same one.
    // The simplest faithful test: re-derive from the baseline's own
    // offset_points (which already are the primary's vertex sequence).
    const auto rebuilt = rebuildBaseOffsetMapFromGeometry(
        base, baseline.offset_points, false, side, dist);
    REQUIRE(rebuilt.ok);

    CHECK(idPairsEqual(rebuilt.id_pairs, baseline.id_pairs));
    CHECK(rebuilt.dropped_base_ranges_lo == baseline.dropped_base_ranges_lo);
    CHECK(rebuilt.dropped_base_ranges_hi == baseline.dropped_base_ranges_hi);
  }
}


// -------------------------------------------------------------------
// Closed-input trailing-duplicate defense — passing a polyline with
// front == back must not double-count the closing vertex.
// -------------------------------------------------------------------

TEST_CASE("rebuildBaseOffsetMapFromGeometry: closed input with trailing "
          "duplicate drops dup defensively",
          "[offset_clipper2][bridge][derive][closed][guard]") {
  const std::vector<SPoint2> base = {
      SPoint2(0.0, 0.0), SPoint2(1.0, 0.0),
      SPoint2(1.0, 1.0), SPoint2(0.0, 1.0),
  };
  const double dist = 0.1;

  auto polys = offsetWithClipper2(base, true, -1, dist);
  REQUIRE(polys.size() == 1);

  // Without trailing dup.
  const auto rebuilt_no_dup = rebuildBaseOffsetMapFromGeometry(
      base, polys[0].points, true, -1, dist);
  REQUIRE(rebuilt_no_dup.ok);

  // With trailing dup appended — caller convention for some PreVABS
  // baselines.
  auto off_with_dup = polys[0].points;
  off_with_dup.push_back(polys[0].points.front());
  const auto rebuilt_with_dup = rebuildBaseOffsetMapFromGeometry(
      base, off_with_dup, true, -1, dist);
  REQUIRE(rebuilt_with_dup.ok);

  CHECK(idPairsEqual(rebuilt_with_dup.id_pairs, rebuilt_no_dup.id_pairs));
  CHECK(rebuilt_with_dup.offset_points.size()
        == rebuilt_no_dup.offset_points.size());
}


// -------------------------------------------------------------------
// Trim invariants (open) — after dropping leading base + offset
// vertices, the rebuilt staircase must still satisfy the geometric
// contract (anchored at (0, 0), monotone, reaches the trimmed
// endpoints).
//
// This is the central property motivating Phase 1: instead of
// splicing the persisted BaseOffsetMap when join trims the base,
// we discard the map and re-derive it from the post-trim geometry.
// Strict pair-by-pair equality with a manually spliced original is
// NOT guaranteed (the per-pair offset assignment can differ where
// nearest-pairing chooses a different seed under the trimmed seed
// search), but the staircase must be valid and cover the trimmed
// geometry end-to-end.
// -------------------------------------------------------------------

TEST_CASE("rebuildBaseOffsetMapFromGeometry: open L-shape head-trim "
          "produces valid staircase covering trimmed geometry",
          "[offset_clipper2][bridge][derive][open][trim]") {
  // 5-vertex open polyline so we have enough room to head-trim.
  const std::vector<SPoint2> base_full = {
      SPoint2(0.0, 0.0),
      SPoint2(1.0, 0.0),
      SPoint2(2.0, 0.0),
      SPoint2(2.0, 1.0),
      SPoint2(2.0, 2.0),
  };
  const double dist = 0.1;

  auto polys = offsetWithClipper2(base_full, false, +1, dist);
  REQUIRE(polys.size() >= 1);

  const auto baseline =
      planReverseMatchByNearest(base_full, false, +1, dist, polys);
  REQUIRE(baseline.ok);

  // Identify how many leading offset points were associated with the
  // first n_trim_base base vertices in the original baseline.
  const int n_trim_base = 2;
  const int n_trim_offset = [&] {
    int o = 0;
    for (const auto& p : baseline.id_pairs) {
      if (p.base < n_trim_base) o = std::max(o, p.offset + 1);
    }
    return o;
  }();
  REQUIRE(n_trim_offset > 0);

  // Trim the geometry — simulates a join trim that ate the head.
  std::vector<SPoint2> base_trim(base_full.begin() + n_trim_base,
                                 base_full.end());
  std::vector<SPoint2> off_trim(baseline.offset_points.begin()
                                + n_trim_offset,
                                baseline.offset_points.end());
  REQUIRE(base_trim.size() >= 2);
  REQUIRE(off_trim.size()  >= 2);

  const auto rebuilt = rebuildBaseOffsetMapFromGeometry(
      base_trim, off_trim, false, +1, dist);
  REQUIRE(rebuilt.ok);

  // (1) Anchored at (0, 0).
  CHECK(rebuilt.id_pairs.front().base   == 0);
  CHECK(rebuilt.id_pairs.front().offset == 0);
  // (2) Reaches the trimmed endpoints.
  CHECK(rebuilt.id_pairs.back().base
        == static_cast<int>(base_trim.size()) - 1);
  CHECK(rebuilt.id_pairs.back().offset
        == static_cast<int>(off_trim.size()) - 1);
  // (3) Satisfies the staircase invariant — no editor splice needed.
  CHECK(isValidStaircase(rebuilt.id_pairs));
}


// -------------------------------------------------------------------
// Guards — short / degenerate inputs return ok=false (no crash).
// -------------------------------------------------------------------

TEST_CASE("rebuildBaseOffsetMapFromGeometry: short inputs return ok=false",
          "[offset_clipper2][bridge][derive][guard]") {
  const std::vector<SPoint2> b1 = {SPoint2(0.0, 0.0)};
  const std::vector<SPoint2> off = {SPoint2(0.0, 0.1), SPoint2(1.0, 0.1)};
  CHECK_FALSE(rebuildBaseOffsetMapFromGeometry(b1, off, false, +1, 0.1).ok);

  const std::vector<SPoint2> base = {
      SPoint2(0.0, 0.0), SPoint2(1.0, 0.0), SPoint2(2.0, 0.0),
  };
  const std::vector<SPoint2> o1 = {SPoint2(0.0, 0.1)};
  CHECK_FALSE(rebuildBaseOffsetMapFromGeometry(base, o1, false, +1, 0.1).ok);

  CHECK_FALSE(rebuildBaseOffsetMapFromGeometry(base, off, false, +1, 0.0).ok);
}
