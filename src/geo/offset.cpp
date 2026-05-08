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

#include <sstream>
// Route homog2d warnings to the prevabs debug logger instead of stderr.
// Must be defined before homog2d.hpp is processed.
#define HOMOG2D_LOG_WARNING(a) \
  do { std::ostringstream _h2oss; _h2oss << a; PLOG(debug) << _h2oss.str(); } while(0)
#include "homog2d.hpp"

#include "geo_types.hpp"

#include <cassert>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <list>
#include <sstream>
#include <string>
#include <unordered_set>
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
// Static helpers for the multi-vertex offset() function
// ---------------------------------------------------------------------------

/**
 * @brief Step 1: Offset every base segment and compute junction vertices using h2d.
 *
 * For each consecutive pair of base vertices, an offset segment is computed.
 * Adjacent offset segments are intersected to yield the junction vertex that
 * is added to the returned list. The caller receives a flat list of junction
 * vertices and the corresponding base-offset map entries.
 *
 * @param base        Original polyline vertices (must have at least 3 vertices;
 *                    an error is logged and an empty list is returned otherwise).
 * @param side        Offset side (+1 / -1).
 * @param dist        Offset distance.
 * @param junction_map Output: one entry per returned junction vertex.
 *                     `pair.base` is the corresponding base-vertex index.
 *                     `pair.offset` is the current junction-vertex index.
 * @return            The junction-vertex list.
 *
 * @note Ownership: every vertex in the returned list is heap-allocated by this
 *       function.  The caller is responsible for deleting them when no longer
 *       needed.
 */
static std::vector<PDCELVertex *> computeOffsetJunctions(
    const std::vector<PDCELVertex *> &base,
    int side, double dist,
    BaseOffsetMap &junction_map
  ) {


  std::size_t size = base.size();
  junction_map.clear();

  if (size < 3) {
    PLOG(error) << "computeOffsetJunctions: base must have at least 3 vertices (got "
      + std::to_string(size) + "); returning empty junction list";
    return {};
  }

  if (config.debug) {
    std::ostringstream oss;
    oss << "computeOffsetJunctions: " << size << " base vertices"
        << ", side=" << side << ", dist=" << dist;
        PLOG(debug) << oss.str();
  }

  // junctions: the output junction-vertex list, one entry per base vertex.
  // Each entry is the junction point between the offset of segment [i-1,i] and
  // the offset of segment [i,i+1] (or just the endpoint for i=0 and i=size-1).
  std::vector<PDCELVertex *> junctions;
  junctions.reserve(size);

  // local_allocs: tracks every vertex heap-allocated by this function.
  // At cleanup time, any entry absent from junctions is freed here; the rest
  // are returned to the caller, which takes ownership and must delete them.
  // Reserve for two endpoints per segment; intersection vertices may add a few
  // more entries beyond this but are infrequent.
  std::vector<PDCELVertex *> local_allocs;
  local_allocs.reserve(2 * (size - 1));

  junction_map.reserve(size);

  // cur_start/cur_end: endpoints of the current segment's offset.
  // prev_start/prev_end: endpoints of the previous segment's offset (needed to
  //   compute the junction with the current segment).
  PDCELVertex *cur_start = nullptr, *cur_end = nullptr;
  PDCELVertex *prev_start = nullptr, *prev_end = nullptr;

  // Iterate over each consecutive pair of base vertices (i.e. each segment).
  for (int i = 0; i < static_cast<int>(size) - 1; ++i) {
    if (config.debug) {
      std::ostringstream oss;
      oss << "segment [" << i << "]: base " << base[i] << " -> " << base[i + 1];
            PLOG(debug) << oss.str();
    }

    // Allocate fresh endpoint vertices for this segment's offset and compute
    // their positions via the single-segment offset() overload.
    cur_start = new PDCELVertex();
    cur_end = new PDCELVertex();
    local_allocs.push_back(cur_start);
    local_allocs.push_back(cur_end);

    offset(base[i], base[i + 1], side, dist, cur_start, cur_end);

    if (config.debug) {
      std::ostringstream oss;
      oss << "  offset result: cur_start=" << cur_start << ", cur_end=" << cur_end;
            PLOG(debug) << oss.str();
    }

    // Record that the junction vertex we are about to add corresponds to base
    // vertex i (the shared vertex between segment i-1 and segment i).
    junction_map.push_back(
        BaseOffsetPair(i, static_cast<int>(junctions.size())));

    if (i == 0) {
      // First segment: no previous segment to intersect with, so the junction
      // is simply the start endpoint of this offset segment.
      junctions.push_back(cur_start);
      if (config.debug) {
                PLOG(debug) << "  first segment: junction = cur_start (no prev segment)";
      }
    } else {
      // General case: find where the infinite lines through the previous and
      // current offset segments meet. That meeting point is the miter junction
      // — the corner of the offset polyline at base vertex i.
      //
      // We use the homog2d (h2d) library for robust 2-D intersection.
      // Note: h2d operates in 2-D (XY plane); Z is ignored here.
      h2d::Point2d prev_p1(prev_start->point2()[0], prev_start->point2()[1]);
      h2d::Point2d prev_p2(prev_end->point2()[0], prev_end->point2()[1]);
      h2d::Point2d cur_p1(cur_start->point2()[0], cur_start->point2()[1]);
      h2d::Point2d cur_p2(cur_end->point2()[0], cur_end->point2()[1]);

      if (config.debug) {
        std::ostringstream oss;
        oss << "  find intersection:\n"
            << "        prev_start = " << prev_start << ", prev_end = " << prev_end << "\n"
            << "        cur_start = "  << cur_start  << ", cur_end = "  << cur_end  << "\n"
            << "        prev segment: " << prev_p1 << " - " << prev_p2 << "\n"
            << "        cur  segment: " << cur_p1  << " - " << cur_p2;
                PLOG(debug) << oss.str();
      }

      // Use infinite-line intersection so the miter point is found even when
      // it lies outside the finite extent of either offset segment (outside
      // corners).  h2d::Line2d::intersects(Line2d) returns no result only for
      // parallel lines; for collinear lines h2d treats them as parallel and
      // returns no intersection, so the else-branch below (which uses prev_end
      // as the junction) remains correct for that degenerate case.
      h2d::Segment prev_seg(prev_p1, prev_p2);
      h2d::Segment cur_seg(cur_p1, cur_p2);
      auto isect = prev_seg.getLine().intersects(cur_seg.getLine());
      if (isect()) {
        // Non-parallel segments: use the intersection point as the junction vertex.
        auto isect_pt = isect.get();

        if (config.debug) {
          std::ostringstream oss;
          oss << "  intersection found: " << isect_pt;
                    PLOG(debug) << oss.str();
        }

        // If the intersection coincides (within tolerance) with an already-
        // allocated endpoint, reuse that vertex to avoid creating a duplicate.
        // Otherwise allocate a new vertex at the exact intersection point.
        if (isClose(isect_pt.getX(), isect_pt.getY(), prev_p1.getX(), prev_p1.getY(), ABS_TOL, REL_TOL)) {
          junctions.push_back(prev_start);
          if (config.debug) {
                        PLOG(debug) << "  snapped to prev_start (within tolerance)";
          }
        } else if (isClose(isect_pt.getX(), isect_pt.getY(), prev_p2.getX(), prev_p2.getY(), ABS_TOL, REL_TOL)) {
          junctions.push_back(prev_end);
          if (config.debug) {
                        PLOG(debug) << "  snapped to prev_end (within tolerance)";
          }
        } else {
          PDCELVertex *v_junction = new PDCELVertex(0, isect_pt.getX(), isect_pt.getY());
          local_allocs.push_back(v_junction);
          junctions.push_back(v_junction);
          if (config.debug) {
                        PLOG(debug) << "  new junction vertex allocated at intersection point";
          }
        }
      } else {
        // Parallel or collinear segments: h2d returns no intersection point.
        // For strictly parallel (non-collinear) segments, no miter junction
        // exists; for collinear (overlapping) segments, prev_end is the shared
        // endpoint and is the geometrically correct junction in both cases.
        junctions.push_back(prev_end);
        if (config.debug) {
                    PLOG(debug) << "  segments parallel/collinear: fell back to prev_end";
        }
      }

      if (config.debug) {
        std::ostringstream oss;
        oss << "  junction[" << junctions.size() - 1 << "] = " << junctions.back();
                PLOG(debug) << oss.str();
      }
    }

    // After processing the last segment, append its tail endpoint as the final
    // junction vertex and record its correspondence with base vertex i+1.
    if (i == static_cast<int>(size) - 2) {
      junctions.push_back(cur_end);
      junction_map.push_back(
          BaseOffsetPair(i + 1, static_cast<int>(junctions.size()) - 1));
      if (config.debug) {
        std::ostringstream oss;
        oss << "  last segment: appended tail junction[" << junctions.size() - 1
            << "] = " << cur_end;
                PLOG(debug) << oss.str();
      }
    }

    prev_start = cur_start;
    prev_end = cur_end;
  }

  // Free every locally-allocated vertex that is not being returned.
  // All allocations (segment endpoints and intersection vertices) are tracked
  // in local_allocs, so this single loop covers all cleanup.  Vertices that
  // do appear in junctions are not freed here — the caller owns them.
  //
  // Use an unordered_set for O(1) membership tests, giving O(n) total cleanup
  // instead of the O(n²) that std::find over junctions would produce.
  const std::unordered_set<PDCELVertex *> retained(junctions.begin(), junctions.end());
  for (auto ep : local_allocs) {
    if (!retained.count(ep)) {
      delete ep;
    }
  }

  if (config.debug) {
    std::ostringstream oss;
    oss << "computeOffsetJunctions: returning " << junctions.size() << " junction vertices";
        PLOG(debug) << oss.str();
  }

  return junctions;
}

