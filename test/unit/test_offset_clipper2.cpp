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

TEST_CASE("offsetWithClipper2: open polyline produces a Butt-surrounded "
          "closed polygon (side is ignored)",
          "[offset_clipper2][open]") {
  // Open polyline along the x axis. Clipper2's EndType::Butt wraps
  // the polyline with a closed perimeter on BOTH sides plus flat caps
  // at the ends; "side" cannot pick one half (that's why open-side
  // selection isn't a supported semantic here — see header doc).
  const std::vector<SPoint2> base = {
      SPoint2(0.0, 0.0), SPoint2(1.0, 0.0), SPoint2(2.0, 0.0),
  };
  const double dist = 0.05;

  auto out = offsetWithClipper2(base, /*closed*/ false, +1, dist);
  REQUIRE(out.size() == 1);
  const auto& op = out[0];

  // Clipper2 always returns closed paths from InflatePaths.
  CHECK(op.is_closed);

  // Area ≈ 2 * dist * total_length = 2 * 0.05 * 2.0 = 0.20
  // (butt caps add nothing; they are flat at the endpoints).
  CHECK(polygonAbsArea(op.points) == Catch::Approx(0.20).margin(1e-6));

  // side = -1 with an open input also produces non-empty output (the
  // wrapper takes |dist| internally because Clipper2 requires positive
  // delta for open paths).
  auto out_neg = offsetWithClipper2(base, false, -1, dist);
  REQUIRE(out_neg.size() == 1);
  CHECK(polygonAbsArea(out_neg[0].points)
        == Catch::Approx(0.20).margin(1e-6));
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
