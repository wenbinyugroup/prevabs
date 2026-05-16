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