/**
 * @brief Step 2: Group valid offset segments into sub-lines.
 *
 * An offset segment is valid when its direction agrees with the corresponding
 * base segment (positive dot product) and has non-zero length.
 * Consecutive valid segments are gathered into sub-lines.
 *
 * @param base           Original polyline vertices.
 * @param vertices_tmp   Junction-vertex list from computeOffsetJunctions.
 *                       Must have the same size as base.
 * @param junction_map   Mapping from computeOffsetJunctions. Must have the
 *                       same size as base.
 * @param lines_group    Output: cleared on entry; each element is one valid sub-line.
 * @param maps_group     Output: cleared on entry; mapping entries for each
 *                       sub-line vertex.
 */
static void groupValidSegments(
    const std::vector<PDCELVertex *> &base,
    const std::vector<PDCELVertex *> &vertices_tmp,
    const BaseOffsetMap &junction_map,
    std::vector<std::vector<PDCELVertex *>> &lines_group,
    std::vector<BaseOffsetMap> &maps_group) {


  std::size_t size = base.size();

  // Enforce size consistency: vertices_tmp and junction_map must match base.
  if (vertices_tmp.size() != size || junction_map.size() != size) {
    PLOG(error) << "groupValidSegments: size mismatch — base=" + std::to_string(size)
      + ", vertices_tmp=" + std::to_string(vertices_tmp.size())
      + ", junction_map=" + std::to_string(junction_map.size())
      + "; skipping grouping";
    return;
  }

  // Clear outputs so this function always produces a fresh result regardless
  // of what the caller passed in.
  lines_group.clear();
  maps_group.clear();

  if (config.debug) {
    std::ostringstream oss;
    oss << "groupValidSegments: " << size << " base vertices, "
        << static_cast<int>(size) - 1 << " segment(s) to check";
        PLOG(debug) << oss.str();
  }

  // prev_valid: whether the previous offset segment passed the validity test.
  // Tracked across iterations to decide whether to open a new sub-line or
  // extend the current one.
  bool prev_valid = false;
  bool cur_valid  = false;

  for (int j = 0; j < static_cast<int>(size) - 1; ++j) {
    // Vectors along the base segment and the corresponding offset segment.
    SVector3 vec_base = SVector3(base[j]->point(), base[j + 1]->point());
    SVector3 vec_off  = SVector3(vertices_tmp[j]->point(), vertices_tmp[j + 1]->point());

    // Validity test — either condition marks the segment as invalid and
    // breaks the current sub-line:
    //   folded_back  dot(base, off) <= 0: the offset segment points opposite
    //                (or perpendicular) to the base — the polyline has been
    //                over-offset and folded back on itself.
    //   degenerate   normSq < ε²: the offset segment has collapsed to a point.
    const bool folded_back = (dot(vec_base, vec_off) <= 0);
    const bool degenerate  = (vec_off.normSq() < TOLERANCE * TOLERANCE);
    cur_valid = !folded_back && !degenerate;

    if (config.debug) {
      std::ostringstream oss;
      oss << "  segment [" << j << "]: "
          << (cur_valid    ? "valid"
              : folded_back ? "invalid (folded back)"
                            : "invalid (degenerate)");
            PLOG(debug) << oss.str();
    }

    if (cur_valid) {
      if (!prev_valid) {
        // This segment starts a new run of valid segments — open a new sub-line.
        lines_group.push_back(std::vector<PDCELVertex *>{vertices_tmp[j]});
        maps_group.push_back(BaseOffsetMap{junction_map[j]});
        if (config.debug) {
          std::ostringstream oss;
          oss << "    opened sub-line " << lines_group.size() - 1
              << " at junction vertex " << j;
                    PLOG(debug) << oss.str();
        }
      }
      // Extend the current sub-line with the tail vertex of this segment.
      lines_group.back().push_back(vertices_tmp[j + 1]);
      maps_group.back().push_back(junction_map[j + 1]);
    } else if (prev_valid) {
      // This segment is invalid and the previous one was valid — the run just
      // ended; the current sub-line is now complete.
      if (config.debug) {
        std::ostringstream oss;
        oss << "    closed sub-line " << lines_group.size() - 1
            << " before segment " << j;
                PLOG(debug) << oss.str();
      }
    }

    prev_valid = cur_valid;
  }

  if (config.debug) {
    std::ostringstream oss;
    oss << "groupValidSegments: produced " << lines_group.size() << " sub-line(s)";
        PLOG(debug) << oss.str();
  }
}

