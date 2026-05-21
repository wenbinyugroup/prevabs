#include "geo.hpp"

#include "PDCELHalfEdge.hpp"
#include "PDCELHalfEdgeLoop.hpp"
#include "PDCELVertex.hpp"
#include "PGeoClasses.hpp"
#include "globalConstants.hpp"
#include "globalVariables.hpp"
#include "PBaseLine.hpp"
#include "overloadOperator.hpp"
#include "utilities.hpp"
#include "plog.hpp"
#include "pui.hpp"

#include "offset_clipper2.hpp"

#include "geo_types.hpp"

#include <cassert>
#include <cmath>
#include <sstream>
#include <string>
#include <vector>

bool validateBaseOffsetMap(
    const BaseOffsetMap &map, std::string *error_message) {
  for (std::size_t i = 0; i < map.size(); ++i) {
    if (map[i].base < 0 || map[i].offset < 0) {
      if (error_message != nullptr) {
        *error_message =
            "negative index at pair " + std::to_string(i)
            + " [" + std::to_string(map[i].base)
            + ", " + std::to_string(map[i].offset) + "]";
      }
      return false;
    }

    if (i == 0) {
      continue;
    }

    const int base_delta = map[i].base - map[i - 1].base;
    const int offset_delta = map[i].offset - map[i - 1].offset;
    if (base_delta < 0 || offset_delta < 0
        || base_delta > 1 || offset_delta > 1) {
      if (error_message != nullptr) {
        *error_message =
            "staircase violation between pairs "
            + std::to_string(i - 1) + " and " + std::to_string(i)
            + " [" + std::to_string(map[i - 1].base)
            + ", " + std::to_string(map[i - 1].offset) + "] -> ["
            + std::to_string(map[i].base)
            + ", " + std::to_string(map[i].offset) + "]";
      }
      return false;
    }
  }

  return true;
}

namespace {

void assertValidBaseOffsetMap(
    const BaseOffsetMap &map, const std::string &caller) {
  std::string error_message;
  const bool valid = validateBaseOffsetMap(map, &error_message);
  if (!valid) {
    PLOG(error) << caller + ": invalid BaseOffsetMap: " + error_message;
  }
  assert(valid && "BaseOffsetMap staircase invariant violated");
}

// Local half-thickness at base[i] for a closed polyline: project a ray
// from base[i] along the inward normal (determined by `side`, matching
// the single-vertex offset() cross-product convention), find the nearest
// non-adjacent base segment it hits, and return that distance. Returns
// +INF when the inward ray never re-enters the polygon (the offset has
// plenty of room locally).
//
// Stage E of plan-20260514-clipper2-offset-backend.md §7 — runs before
// the Clipper2 backend kicks in on the closed branch and lets us
// fail-fast with a readable error when the local geometry leaves
// Clipper2 nothing to inflate.
//
// Pre: `base` is a closed polyline (caller checks
// base.front() == base.back()) with N >= 4 vertices (so the distinct
// vertex count N_distinct = N - 1 is >= 3 — required for a non-trivial
// polygon).
double signedHalfThickness(
    const std::vector<PDCELVertex *> &base, int i, int side,
    bool base_is_closed) {
  // Distinct base vertex count:
  //   closed → drop trailing duplicate (front == back pointer).
  //   open   → all entries are distinct.
  const int n_distinct = base_is_closed
                           ? static_cast<int>(base.size()) - 1
                           : static_cast<int>(base.size());
  if (n_distinct < 3) return INF;

  // Endpoint vertices on an open base have only one neighbour, so a
  // half-thickness measurement is geometrically undefined (no opposite
  // side to project onto). Stage E callers should iterate interior
  // vertices only; this guard is defensive.
  if (!base_is_closed && (i <= 0 || i >= n_distinct - 1)) return INF;

  const int i_prev = base_is_closed
                       ? (i - 1 + n_distinct) % n_distinct
                       : i - 1;
  const int i_next = base_is_closed
                       ? (i + 1) % n_distinct
                       : i + 1;

  const SPoint2 p_i    = base[i]     ->point2();
  const SPoint2 p_prev = base[i_prev]->point2();
  const SPoint2 p_next = base[i_next]->point2();

  // Local tangent: average of incoming + outgoing edge tangents at
  // base[i]. Using (p_next - p_prev) gives this average direction in
  // one subtraction and is robust at near-cusp vertices.
  const double tx = p_next.x() - p_prev.x();
  const double ty = p_next.y() - p_prev.y();
  const double t_norm = std::sqrt(tx * tx + ty * ty);
  if (t_norm < TOLERANCE) return INF;
  const double tx_n = tx / t_norm;
  const double ty_n = ty / t_norm;

  // Inward normal in the yz plane. In 3D the single-segment offset()
  // direction is n × t with n = SVector3(side, 0, 0), giving 2-D yz
  // components (-side * tz, side * ty). In SPoint2 coordinates (x=y, y=z):
  //   normal_x = -side * (ty_n in yz, which is the SPoint2 y-component)
  //   normal_y =  side * (the SPoint2 x-component)
  const double dir_x = -side * ty_n;
  const double dir_y =  side * tx_n;

  // Intersect ray (p_i + t * dir) with each non-adjacent segment.
  // Closed: there are n_distinct segments (last wraps back to base[0]).
  // Open:   there are n_distinct - 1 segments (no wrap).
  const int n_seg = base_is_closed ? n_distinct : n_distinct - 1;
  double min_t = INF;
  for (int j = 0; j < n_seg; ++j) {
    // Skip segments that touch base[i].
    //   seg j      = base[j]   → base[j+1]
    //   "j == i"   touches at start
    //   "j == i_prev" touches at end
    if (j == i || j == i_prev) continue;

    const int j_next = base_is_closed ? (j + 1) % n_distinct : j + 1;
    const SPoint2 a = base[j]     ->point2();
    const SPoint2 b = base[j_next]->point2();

    const double ex = b.x() - a.x();
    const double ey = b.y() - a.y();

    // Solve  p_i + t * dir = a + s * (b - a)
    //  →  [ dir_x, -ex ] [ t ]   [ a.x - p_i.x ]
    //     [ dir_y, -ey ] [ s ] = [ a.y - p_i.y ]
    const double det = -dir_x * ey + dir_y * ex;
    if (std::fabs(det) < TOLERANCE) continue;  // parallel / collinear

    const double rx = a.x() - p_i.x();
    const double ry = a.y() - p_i.y();
    const double t = (-rx * ey + ry * ex) / det;
    const double s = (-dir_x * ry + dir_y * rx) / -det;
    if (t > TOLERANCE && s >= -TOLERANCE && s <= 1.0 + TOLERANCE) {
      if (t < min_t) min_t = t;
    }
  }
  return min_t;
}

} // namespace

