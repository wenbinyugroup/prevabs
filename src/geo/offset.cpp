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
#include "geo_diagnostics.hpp"
#include "debug/segmentBuildDump.hpp"

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

// ---------------------------------------------------------------------------
// Stage E pre-trim (plan-20260522-stage-e-pretrim.md, OPEN base only).
//
// Build a thin-mask via signedHalfThickness with α = 1.2 (production-tuned;
// Phase B), treating an interior INF vertex as a cusp apex only when bordered
// by a finite-thin neighbour. If exactly one strictly-interior contiguous thin
// run exists, produce a trimmed polyline that bridges the cusp tip with a
// single chord and report the run.
//
// Returns true (and fills `trimmed` + `run`) when a trim was applied; false
// otherwise — the caller then keeps the untrimmed `base_pts`.
bool buildStageEPreTrimInput(
    const std::vector<PDCELVertex *> &base,
    const std::vector<SPoint2> &base_pts,
    int side, double dist,
    std::vector<SPoint2> &trimmed,
    prevabs::geo::ThinRun &run) {
  constexpr double kPreTrimAlpha = 1.2;
  const int n_distinct = static_cast<int>(base.size());
  const double abs_dist = std::fabs(dist);
  const double threshold = kPreTrimAlpha * abs_dist;

  // Pass 1: finite half-thickness per interior vertex; mark finite-thin.
  std::vector<double> h_vals(n_distinct, INF);
  std::vector<bool> finite_thin(n_distinct, false);
  for (int i = 1; i < n_distinct - 1; ++i) {
    const double h = prevabs::geo::signedHalfThickness(
        base, i, side, /*base_is_closed*/ false);
    h_vals[i] = h;
    if (h < INF * 0.5 && h < threshold - TOLERANCE) {
      finite_thin[i] = true;
    }
  }
  // Pass 2: an INF interior vertex counts as a cusp apex only when an
  // immediate neighbour is finite-thin (distinguishes a real cusp from a
  // healthy short web whose interior vertices all return INF).
  std::vector<bool> thin_mask(n_distinct, false);
  for (int i = 1; i < n_distinct - 1; ++i) {
    if (finite_thin[i]) {
      thin_mask[i] = true;
    } else if (h_vals[i] >= INF * 0.5) {
      const bool left_finite_thin  = (i > 0)              && finite_thin[i - 1];
      const bool right_finite_thin = (i < n_distinct - 1) && finite_thin[i + 1];
      thin_mask[i] = left_finite_thin || right_finite_thin;
    }
  }

  if (!prevabs::geo::extractSingleInteriorRun(thin_mask, &run)) {
    return false;
  }
  std::vector<SPoint2> t =
      prevabs::geo::buildTrimmedOpenPolyline(base_pts, run);
  if (t.size() < 2) {
    // Should not happen — extractSingleInteriorRun guarantees lo > 0 and
    // hi < N-1 so the trimmed polyline has >= 2 vertices.
    return false;
  }
  trimmed = std::move(t);
  PLOG(warning)
      << "offset (multi-vertex, open): Stage E pre-trim removed base["
      << run.lo << ".." << run.hi << "] ("
      << (run.hi - run.lo + 1)
      << " vertices) due to local half-thickness < "
      << kPreTrimAlpha << " * |dist| = " << threshold
      << "; skin will be reported dropped over these indices";
  return true;
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
 * @param enable_pretrim   Optional (default false): when true, run the
 *                         Stage E pre-trim (open base only) that bridges a
 *                         single interior thin cusp with a chord before the
 *                         Clipper2 core. Off by default — callers opt in.
 * @return int             1 on success, 0 on degenerate input or
 *                         Clipper2 backend failure.
 */
int offset(const std::vector<PDCELVertex *> &base, int side, double dist,
           std::vector<PDCELVertex *> &offset_vertices,
           BaseOffsetMap &id_pairs,
           std::vector<bool> *offset_resampled,
           std::vector<SPoint2> *pre_resample_raw_points,
           std::vector<int> *dropped_base_ranges_lo,
           std::vector<int> *dropped_base_ranges_hi,
           bool enable_pretrim) {


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
  if (dropped_base_ranges_lo) dropped_base_ranges_lo->clear();
  if (dropped_base_ranges_hi) dropped_base_ranges_hi->clear();

  if (config.debug_level >= DebugLevel::geo) {
        PLOG(debug) <<
      "offset (multi-vertex): size=" + std::to_string(size)
      + " side=" + std::to_string(side)
      + " dist=" + std::to_string(dist);
  }

  // -------------------------------------------------------------------------
  // Two steps: raw geometry (offsetGeometry), then base-offset map with
  // resample (buildBaseOffsetMap). offset() is the convenience composition;
  // callers that want raw geometry only (e.g. the layered-offset path) call
  // offsetGeometry() directly. The 2-vertex fast path and diagnostics live in
  // offsetGeometry.
  // -------------------------------------------------------------------------
  OffsetGeometry geom = offsetGeometry(base, side, dist, enable_pretrim);
  if (!geom.ok) {
    PLOG(error)
        << "offset (multi-vertex, "
        << (geom.base_is_closed ? "closed" : "open")
        << "): Clipper2 returned no offset polygon (local thickness "
           "< |dist| somewhere along the path?); returning empty result";
    return 0;
  }
  return buildBaseOffsetMap(base, side, dist, geom, offset_vertices, id_pairs,
                            offset_resampled, pre_resample_raw_points,
                            dropped_base_ranges_lo, dropped_base_ranges_hi);
}

// ===========================================================================
// Two-step offset: raw geometry (offsetGeometry) + staircase base-offset map
// with resample (buildBaseOffsetMap). See geo.hpp.
// ===========================================================================

OffsetGeometry offsetGeometry(const std::vector<PDCELVertex *> &base,
                              int side, double dist, bool enable_pretrim) {
  OffsetGeometry geom;
  if (base.size() < 2) return geom;  // ok stays false

  // Diagnostic (informational): offset distance vs shortest base segment.
  prevabs::geo::checkOffsetDistanceVsShortestEdge(base, dist);

  // Fast path: 2-vertex polyline = a single segment, no Clipper2 junctions.
  // Produce the two translated endpoints as a trivial raw run; the staircase
  // (0,0)(1,1) is materialized in buildBaseOffsetMap (two_vertex branch).
  if (base.size() == 2) {
    PDCELVertex *s0 = new PDCELVertex();
    PDCELVertex *s1 = new PDCELVertex();
    offset(base[0], base[1], side, dist, s0, s1);  // single-segment overload
    prevabs::geo::OffsetPolygon poly;
    poly.is_closed = false;
    poly.points = {SPoint2(s0->point2()[0], s0->point2()[1]),
                   SPoint2(s1->point2()[0], s1->point2()[1])};
    poly.sources   = {{0, 0.0}, {0, 1.0}};
    poly.resampled = {false, false};
    delete s0;
    delete s1;
    geom.polygons.push_back(std::move(poly));
    geom.base_is_closed = false;
    geom.clipper_side   = side;
    geom.abs_dist       = std::fabs(dist);
    geom.two_vertex     = true;
    geom.ok             = true;
    return geom;
  }

  geom.base_is_closed = (base.front() == base.back());

  // Diagnostic (informational): local half-thickness precheck.
  prevabs::geo::warnLocalThinRegions(base, side, dist, geom.base_is_closed);

  std::vector<SPoint2> base_pts;
  base_pts.reserve(base.size());
  for (auto *v : base) {
    base_pts.emplace_back(v->point2()[0], v->point2()[1]);
  }
  // Closed inputs: PreVABS `side` is defined relative to the *directed*
  // base curve (left/right of travel), so whether that side is geometric
  // inward or outward depends on the curve's winding. Clipper2
  // InflatePaths (EndType::Polygon) inflates the polygon area for δ > 0
  // regardless of orientation, so the δ sign must fold in the winding:
  //   clipper_side = -orientation * side   (δ = sign(clipper_side)·|dist|)
  // where orientation = +1 for CCW (signed area > 0), -1 for CW.
  //   CCW: side=+1 (left)  → inward (δ<0); side=-1 (right) → outward.
  //   CW : flips, so a "right" layup on a CW baseline insets correctly.
  // Open polyline: side already matches Phase 1's single-side filter ⇒ pass.
  geom.clipper_side = side;
  if (geom.base_is_closed) {
    // Shoelace signed area over base_pts (trailing duplicate vertex of the
    // closed PreVABS convention contributes a zero-area term, so it is safe).
    double area2 = 0.0;
    const std::size_t n = base_pts.size();
    for (std::size_t i = 0; i < n; ++i) {
      const SPoint2 &p0 = base_pts[i];
      const SPoint2 &p1 = base_pts[(i + 1) % n];
      area2 += p0.x() * p1.y() - p1.x() * p0.y();
    }
    const int orientation = (area2 >= 0.0) ? 1 : -1;
    geom.clipper_side = -orientation * side;
  }
  geom.abs_dist     = std::fabs(dist);

  // Stage E pre-trim (OPEN base only; off unless enable_pretrim). Bridges a
  // single interior thin cusp before Clipper2; remap happens in step 2.
  geom.clipper_input = base_pts;
  if (enable_pretrim && !geom.base_is_closed) {
    geom.did_pretrim = buildStageEPreTrimInput(
        base, base_pts, side, dist, geom.clipper_input, geom.thin_run);
  }

  // Geometry core — RAW (resample_open = false). The base-vertex resample is
  // applied later by buildBaseOffsetMap, not here.
  geom.polygons = prevabs::geo::offsetWithClipper2(
      geom.clipper_input, geom.base_is_closed, geom.clipper_side, geom.abs_dist,
      prevabs::geo::JoinTypeChoice::Miter, 2.0,
      /*resample_open*/ false, /*experimental*/ false);
  geom.ok = !geom.polygons.empty();
  return geom;
}

int buildBaseOffsetMap(const std::vector<PDCELVertex *> &base, int side,
                       double dist, OffsetGeometry &geom,
                       std::vector<PDCELVertex *> &offset_vertices,
                       BaseOffsetMap &id_pairs,
                       std::vector<bool> *offset_resampled,
                       std::vector<SPoint2> *pre_resample_raw_points,
                       std::vector<int> *dropped_base_ranges_lo,
                       std::vector<int> *dropped_base_ranges_hi,
                       bool resample) {
  prevabs::debug::SegmentTraceScope _trace_scope(
      "geo::buildBaseOffsetMap (" + std::string(geom.base_is_closed ? "closed"
                                                                     : "open")
      + ", resample=" + (resample ? "Y" : "N") + ")");
  (void)side;
  offset_vertices.clear();
  id_pairs.clear();
  if (offset_resampled) offset_resampled->clear();
  if (pre_resample_raw_points) pre_resample_raw_points->clear();
  if (dropped_base_ranges_lo) dropped_base_ranges_lo->clear();
  if (dropped_base_ranges_hi) dropped_base_ranges_hi->clear();

  if (!geom.ok) {
    PLOG(error) << "buildBaseOffsetMap: offset geometry is empty / not ok";
    prevabs::debug::segmentTracePush("offset geometry not ok -> FAIL");
    return 0;
  }

  // 2-vertex fast path: trivial 1:1 staircase, no resample / reverse-match.
  if (geom.two_vertex && geom.polygons.size() == 1
      && geom.polygons[0].points.size() == 2) {
    prevabs::debug::segmentTracePush("two-vertex fast path (1:1 staircase)");
    const auto &pts = geom.polygons[0].points;
    offset_vertices.push_back(new PDCELVertex(0.0, pts[0].x(), pts[0].y()));
    offset_vertices.push_back(new PDCELVertex(0.0, pts[1].x(), pts[1].y()));
    id_pairs.push_back(BaseOffsetPair(0, 0));
    id_pairs.push_back(BaseOffsetPair(1, 1));
    if (offset_resampled) offset_resampled->assign(2, false);
    assertValidBaseOffsetMap(id_pairs, "offset fast path");
    return 1;
  }

  // 1. Base-vertex resample (moved here from the geometry core), against the
  //    same base that was fed to Clipper2 (trimmed if pre-trim ran). Skipped
  //    when `resample=false` (layered path keeps raw clean-miter geometry).
  if (resample) {
    prevabs::geo::resampleOpenRuns(geom.polygons, geom.clipper_input,
                                   geom.clipper_side, geom.abs_dist,
                                   /*do_miter_resample*/ false,
        prevabs::geo::openResampleModeFromString(
            config.offset_resample_mode));
  }
  // 2. Remap pre-trim base_seg back to original indices (after resample).
  if (geom.did_pretrim) {
    prevabs::geo::remapBaseSegToOriginal(geom.polygons, geom.thin_run);
  }

  // 3. Reverse-match staircase.
  auto result = prevabs::geo::buildBaseOffsetMapFromOffsetPolygons(
      base, geom.base_is_closed, geom.clipper_side, geom.abs_dist,
      geom.polygons);
  if (!result.ok) {
    PLOG(error)
        << "buildBaseOffsetMap (" << (geom.base_is_closed ? "closed" : "open")
        << "): reverse-match bridge failed to build a valid BaseOffsetMap";
    prevabs::debug::segmentTracePush("reverse-match bridge FAILED -> FAIL");
    return 0;
  }

  // Ensure the pre-trim range is recorded as dropped (avoid double-warning if
  // the Stage C forward-fill already covered it).
  if (geom.did_pretrim) {
    bool covered = false;
    for (std::size_t k = 0; k < result.dropped_base_ranges_lo.size(); ++k) {
      if (result.dropped_base_ranges_lo[k] <= geom.thin_run.lo
          && result.dropped_base_ranges_hi[k] >= geom.thin_run.hi) {
        covered = true;
        break;
      }
    }
    if (!covered) {
      result.dropped_base_ranges_lo.push_back(geom.thin_run.lo);
      result.dropped_base_ranges_hi.push_back(geom.thin_run.hi);
    }
  }

  offset_vertices = result.offset_vertices;
  id_pairs        = result.id_pairs;
  if (offset_resampled) *offset_resampled = result.offset_resampled;
  if (pre_resample_raw_points)
    *pre_resample_raw_points = result.pre_resample_raw_points;
  if (dropped_base_ranges_lo)
    *dropped_base_ranges_lo = result.dropped_base_ranges_lo;
  if (dropped_base_ranges_hi)
    *dropped_base_ranges_hi = result.dropped_base_ranges_hi;

  for (std::size_t k = 0; k < result.dropped_base_ranges_lo.size(); ++k) {
    PLOG(warning) << "offset (multi-vertex, "
                  << (geom.base_is_closed ? "closed" : "open")
                  << "): skin dropped over base indices ["
                  << result.dropped_base_ranges_lo[k] << ".."
                  << result.dropped_base_ranges_hi[k]
                  << "] due to insufficient local thickness";
  }

  // Diagnostic (user-facing): low M/N ratio.
  prevabs::geo::warnLowOffsetMNRatio(
      base.size(), offset_vertices.size(), geom.base_is_closed, dist);

  assertValidBaseOffsetMap(id_pairs, "offset clipper2 backend");
  prevabs::debug::segmentTracePush(
      "OK (offset_vertices=" + std::to_string(offset_vertices.size())
      + ", base=" + std::to_string(base.size()) + ")");
  return 1;
}