/**
 * @brief Trim a pair of adjacent offset sub-lines at their mutual intersection.
 *
 * Finds the intersection of tail_line's tail region with head_line's head region,
 * inserts the junction vertex into both curves, trims tail_line from the
 * junction toward its tail end and head_line from the junction toward its head
 * end, and adjusts the base-index link vectors accordingly.
 *
 * @param tail_line  Tail sub-line (modified in place): trimmed from the junction
 *                   toward its tail; the junction becomes its new last vertex.
 * @param head_line  Head sub-line (modified in place): trimmed from the junction
 *                   toward its head; the junction becomes its new first vertex.
 * @param map_tail   Base-offset map entries for tail_line (modified in place).
 * @param map_head   Base-offset map entries for head_line (modified in place).
 */
static void trimSubLinePair(
    std::vector<PDCELVertex *> &tail_line,
    std::vector<PDCELVertex *> &head_line,
    BaseOffsetMap &map_tail,
    BaseOffsetMap &map_head
  ) {


  if (config.debug) {
    std::ostringstream oss;
    oss << "trimSubLinePair:"
        << " tail_line=" << tail_line.size() << " vertices"
        << ", head_line=" << head_line.size() << " vertices";
        PLOG(debug) << oss.str();
  }

  // Step 1: find all pairwise intersections between the two sub-lines.
  // isect_segs_tail / isect_segs_head: segment indices in each sub-line.
  // params_tail / params_head: parametric locations (0–1) on those segments.
  std::vector<int> isect_segs_tail, isect_segs_head;
  std::vector<double> params_tail, params_head;
  findAllIntersections(
    tail_line, head_line,
    isect_segs_tail, isect_segs_head, params_tail, params_head
  );

  if (isect_segs_tail.empty()) {
    PLOG(warning) << "no intersection found between consecutive offset sub-lines; joining at endpoints";
    return;
  }

  if (config.debug) {
    std::ostringstream oss;
    oss << "  found " << isect_segs_tail.size() << " intersection(s)";
        PLOG(debug) << oss.str();
  }

  // Step 2: among all intersections, pick the one closest to the tail end of
  // tail_line.
  //   which_end=1   — select the intersection nearest the tail (end) of the curve.
  //   inner_only=0  — consider all intersections, not only interior ones.
  // Outputs: seg_idx_tail (segment index in tail_line), isect_idx (position in
  // the intersection arrays for the chosen intersection), u_tail (parametric
  // location on that segment).
  int seg_idx_tail, isect_idx;
  double u_tail = getIntersectionLocation(
    tail_line, isect_segs_tail, params_tail,
    /*which_end=*/1, /*inner_only=*/0,
    seg_idx_tail, isect_idx
  );

  if (isect_idx < 0 || isect_idx >= static_cast<int>(isect_segs_head.size())) {
    PLOG(warning) << "intersection index out of range; skipping trim for this sub-line pair";
    return;
  }

  // Retrieve the matching intersection data on the head_line side.
  int    seg_idx_head = isect_segs_head[isect_idx];
  double u_head       = params_head[isect_idx];

  if (config.debug) {
    std::ostringstream oss;
    oss << "  selected intersection:"
        << " tail seg=" << seg_idx_tail << " u=" << u_tail
        << ", head seg=" << seg_idx_head << " u=" << u_head;
        PLOG(debug) << oss.str();
  }

  // Step 3: obtain or create the junction vertex at the intersection point.
  //   which_end_1=1  — use the intersection nearest the tail end of tail_line.
  //   which_end_2=0  — use the intersection nearest the head end of head_line.
  //   inner_only_1/2=0 — consider all intersections, not only interior ones.
  // is_new_tail / is_new_head: output flags — 1 if a new vertex was
  //   heap-allocated for that curve, 0 if an existing endpoint was reused.
  //   In either case getIntersectionVertex inserts the junction vertex into
  //   both curves, so no additional ownership action is needed here.
  int is_new_tail, is_new_head;
  PDCELVertex *junction = getIntersectionVertex(
    tail_line, head_line,
    seg_idx_tail, seg_idx_head, u_tail, u_head,
    is_new_tail, is_new_head, TOLERANCE
  );

  if (!junction) {
    PLOG(error) << "getIntersectionVertex returned null; skipping trim for this sub-line pair";
    return;
  }

  if (config.debug) {
    std::ostringstream oss;
    oss << "  junction vertex: " << junction
        << " (is_new_tail=" << is_new_tail
        << ", is_new_head=" << is_new_head << ")";
        PLOG(debug) << oss.str();
  }

  // Step 4: trim the curves at the junction vertex.
  //   trimCurveAtVertex(..., CurveEnd::End): keep from the beginning up to
  //     junction;
  //     discard the tail.
  //   trimCurveAtVertex(..., CurveEnd::Begin): keep from junction to the end;
  //     discard the head.
  trimCurveAtVertex(tail_line, junction, CurveEnd::End);
  trimCurveAtVertex(head_line, junction, CurveEnd::Begin);

  if (config.debug) {
    std::ostringstream oss;
    oss << "  after trim:"
        << " tail_line=" << tail_line.size() << " vertices"
        << ", head_line=" << head_line.size() << " vertices";
        PLOG(debug) << oss.str();
  }

  // Step 5: adjust the mapping entries to match the trimmed curves.
  //
  // tail_line now ends at junction, which was at segment seg_idx_tail.
  // Keep only the first seg_idx_tail + 1 entries (0 … seg_idx_tail).
  map_tail.resize(seg_idx_tail + 1);
  //
  // head_line now starts at junction, which was at segment seg_idx_head.
  // Drop the first seg_idx_head - 1 entries (those before the junction segment).
  if (seg_idx_head > 1) {
    map_head.erase(map_head.begin(), map_head.begin() + (seg_idx_head - 1));
  }

  if (config.debug) {
    std::ostringstream oss;
    oss << "  after map adjust:"
        << " map_tail=" << map_tail.size() << " entries"
        << ", map_head=" << map_head.size() << " entries";
        PLOG(debug) << oss.str();
  }
}