/**
 * @brief Offsets a line segment by a given direction and distance.
 *
 * This function takes two points representing a line segment and offsets them
 * by a specified direction vector and distance. The resulting offset points
 * are stored in the provided output parameters.
 *
 * @param p1 The starting point of the line segment.
 * @param p2 The ending point of the line segment.
 * @param dr The direction vector for the offset. This vector will be normalized.
 * @param ds The distance by which to offset the line segment.
 * @param q1 The resulting starting point of the offset line segment.
 * @param q2 The resulting ending point of the offset line segment.
 */
void offsetLineSegment(SPoint3 &p1, SPoint3 &p2, SVector3 &dr, double &ds,
                       SPoint3 &q1, SPoint3 &q2) {
  SVector3 dr_norm = dr;  // work on a local copy — do not mutate the caller's vector
  dr_norm.normalize();
  q1 = (SVector3(p1) + dr_norm * ds).point();
  q2 = (SVector3(p2) + dr_norm * ds).point();
}

/**
 * @brief Offsets a line segment by a given distance on a specified side.
 *
 * This function takes a line segment and offsets it by a specified distance
 * on either the left or right side. The side is determined by the `side` parameter,
 * where a positive value indicates the left side and a negative value indicates the right side.
 *
 * @param ls Pointer to the line segment to be offset.
 * @param side Integer indicating the side to offset the line segment. Positive for left, negative for right.
 * @param d The distance by which to offset the line segment.
 * @return Pointer to the new offset line segment.
 */
PGeoLineSegment *offsetLineSegment(PGeoLineSegment *ls, int side, double d) {
  SVector3 t, n, p;
  n = SVector3(side, 0, 0);
  t = ls->toVector();
  p = crossprod(n, t).unit() * d;

  return offsetLineSegment(ls, p);
}