/**
 * @brief Step 3.5: Remove interior self-intersecting loops within a sub-line.
 *
 * At highly curved baseline regions the miter junction from computeOffsetJunctions
 * can overshoot, creating a small self-intersecting loop even when
 * groupValidSegments detects no fold (because the base direction is nearly
 * perpendicular to the offset reversal direction at the nose of the curve).
 *
 * The function searches for the first pair of non-adjacent segments (j >= i+2)
 * that cross, inserts the intersection vertex, and removes the loop vertices
 * between them.  The process repeats until no interior crossing remains.
 *
 * Map notes: after removal the maps_group entry for the intersection vertex
 * reuses the entry of the first trimmed vertex (map[i+1]) so that Step 5's
 * id_pairs assignment and forward-fill produce a valid staircase.  The
 * maps_group staircase invariant itself may be violated (base_delta > 1
 * between the new entry and the next retained entry); assertValidBaseOffsetMap
 * is therefore NOT called for maps_group after this step.
 *
 * @param line  Sub-line vertex list; modified in place.
 * @param map   BaseOffsetMap for this sub-line; modified to stay in sync
 *              with line (map.size() == line.size() after return).
 */
static void trimSubLineSelfIntersections(
    std::vector<PDCELVertex *> &line,
    BaseOffsetMap &map
  ) {

  if (line.size() != map.size()) {
    PLOG(error) << "trimSubLineSelfIntersections: line/map size mismatch before trim ("
                << line.size() << " vs " << map.size() << ")";
    return;
  }

  bool found = true;
  while (found) {
    found = false;
    const int n = static_cast<int>(line.size());

    for (int i = 0; i < n - 2 && !found; ++i) {
      for (int j = i + 2; j < n - 1 && !found; ++j) {
        double u1, u2;
        const bool not_parallel = calcLineIntersection2D(
            line[i], line[i + 1],
            line[j], line[j + 1],
            u1, u2, TOLERANCE);

        // Only act on true interior-to-interior crossings.
        if (!not_parallel || u1 <= 0.0 || u1 >= 1.0 || u2 <= 0.0 || u2 >= 1.0)
          continue;

        PDCELVertex *xx = new PDCELVertex(
            getParametricPoint(line[i]->point(), line[i + 1]->point(), u1));

        if (config.debug) {
          std::ostringstream oss;
          oss << "trimSubLineSelfIntersections: loop between seg "
              << i << " and seg " << j
              << " u1=" << u1 << " u2=" << u2
              << " at " << xx;
          PLOG(debug) << oss.str();
        }

        // Rebuild line: erase loop [i+1 .. j], insert xx at position i+1.
        line.erase(line.begin() + i + 1, line.begin() + j + 1);
        line.insert(line.begin() + i + 1, xx);

        // Rebuild map to stay in sync (map.size() must equal line.size()).
        // Assign xx the entry of the first trimmed vertex (map[i+1]) so that
        // id_pairs[i+1] gets xx's offset index in Step 5, and base vertices
        // i+2..j are forward-filled to that same index.
        BaseOffsetMap new_map;
        new_map.reserve(
            i + 1 + 1 + static_cast<int>(map.size()) - j - 1);
        for (int k = 0; k <= i; ++k)
          new_map.push_back(map[k]);
        new_map.push_back(map[i + 1]);  // entry for xx
        for (int k = j + 1; k < static_cast<int>(map.size()); ++k)
          new_map.push_back(map[k]);
        map = std::move(new_map);

        if (line.size() != map.size()) {
          PLOG(error) << "trimSubLineSelfIntersections: line/map size mismatch after trim ("
                      << line.size() << " vs " << map.size() << ")";
          return;
        }

        found = true;
      }
    }
  }
}

/**
 * @brief Step 5.5: Remove cross-sub-line self-intersecting loops from the
 *        joined output curve.
 *
 * After Step 5 joins all sub-lines, the result may contain a self-intersection
 * that spans sub-line boundaries — e.g., trimSubLinePair can produce a
 * degenerate "bridge" sub-line (two vertices) whose enclosing segments cross
 * a neighbour sub-line, creating a backward loop that Step 3.5 cannot see
 * because it operates per-sub-line before joining.
 *
 * The function finds the first pair of non-adjacent segments (j >= i+2)
 * in offset_vertices that cross in their interiors (u1, u2 in (0,1)),
 * inserts the intersection vertex at position i+1, removes the loop
 * [i+1..j], and updates id_pairs accordingly. Repeats until no crossing
 * remains.
 *
 * id_pairs update after removing loop [i+1..j] and inserting xx at i+1:
 *   - offset <= i        : unchanged
 *   - offset in [i+1, j] : set to i+1 (the new intersection vertex)
 *   - offset > j         : subtract (j - i - 1)
 * The resulting staircase remains valid (see comment in implementation).
 *
 * @param offset_vertices Joined output curve; modified in place.
 * @param id_pairs        BaseOffsetMap for the full curve; modified in sync.
 */