/**
 * @brief Offsets a given line segment by a specified vector.
 *
 * This function creates a new line segment by offsetting the endpoints of the
 * input line segment by the given offset vector. The new line segment is
 * constructed using new vertices that are the result of adding the offset
 * vector to the original vertices of the input line segment.
 *
 * @param ls Pointer to the original line segment to be offset.
 * @param offset Reference to the vector by which to offset the line segment.
 * @return Pointer to the newly created offset line segment.
 */
PGeoLineSegment *offsetLineSegment(PGeoLineSegment *ls, SVector3 &offset) {
  PDCELVertex *v1, *v2;
  v1 = new PDCELVertex(ls->v1()->point() + offset.point());
  v2 = new PDCELVertex(ls->v2()->point() + offset.point());

  return new PGeoLineSegment(v1, v2);
}

/**
 * @brief Offsets a given baseline curve by a specified distance on a specified side.
 *
 * This function takes a baseline curve and generates a new curve that is offset
 * from the original by a given distance on the specified side. The offset is
 * calculated by creating line segments between the vertices of the original curve,
 * offsetting these segments, and then constructing the new curve from the offset
 * segments.
 *
 * @param curve The original baseline curve to be offset.
 * @param side The side on which to offset the curve (e.g., left or right).
 * @param distance The distance by which to offset the curve.
 * @return A pointer to the new offset baseline curve.
 */
Baseline *offsetCurve(Baseline *curve, int side, double distance) {
  Baseline *curve_off = new Baseline();

  PGeoLineSegment *ls;
  if (curve->vertices().size() == 2) {
    PGeoLineSegment *ls_raw = new PGeoLineSegment(curve->vertices()[0], curve->vertices()[1]);
    ls = offsetLineSegment(ls_raw, side, distance);
    delete ls_raw;

    curve_off->addPVertex(ls->v1());
    curve_off->addPVertex(ls->v2());
  } else {
    PGeoLineSegment *ls_prev = nullptr, *ls_first_off = nullptr;
    for (int i = 0; i < static_cast<int>(curve->vertices().size()) - 1; ++i) {
      PGeoLineSegment *ls_raw = new PGeoLineSegment(curve->vertices()[i], curve->vertices()[i + 1]);
      ls = offsetLineSegment(ls_raw, side, distance);
      delete ls_raw;

      if (i == 0) {
        ls_first_off = ls;
        curve_off->addPVertex(ls->v1());
      }

      if (i > 0) {
        // Calculate intersection
        double u1, u2;
        bool not_parallel;
        not_parallel = calcLineIntersection2D(ls, ls_prev, u1, u2, TOLERANCE);
        if (not_parallel) {
          curve_off->addPVertex(ls->getParametricVertex(u1));
        } else {
          curve_off->addPVertex(ls->v1());
        }
      }

      if (i == static_cast<int>(curve->vertices().size()) - 2) {
        curve_off->addPVertex(ls->v2());
      }

      ls_prev = ls;
    }

    if (curve->vertices().front() == curve->vertices().back()) {
      double u1, u2;
      bool not_parallel;
      not_parallel = calcLineIntersection2D(ls_first_off, ls_prev, u1, u2, TOLERANCE);
      if (not_parallel) {
        curve_off->vertices()[0] = ls_first_off->getParametricVertex(u1);
      }
      curve_off->vertices()[curve_off->vertices().size() - 1] =
          curve_off->vertices()[0];
    }
  }

  return curve_off;
}

/**
 * @brief Offsets a line segment defined by two vertices by a specified distance.
 *
 * This function calculates a new position for the vertices of a line segment
 * by offsetting them perpendicular to the segment direction by a given distance.
 *
 * @param v1_base Pointer to the first vertex of the original line segment.
 * @param v2_base Pointer to the second vertex of the original line segment.
 * @param side Integer indicating the side of the offset (positive or negative).
 * @param dist Distance by which to offset the line segment.
 * @param v1_off Pointer to the first vertex of the offset line segment (output).
 * @param v2_off Pointer to the second vertex of the offset line segment (output).
 * @return Integer indicating the success of the operation (always returns 1).
 */
int offset(PDCELVertex *v1_base, PDCELVertex *v2_base, int side, double dist,
           PDCELVertex *v1_off, PDCELVertex *v2_off) {

  if (!v1_off || !v2_off) {
    PLOG(error) << "offset: null output vertex pointer";
    return 0;
  }

  SVector3 dir, n, t;
  n = SVector3(side, 0, 0);
  t = SVector3(v1_base->point(), v2_base->point());

  dir = crossprod(n, t).unit() * dist;

  SPoint3 p1, p2;
  p1 = v1_base->point() + dir.point();
  p2 = v2_base->point() + dir.point();

  v1_off->setPoint(p1);
  v2_off->setPoint(p2);

  return 1;
}