static void trimJoinedCurveSelfIntersections(
    std::vector<PDCELVertex *> &offset_vertices,
    BaseOffsetMap &id_pairs) {

  bool found = true;
  while (found) {
    found = false;
    const int n = static_cast<int>(offset_vertices.size());
    const bool is_closed =
        n >= 3
        && offset_vertices.front() != nullptr
        && offset_vertices.back() != nullptr
        && offset_vertices.front()->point().distance(
               offset_vertices.back()->point()) <= TOLERANCE;
    const double param_tol = TOLERANCE;

    for (int i = 0; i < n - 2 && !found; ++i) {
      for (int j = i + 2; j < n - 1 && !found; ++j) {
        // For a closed curve the first and last segments are adjacent through
        // the closure vertex, so their shared endpoint is not a removable loop.
        if (is_closed && i == 0 && j == n - 2) {
          continue;
        }

        double u1, u2;
        const bool not_parallel = calcLineIntersection2D(
            offset_vertices[i], offset_vertices[i + 1],
            offset_vertices[j], offset_vertices[j + 1],
            u1, u2, TOLERANCE);

        if (!not_parallel
            || u1 <= param_tol || u1 >= 1.0 - param_tol
            || u2 <= param_tol || u2 >= 1.0 - param_tol)
          continue;

        PDCELVertex *xx = new PDCELVertex(
            getParametricPoint(
                offset_vertices[i]->point(),
                offset_vertices[i + 1]->point(), u1));

        if (config.debug) {
          std::ostringstream oss;
          oss << "trimJoinedCurveSelfIntersections: loop between seg "
              << i << " and seg " << j
              << " u1=" << u1 << " u2=" << u2
              << " at " << xx->printString();
          PLOG(debug) << oss.str();
        }

        // Remove the loop [i+1..j] and insert xx at position i+1.
        // (Ownership of removed heap vertices is not transferred — the
        // vertices may have been shared across sub-line boundaries and the
        // caller retains responsibility for their lifetimes.)
        offset_vertices.erase(
            offset_vertices.begin() + i + 1,
            offset_vertices.begin() + j + 1);
        offset_vertices.insert(offset_vertices.begin() + i + 1, xx);

        // Update id_pairs to reflect the new offset indices.
        // Proof that the staircase invariant is preserved after clamping:
        //   - Entries with offset <= i: unchanged → staircase preserved.
        //   - Entries with offset in [i+1, j]: all become i+1 (non-decreasing
        //     within this range since they were already non-decreasing).
        //   - Transition from the last clamped entry (i+1) to the first
        //     non-clamped entry: old offset j+1 → new i+2, delta = 1. ✓
        //   - Entries with offset > j: each reduced by (j-i-1), relative
        //     order preserved → staircase preserved.
        const int loop_count = j - i;  // vertices removed = j - i
        for (auto &p : id_pairs) {
          if (p.offset >= i + 1 && p.offset <= j) {
            p.offset = i + 1;
          } else if (p.offset > j) {
            p.offset -= (loop_count - 1);
          }
        }

        found = true;
      }
    }
  }
}

// ---------------------------------------------------------------------------
// Public multi-vertex offset() function
// ---------------------------------------------------------------------------

/**
 * @brief Offsets a given set of vertices by a specified distance.
 *
 * This function takes a set of base vertices and offsets them by a specified distance
 * to create a new set of offset vertices. It handles both open and closed curves,
 * and ensures that the resulting offset vertices maintain the correct orientation.
 *
 * The five-step pipeline:
 *   Step 1: Compute offset junction vertices via computeOffsetJunctions.
 *   Step 2: Group valid (non-folded, non-degenerate) segments into sub-lines.
 *   Step 3: Trim adjacent sub-line pairs at their mutual intersection.
 *   Step 4: For closed curves, trim the head-tail junction as well.
 *   Step 5: Join sub-lines into the output vertex sequence and build mappings.
 *
 * @param base             The input vector of base vertices to be offset.
 *                         Must have at least 2 vertices.
 * @param side             The side on which to offset (e.g., left or right).
 * @param dist             The offset distance.
 * @param offset_vertices  Output: the offset vertex sequence.  Cleared on entry.
 * @param id_pairs         Output: list of {base_idx, offset_idx} pairs.  Cleared on entry.
 * @return int Returns 1 on success, 0 if the input is degenerate or all segments
 *             are invalid.
 */
int offset(const std::vector<PDCELVertex *> &base, int side, double dist,
           std::vector<PDCELVertex *> &offset_vertices,
           BaseOffsetMap &id_pairs) {


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

  if (config.debug) {
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

    return 1;
  }

  // -------------------------------------------------------------------------
  // Step 1: Compute offset junction vertices.
  // -------------------------------------------------------------------------
  if (config.debug) {
        PLOG(debug) << 
      "offset (multi-vertex): step 1 — compute junction vertices";
  }

  BaseOffsetMap junction_map;
  std::vector<PDCELVertex *> vertices_tmp =
      computeOffsetJunctions(base, side, dist, junction_map);
  assertValidBaseOffsetMap(junction_map, "offset step 1");

  // -------------------------------------------------------------------------
  // Step 2: Group valid offset segments into sub-lines.
  // -------------------------------------------------------------------------
  if (config.debug) {
        PLOG(debug) << 
      "offset (multi-vertex): step 2 — group valid segments";
  }

  std::vector<std::vector<PDCELVertex *>> lines_group;
  std::vector<BaseOffsetMap> maps_group;
  groupValidSegments(base, vertices_tmp, junction_map, lines_group, maps_group);
  for (std::size_t i = 0; i < maps_group.size(); ++i) {
    assertValidBaseOffsetMap(
        maps_group[i],
        "offset step 2 group " + std::to_string(i));
  }

  // Free junction vertices that were filtered out by groupValidSegments.
  // computeOffsetJunctions transfers ownership of all vertices_tmp entries to
  // this function; those not retained in lines_group must be deleted here.
  {
    std::unordered_set<PDCELVertex *> retained;
    for (auto& line : lines_group) {
      for (auto v : line) {
        retained.insert(v);
      }
    }
    for (auto v : vertices_tmp) {
      if (!retained.count(v)) {
        delete v;
      }
    }
  }

  // Guard: if all segments were invalid (folded-back or degenerate), there is
  // nothing to offset.
  if (lines_group.empty()) {
    PLOG(warning) << "offset (multi-vertex): all offset segments are invalid; returning empty result";
    return 0;
  }

  if (config.debug) {
        PLOG(debug) << 
      "offset (multi-vertex): " + std::to_string(lines_group.size())
      + " sub-line(s) after grouping";
  }

  // -------------------------------------------------------------------------
  // Step 3: Trim adjacent sub-line pairs at their mutual intersection.
  //
  // trimSubLinePair modifies both sub-lines in place, so lines_group and
  // maps_group are the authoritative post-trim state.  No separate
  // "trimmed" copy is maintained — Step 5 reads lines_group directly.
  // -------------------------------------------------------------------------
  if (config.debug) {
        PLOG(debug) << 
      "offset (multi-vertex): step 3 — trim adjacent sub-line pairs";
  }

  if (lines_group.size() > 1) {
    for (int line_i = 0; line_i < static_cast<int>(lines_group.size()) - 1; ++line_i) {
      if (config.debug) {
                PLOG(debug) << 
          "offset (multi-vertex): trimming sub-line pair "
          + std::to_string(line_i) + "/" + std::to_string(line_i + 1);
      }
      trimSubLinePair(
        lines_group[line_i], lines_group[line_i + 1],
        maps_group[line_i], maps_group[line_i + 1]
      );
      assertValidBaseOffsetMap(
          maps_group[line_i],
          "offset step 3 group " + std::to_string(line_i));
      assertValidBaseOffsetMap(
          maps_group[line_i + 1],
          "offset step 3 group " + std::to_string(line_i + 1));
    }
  }

  // -------------------------------------------------------------------------
  // Step 3.5: Trim any interior self-intersecting loops within each sub-line.
  //
  // A miter junction can overshoot at high-curvature baseline regions,
  // producing a loop that the dot-product fold test in groupValidSegments
  // misses (e.g. when the base is nearly horizontal at the leading edge).
  // NOTE: maps_group staircase invariant may be violated after this step —
  // assertValidBaseOffsetMap is intentionally NOT called here.  The final
  // id_pairs staircase is restored by Step 5's forward-fill.
  //
  // After P0.1, step 4 rebuilds the closed single-sub-line path atomically
  // and step 5 overwrites per-sub-line offset indices before validating the
  // final staircase, so step 3.5 can safely run again to remove local loops
  // earlier in the pipeline.
  // -------------------------------------------------------------------------
  if (config.debug) {
    PLOG(debug) <<
        "offset (multi-vertex): step 3.5 — trim sub-line self-intersections";
  }
  const bool base_is_closed = (base.front() == base.back());
  for (int line_i = 0; line_i < static_cast<int>(lines_group.size()); ++line_i) {
    if (base_is_closed && lines_group.size() == 1) {
      if (config.debug) {
        PLOG(debug) <<
            "offset (multi-vertex): step 3.5 — skipping closed single-sub-line; "
            "step 4 will resolve closure/self-intersection";
      }
      break;
    }
    trimSubLineSelfIntersections(lines_group[line_i], maps_group[line_i]);
    if (lines_group[line_i].size() != maps_group[line_i].size()) {
      PLOG(error) << "offset step 3.5: line/map size mismatch in group "
                  << line_i << " ("
                  << lines_group[line_i].size() << " vs "
                  << maps_group[line_i].size() << ")";
      return 0;
    }
  }

  // -------------------------------------------------------------------------
  // Step 4: Handle closed-curve head-tail trimming.
  // -------------------------------------------------------------------------
  if (config.debug) {
        PLOG(debug) <<
      "offset (multi-vertex): step 4 — closed-curve head-tail trim";
  }

  if (base.front() == base.back()) {
    if (lines_group.size() > 1) {
      // Multiple sub-lines: trim the tail of the last sub-line against the
      // head of the first sub-line.
      if (config.debug) {
                PLOG(debug) << 
          "offset (multi-vertex): closed curve, multiple sub-lines — trimming tail/head pair";
      }
      trimSubLinePair(
        lines_group.back(), lines_group.front(),
        maps_group.back(), maps_group.front()
      );
      assertValidBaseOffsetMap(
          maps_group.back(), "offset step 4 closed tail group");
      assertValidBaseOffsetMap(
          maps_group.front(), "offset step 4 closed head group");
    } else {
      // Single sub-line closed curve: the offset is one continuous loop that
      // crosses itself once near a sharp inward feature (e.g. an airfoil
      // trailing-edge cusp under thick offset).  Goal: split the offset loop
      // at the self-intersection, keep the "main body" branch (the larger arc
      // covering most base vertices) and discard the small loop near the
      // cusp, while keeping `lines_group[0]` and `maps_group[0]` in sync so
      // step 5 can build a valid staircase.
      if (config.debug) {
        PLOG(debug) <<
          "offset (multi-vertex): closed curve, single sub-line — self-intersection trim";
      }

      // findAllIntersections(c, c) lists each real self-intersection twice —
      // once as (a, b, u_a, u_b) and once as (b, a, u_b, u_a).
      std::vector<int>    isect_segs_back, isect_segs_front;
      std::vector<double> params_back, params_front;
      findAllIntersections(
        lines_group[0], lines_group[0],
        isect_segs_back, isect_segs_front, params_back, params_front
      );

      // Filter the raw intersection list to keep only real self-intersections.
      // findAllIntersections accepts u in [0, 1] inclusive on both sides, so
      // adjacent segments report a "crossing" at their shared vertex (u1=1,
      // u2=0).  For a closed curve the first and last segments are adjacent
      // through the closure vertex too.  Both are bookkeeping artifacts, not
      // real folds, and trimming on them produces a degenerate sub-line.
      const int n_segs = static_cast<int>(lines_group[0].size()) - 1;
      int    seg_idx_back  = -1;
      int    seg_idx_front = -1;
      double u_back        = 0.0;
      double u_front       = 0.0;
      bool   found_real    = false;
      for (int k = 0;
           k < static_cast<int>(isect_segs_back.size()); ++k) {
        int sa = isect_segs_back[k];
        int sb = isect_segs_front[k];
        int seg_lo = std::min(sa, sb);
        int seg_hi = std::max(sa, sb);
        // Same segment or adjacent: skip — vertex-sharing artifact.
        if (seg_hi - seg_lo <= 1) continue;
        // Wrap-around adjacency on a closed loop: first seg meets last seg
        // at the closure vertex.  Also a sharing artifact.
        if (seg_lo == 0 && seg_hi == n_segs - 1) continue;

        // Real self-intersection candidate.  Pick the one with the largest
        // "back" segment index — closest to the tail of the curve, mirroring
        // the original which_end=1 selection criterion.
        int    cand_back  = (sa >= sb) ? sa : sb;
        int    cand_front = (sa >= sb) ? sb : sa;
        double cand_u_back  = (sa >= sb) ? params_back[k] : params_front[k];
        double cand_u_front = (sa >= sb) ? params_front[k] : params_back[k];

        if (!found_real || cand_back > seg_idx_back
            || (cand_back == seg_idx_back && cand_u_back > u_back)) {
          seg_idx_back  = cand_back;
          seg_idx_front = cand_front;
          u_back        = cand_u_back;
          u_front       = cand_u_front;
          found_real    = true;
        }
      }

      if (config.debug) {
        PLOG(debug) <<
          "offset (multi-vertex): self-intersection: found "
          + std::to_string(isect_segs_back.size())
          + " raw candidates, real="
          + (found_real ? "yes" : "no");
      }

      if (!found_real) {
        // No real fold: the offset is a clean closed loop, but
        // computeOffsetJunctions left two distinct vertex instances at the
        // closure point — line[0] is the offset of base[0] using only
        // segment 0's neighbour info, line[N-1] is the offset using only
        // segment N-2's neighbour info.  Their geometric positions diverge
        // for any closure with a non-zero corner angle (e.g. box corners),
        // and downstream code requires lines_group[0].front() ==
        // lines_group[0].back() as the same pointer.
        //
        // Compute the proper miter junction here from the infinite-line
        // intersection of offset segments N-2 and 0 (which share base[0]
        // through the closure).  This mirrors what computeOffsetJunctions
        // does at every interior corner.  Fallback to line[0] if the two
        // segments are parallel/collinear.
        if (config.debug) {
          PLOG(debug) <<
            "offset (multi-vertex): no real self-intersection among "
            + std::to_string(isect_segs_back.size())
            + " raw candidates; computing closure miter";
        }
        if (lines_group[0].size() >= 2
            && lines_group[0].front() != lines_group[0].back()) {
          const int n = static_cast<int>(lines_group[0].size());
          PDCELVertex *seg0_a = lines_group[0][0];
          PDCELVertex *seg0_b = lines_group[0][1];
          PDCELVertex *segN_a = lines_group[0][n - 2];
          PDCELVertex *segN_b = lines_group[0][n - 1];
          double u_n, u_0;
          const bool not_parallel = calcLineIntersection2D(
              segN_a, segN_b, seg0_a, seg0_b, u_n, u_0, TOLERANCE);
          PDCELVertex *closure;
          if (not_parallel) {
            closure = new PDCELVertex(getParametricPoint(
                segN_a->point(), segN_b->point(), u_n));
            if (config.debug) {
              PLOG(debug) <<
                "offset (multi-vertex): closure miter at "
                + closure->printString()
                + " (u_n=" + std::to_string(u_n)
                + ", u_0=" + std::to_string(u_0) + ")";
            }
          } else {
            closure = lines_group[0].front();
            if (config.debug) {
              PLOG(debug) <<
                "offset (multi-vertex): closure segments parallel; "
                "reusing front vertex";
            }
          }
          lines_group[0].front() = closure;
          lines_group[0].back()  = closure;
        }
      } else {
          if (config.debug) {
            PLOG(debug) <<
              "offset (multi-vertex): self-intersection picked: front_seg="
              + std::to_string(seg_idx_front) + " u=" + std::to_string(u_front)
              + ", back_seg=" + std::to_string(seg_idx_back)
              + " u=" + std::to_string(u_back);
          }

          // Build the junction vertex independently from the line vector to
          // avoid the c1 == c2 aliasing in getIntersectionVertex (which
          // otherwise inserts into the same vector twice with the second
          // index unshifted).
          PDCELVertex *junction = new PDCELVertex(getParametricPoint(
              lines_group[0][seg_idx_front]->point(),
              lines_group[0][seg_idx_front + 1]->point(),
              u_front));

          // The closed offset loop is split by the junction into two branches:
          //   main body: junction -> v_{f+1} -> ... -> v_b -> junction
          //              spans the larger arc (e.g. around the LE)
          //   small loop: junction -> v_{b+1} -> ... -> v_f -> junction
          //               wraps the cusp (e.g. TE) — discard for inward offset.
          // We always keep the main body.
          std::vector<PDCELVertex *> new_line;
          BaseOffsetMap              new_map;
          const int kept_inner = seg_idx_back - seg_idx_front;  // count of v_{f+1}..v_b
          new_line.reserve(kept_inner + 2);
          new_map.reserve(kept_inner + 2);

          // Head junction: geometrically on segment seg_idx_front (between
          // base[f] and base[f+1] in offset space).  Assigning it base = f
          // and the trailing junction base = b+1 makes the rebuilt staircase
          // monotone end-to-end after step 5's forward fill, with both ends
          // landing in plain (base_delta=1, offset_delta=1) steps rather than
          // degenerate steps (which would require special handling at the
          // edges of the map).
          //
          // Offset values here are synthetic (sequential 0..M-1, matching
          // each entry's index in new_map).  Step 5 will overwrite them with
          // the absolute offset_vertices indices; the synthetic values exist
          // only so the rebuilt map satisfies the staircase invariant and
          // passes assertValidBaseOffsetMap right after the rebuild.
          const int head_base = maps_group[0][seg_idx_front].base;
          new_line.push_back(junction);
          new_map.push_back(BaseOffsetPair(head_base, 0));

          // Inner kept vertices v_{f+1} .. v_b.  Base indices are inherited
          // from the original map; offsets are reset to a sequential placeholder.
          for (int k = seg_idx_front + 1; k <= seg_idx_back; ++k) {
            new_line.push_back(lines_group[0][k]);
            new_map.push_back(BaseOffsetPair(
                maps_group[0][k].base,
                k - seg_idx_front));
          }

          // Tail junction: same vertex pointer as the head junction so the
          // rebuilt line is closed (front == back), matching the closed-curve
          // invariant downstream relies on.  base = b + 1 keeps the staircase
          // strictly monotone over (..., (b, X), (b+1, X+1)) at the boundary.
          const int tail_base = maps_group[0][seg_idx_back].base + 1;
          new_line.push_back(junction);
          new_map.push_back(BaseOffsetPair(
              tail_base, seg_idx_back - seg_idx_front + 1));

          // Note on ownership: the dropped vertices in the small loop branch
          // are leaked here, matching the existing behaviour of this code
          // path (the original implementation also replaced lines_group[0]
          // with a sub-vector without freeing the discarded vertices).  Fix
          // is out of scope for P0.1.
          lines_group[0] = std::move(new_line);
          maps_group[0]  = std::move(new_map);

          assertValidBaseOffsetMap(
              maps_group[0],
              "offset step 4 self-intersection rebuild");

          if (config.debug) {
            PLOG(debug) <<
              "offset (multi-vertex): single sub-line rebuilt to "
              + std::to_string(lines_group[0].size())
              + " line vertices, " + std::to_string(maps_group[0].size())
              + " map entries (head_base=" + std::to_string(head_base)
              + ", tail_base=" + std::to_string(tail_base) + ")";
          }
      }
    }
  }

  // -------------------------------------------------------------------------
  // Step 5: Join sub-lines and build output mappings.
  //
  // Vertices are shared at sub-line junctions: after trimSubLinePair, the last
  // vertex of sub-line i is the same pointer as the first vertex of sub-line i+1.
  // The inner loop therefore pushes all but the last vertex of each sub-line,
  // and the final vertex of the last sub-line is appended after the loop.
  //
  // The pre-assignment of each sub-line map entry's offset for the last vertex
  // uses
  // offset_vertices.size() *before* the corresponding push_back — it anticipates
  // the index that the next push_back will assign.  No push_back must be inserted
  // between the pre-assignment and the push_back that fills it.
  // -------------------------------------------------------------------------
  if (config.debug) {
        PLOG(debug) << 
      "offset (multi-vertex): step 5 — build output mappings";
  }

  // Use -1 as the "unmapped" sentinel. 0 is a valid offset vertex index and
  // must not be used as a sentinel (it would collide with the first vertex).
  id_pairs.assign(base.size(), BaseOffsetPair());
  for (int i = 0; i < static_cast<int>(base.size()); ++i) {
    id_pairs[i].base = i;
    id_pairs[i].offset = -1;
  }

  for (auto i = 0; i < static_cast<int>(lines_group.size()); i++) {
    for (auto j = 0; j < static_cast<int>(lines_group[i].size()) - 1; j++) {
      offset_vertices.push_back(lines_group[i][j]);
      maps_group[i][j].offset =
          static_cast<int>(offset_vertices.size()) - 1;
      id_pairs[maps_group[i][j].base].offset = maps_group[i][j].offset;
    }
    // Pre-assign the index that the last vertex of this sub-line will occupy.
    // It will be pushed by the next iteration's j=0 (as the junction that
    // starts the following sub-line), or by the push_back below for the very last.
    maps_group[i].back().offset = static_cast<int>(offset_vertices.size());
    id_pairs[maps_group[i].back().base].offset = maps_group[i].back().offset;
  }
  // Push the final vertex (last of the last sub-line), whose index was
  // pre-assigned in the loop above.
  offset_vertices.push_back(lines_group.back().back());

  // Forward-fill unmapped entries: a base vertex that was trimmed away inherits
  // the nearest offset index from its predecessor (or 0 for the very first).
  for (auto i = 0; i < static_cast<int>(id_pairs.size()); i++) {
    if (id_pairs[i].offset == -1) {
      id_pairs[i].offset =
          (i > 0 && id_pairs[i - 1].offset >= 0) ? id_pairs[i - 1].offset : 0;
    }
  }
  assertValidBaseOffsetMap(id_pairs, "offset step 5");

  // -------------------------------------------------------------------------
  // Step 5.5: Trim cross-sub-line self-intersecting loops from the joined
  //           output curve.
  //
  // trimSubLinePair can produce a degenerate 2-vertex "bridge" sub-line
  // (e.g. [junction1, junction2] at a high-curvature nose) whose surrounding
  // segments cross a neighbour sub-line once the curves are joined, creating a
  // backward loop invisible to Step 3.5 (which operates per-sub-line).
  // This step detects and removes any such cross-sub-line crossing.
  // -------------------------------------------------------------------------
  if (config.debug) {
    PLOG(debug) <<
        "offset (multi-vertex): step 5.5 — trim joined-curve self-intersections";
  }
  trimJoinedCurveSelfIntersections(offset_vertices, id_pairs);
  assertValidBaseOffsetMap(id_pairs, "offset step 5.5");

  if (config.debug) {
        PLOG(debug) << "base vertices -- base_link_to_offset_indices";
    for (auto i = 0; i < static_cast<int>(id_pairs.size()); i++) {
            PLOG(debug) << 
        "  " + std::to_string(id_pairs[i].base) + ": "
        + base[id_pairs[i].base]->printString()
        + " -- " + std::to_string(id_pairs[i].offset)
      ;
    }
  }

  return 1;
}