// ---------------------------------------------------------------------------
// Public multi-vertex offset() function
// ---------------------------------------------------------------------------

/**
 * @brief Offset a set of base vertices by a given distance.
 *
 * Dispatches into the Clipper2 backend (plan-20260514 / plan-20260518):
 *   1. Fast path for 2-vertex baselines (single-segment translate).
 *   2. Stage E half-thickness precheck (warnings only).
 *   3. `offsetWithClipper2` geometric core — closed inputs go to
 *      EndType::Polygon, open inputs use EndType::Butt + single-side
 *      filter.
 *   4. `buildBaseOffsetMapFromOffsetPolygons` reverse-match bridge,
 *      producing PDCELVertex* + a staircase BaseOffsetMap.
 *
 * @param base             Input base vertices (≥ 2). For closed inputs
 *                         the PreVABS convention `front() == back()`
 *                         (same pointer) is honoured.
 * @param side             +1 / -1 — PreVABS legacy n × t convention.
 *                         For a CCW closed curve, +1 is inward.
 * @param dist             Offset distance magnitude.
 * @param offset_vertices  Output: the offset vertex sequence. Cleared.
 *                         Closed → front == back; open → distinct ends.
 * @param id_pairs         Output: BaseOffsetMap staircase. Cleared.
 * @param offset_resampled Optional output (may be null): per-vertex
 *                         origin tag parallel to `offset_vertices`.
 *                         `false` = raw Clipper2 corner / cap;
 *                         `true`  = synthesized at a base-vertex
 *                         perpendicular foot by the open-path
 *                         resample step. Closed inputs and the
 *                         2-vertex fast path always report `false`.
 * @param pre_resample_raw_points
 *                         Optional debug-only output (may be null):
 *                         snapshot of the Clipper2 raw run polyline
 *                         taken immediately before the open-path
 *                         resample step replaced every vertex. Empty
 *                         on closed inputs, the 2-vertex fast path,
 *                         and on open runs that did not need resample.
 *                         Consumed by `dumpBaseOffsetMapSvg` as a
 *                         topmost scatter overlay.
 * @return int             1 on success, 0 on degenerate input or
 *                         Clipper2 backend failure.
 */
int offset(const std::vector<PDCELVertex *> &base, int side, double dist,
           std::vector<PDCELVertex *> &offset_vertices,
           BaseOffsetMap &id_pairs,
           std::vector<bool> *offset_resampled,
           std::vector<SPoint2> *pre_resample_raw_points) {


  std::size_t size = base.size();

  // Precondition: at least 2 vertices are required to form a segment.
  if (size < 2) {
    PLOG(error) << "offset (multi-vertex): base must have at least 2 vertices (got "
      + std::to_string(size) + "); returning empty result";
    return 0;
  }

  // Clear output vectors so callers can safely pass non-empty containers.
  offset_vertices.clear();
  id_pairs.clear();
  if (offset_resampled) offset_resampled->clear();
  if (pre_resample_raw_points) pre_resample_raw_points->clear();

  if (config.debug_level >= DebugLevel::geo) {
        PLOG(debug) <<
      "offset (multi-vertex): size=" + std::to_string(size)
      + " side=" + std::to_string(side)
      + " dist=" + std::to_string(dist);
  }

  // -------------------------------------------------------------------------
  // Geometric robustness check: warn when the offset distance is large
  // relative to the shortest base segment.  Beyond ratio ~0.5 the offset
  // construction becomes numerically fragile (junction overshoot, near-
  // tangent self-intersections at sharp closures, etc.).  The fix in
  // src/geo/offset.cpp's step 4 lets us survive ratios well above this
  // threshold in practice, so the warning is informational only — it gives
  // users a readable trail to follow if a downstream stage fails on an
  // input that sits close to the failure regime, before the stack trace
  // has decayed into "DCEL half-edge cycle discarded" or a Gmsh meshing
  // error.
  // -------------------------------------------------------------------------
  {
    double L_min     = base[0]->point().distance(base[1]->point());
    int    L_min_seg = 0;
    for (std::size_t i = 1; i + 1 < size; ++i) {
      const double L = base[i]->point().distance(base[i + 1]->point());
      if (L < L_min) {
        L_min     = L;
        L_min_seg = static_cast<int>(i);
      }
    }
    const double abs_dist = std::fabs(dist);
    if (L_min > 0.0 && abs_dist > 0.5 * L_min) {
      std::ostringstream oss;
      oss.precision(6);
      oss << "offset (multi-vertex): offset distance " << abs_dist
          << " is " << (abs_dist / L_min) << "x the shortest base segment "
          << "length (" << L_min
          << " at segment " << L_min_seg
          << "); offset construction may be numerically fragile — "
          << "consider <normalize>1</normalize>, a larger <scale>, "
          << "or a denser baseline";
      PLOG(warning) << oss.str();
    }
  }

  // -------------------------------------------------------------------------
  // Fast path: 2-vertex polyline — a single segment, no junctions needed.
  // -------------------------------------------------------------------------
  if (size == 2) {
    PDCELVertex *seg_start = new PDCELVertex();
    PDCELVertex *seg_end   = new PDCELVertex();

    offset(base[0], base[1], side, dist, seg_start, seg_end);

    offset_vertices.push_back(seg_start);
    offset_vertices.push_back(seg_end);

    id_pairs.push_back(BaseOffsetPair(0, 0));
    id_pairs.push_back(BaseOffsetPair(1, 1));
    assertValidBaseOffsetMap(id_pairs, "offset fast path");

    if (offset_resampled) {
      offset_resampled->assign(2, false);
    }
    return 1;
  }

  // -------------------------------------------------------------------------
  // Multi-vertex path (closed OR open): Clipper2 backend.
  //
  // PreVABS legacy convention (single-vertex offset() above uses n × t
  // with n = SVector3(side, 0, 0)): for a CCW closed curve side = +1
  // is the inward direction. Clipper2's InflatePaths takes positive
  // delta = outward. For open inputs the same `clipper_side = -side`
  // flip puts side = +1 on the left of the base walking direction
  // (consistent with the single-segment `offsetLineSegment` n × t
  // direction).
  // -------------------------------------------------------------------------
  const bool base_is_closed = (base.front() == base.back());

  // ---------------------------------------------------------------------
  // Stage E precheck: local half-thickness scan (closed + open).
  //
  // For each interior base vertex i, project an inward-normal ray and
  // measure the distance to the nearest non-adjacent base segment (h_i).
  // Categories:
  //   h_i < |dist|     — the inward offset crosses to the far side
  //                      locally. Stage C will record this region in
  //                      `dropped_base_ranges` and downstream skin is
  //                      locally absent.
  //   h_i < 2 * |dist| — the post-offset thickness is below the
  //                      "two-sided" headroom; Clipper2 still
  //                      succeeds.
  //
  // Behaviour: count + summarise as PLOG(warning). We do NOT fail-fast
  // — Clipper2's InflatePaths handles all these geometrically (Stage
  // C bridge's dropped-range mechanism produces the user-actionable
  // diagnostic when skin is actually eaten). The plan-20260514 §7
  // fail-fast was tuned for a moderately thick uniform skin layup;
  // sharp TE cusps in real airfoils give h_i ≪ |dist| at the cusp
  // vertex but the geometry is still meshable.
  //
  // Open inputs skip the two endpoint vertices (half-thickness is
  // geometrically undefined there — `signedHalfThickness` returns INF
  // for them when base_is_closed=false).
  //
  // O(N²); fine for the N < 200 typical PreVABS payload.
  // ---------------------------------------------------------------------
  {
    const int n_distinct = base_is_closed
                             ? static_cast<int>(size) - 1
                             : static_cast<int>(size);
    const double abs_dist   = std::fabs(dist);
    const char* const kind  = base_is_closed ? "closed" : "open";
    int n_below_dist   = 0;
    int n_below_2dist  = 0;
    int first_below    = -1;
    for (int i = 0; i < n_distinct; ++i) {
      const double h = signedHalfThickness(base, i, side, base_is_closed);
      if (h < abs_dist - TOLERANCE) {
        ++n_below_dist;
        if (first_below < 0) first_below = i;
      } else if (h < 2.0 * abs_dist - TOLERANCE) {
        ++n_below_2dist;
      }
    }
    if (n_below_dist > 0) {
      PLOG(warning)
          << "offset (multi-vertex, " << kind << "): " << n_below_dist
          << " base vertex/vertices have local half-thickness < |dist| = "
          << abs_dist << " (first at base[" << first_below << "] = "
          << base[first_below]->printString()
          << "); skin will be locally dropped at those locations";
    }
    if (n_below_2dist > 0) {
      PLOG(warning)
          << "offset (multi-vertex, " << kind << "): " << n_below_2dist
          << " base vertex/vertices have local half-thickness "
          << "in [|dist|, 2*|dist|) = [" << abs_dist << ", "
          << (2.0 * abs_dist)
          << "); offset will be valid but locally thin";
    }
  }

  // Build SPoint2 view of base + invoke Clipper2 backend.
  std::vector<SPoint2> base_pts;
  base_pts.reserve(size);
  for (auto *v : base) {
    base_pts.emplace_back(v->point2()[0], v->point2()[1]);
  }
  // Closed CCW: PreVABS side = +1 (inward) ⇒ Clipper2 δ < 0 (shrink) ⇒ flip.
  // Open polyline: PreVABS side = +1 (left, sign(t × d) > 0) already matches
  //                Phase 1's filter convention ⇒ pass through.
  const int clipper_side = base_is_closed ? -side : side;
  auto polygons = prevabs::geo::offsetWithClipper2(
      base_pts, base_is_closed, clipper_side, std::fabs(dist));

  if (polygons.empty()) {
    PLOG(error)
        << "offset (multi-vertex, "
        << (base_is_closed ? "closed" : "open")
        << "): Clipper2 returned no offset polygon (local thickness "
           "< |dist| somewhere along the path?); returning empty result";
    return 0;
  }

  const auto result =
      prevabs::geo::buildBaseOffsetMapFromOffsetPolygons(
          base, base_is_closed, clipper_side, std::fabs(dist), polygons);

  if (!result.ok) {
    PLOG(error)
        << "offset (multi-vertex, "
        << (base_is_closed ? "closed" : "open")
        << "): reverse-match bridge failed to build a valid "
           "BaseOffsetMap; returning empty result";
    return 0;
  }

  offset_vertices = result.offset_vertices;
  id_pairs        = result.id_pairs;
  if (offset_resampled) *offset_resampled = result.offset_resampled;
  if (pre_resample_raw_points)
    *pre_resample_raw_points = result.pre_resample_raw_points;

  for (std::size_t k = 0; k < result.dropped_base_ranges_lo.size(); ++k) {
    PLOG(warning) << "offset (multi-vertex, "
                  << (base_is_closed ? "closed" : "open")
                  << "): skin dropped over base indices ["
                  << result.dropped_base_ranges_lo[k] << ".."
                  << result.dropped_base_ranges_hi[k]
                  << "] due to insufficient local thickness";
  }

  // M/N ratio diagnostic (issue-20260521 §6): Clipper2 inset merges /
  // truncates base vertices when |dist| approaches local half-thickness.
  // Empirically (mh104 TE thickness sweep) the downstream gmsh recovery
  // breaks once M/N drops below ~0.7 — surface that as a user-visible
  // warning so the report is actionable (reduce layup thickness or split
  // layup) rather than a raw `Unable to recover edge` from gmsh.
  {
    const std::size_t n_base_distinct = base_is_closed ? size - 1 : size;
    const std::size_t n_off_distinct  =
        base_is_closed ? offset_vertices.size() - 1
                       : offset_vertices.size();
    constexpr double kMNRatioWarn = 0.7;
    if (n_base_distinct > 0
        && static_cast<double>(n_off_distinct)
             < kMNRatioWarn * static_cast<double>(n_base_distinct)) {
      const double ratio =
          static_cast<double>(n_off_distinct)
          / static_cast<double>(n_base_distinct);
      std::ostringstream oss;
      oss.precision(2);
      oss << std::fixed
          << "offset (multi-vertex, "
          << (base_is_closed ? "closed" : "open")
          << "): only " << n_off_distinct
          << " offset verts produced for " << n_base_distinct
          << " base verts (M/N=" << ratio << "). The layup half-thickness "
          << std::fabs(dist)
          << " likely exceeds the local base curvature radius along part "
             "of the curve — Clipper2 has merged base corners during inset. "
             "Downstream mesh recovery typically fails below M/N=0.7. "
             "Consider reducing layup thickness, splitting the layup over a "
             "denser baseline, or excluding the thin region from the layup.";
      PUI_WARN << oss.str();
      PLOG(warning) << oss.str();
    }
  }

  assertValidBaseOffsetMap(id_pairs, "offset clipper2 backend");
  return 1;
}
