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

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <list>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

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
  MESSAGE_SCOPE(g_msg);

  if (!v1_off || !v2_off) {
    PLOG(error) << g_msg->message("offset: null output vertex pointer");
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
 * is added to the returned list.  The caller receives a flat list of junction
 * vertices and the corresponding link_to_tmp index vector.
 *
 * @param base        Original polyline vertices (must have at least 3 vertices;
 *                    an error is logged and an empty list is returned otherwise).
 * @param side        Offset side (+1 / -1).
 * @param dist        Offset distance.
 * @param link_to_tmp Output: index in base[] each junction entry corresponds to.
 * @return            The junction-vertex list.
 *
 * @note Ownership: every vertex in the returned list is heap-allocated by this
 *       function.  The caller is responsible for deleting them when no longer
 *       needed.
 */
static std::vector<PDCELVertex *> computeOffsetJunctions(
    const std::vector<PDCELVertex *> &base,
    int side, double dist,
    std::vector<int> &link_to_tmp
  ) {

  MESSAGE_SCOPE(g_msg);

  std::size_t size = base.size();
  link_to_tmp.clear();

  if (size < 3) {
    PLOG(error) << g_msg->message(
      "computeOffsetJunctions: base must have at least 3 vertices (got "
      + std::to_string(size) + "); returning empty junction list");
    return {};
  }

  if (config.debug) {
    std::ostringstream oss;
    oss << "computeOffsetJunctions: " << size << " base vertices"
        << ", side=" << side << ", dist=" << dist;
    PLOG(debug) << g_msg->message(oss.str());
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

  link_to_tmp.reserve(size);

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
      PLOG(debug) << g_msg->message(oss.str());
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
      PLOG(debug) << g_msg->message(oss.str());
    }

    // Record that the junction vertex we are about to add corresponds to base
    // vertex i (the shared vertex between segment i-1 and segment i).
    link_to_tmp.push_back(i);

    if (i == 0) {
      // First segment: no previous segment to intersect with, so the junction
      // is simply the start endpoint of this offset segment.
      junctions.push_back(cur_start);
      if (config.debug) {
        PLOG(debug) << g_msg->message("  first segment: junction = cur_start (no prev segment)");
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
        PLOG(debug) << g_msg->message(oss.str());
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
          PLOG(debug) << g_msg->message(oss.str());
        }

        // If the intersection coincides (within tolerance) with an already-
        // allocated endpoint, reuse that vertex to avoid creating a duplicate.
        // Otherwise allocate a new vertex at the exact intersection point.
        if (isClose(isect_pt.getX(), isect_pt.getY(), prev_p1.getX(), prev_p1.getY(), ABS_TOL, REL_TOL)) {
          junctions.push_back(prev_start);
          if (config.debug) {
            PLOG(debug) << g_msg->message("  snapped to prev_start (within tolerance)");
          }
        } else if (isClose(isect_pt.getX(), isect_pt.getY(), prev_p2.getX(), prev_p2.getY(), ABS_TOL, REL_TOL)) {
          junctions.push_back(prev_end);
          if (config.debug) {
            PLOG(debug) << g_msg->message("  snapped to prev_end (within tolerance)");
          }
        } else {
          PDCELVertex *v_junction = new PDCELVertex(0, isect_pt.getX(), isect_pt.getY());
          local_allocs.push_back(v_junction);
          junctions.push_back(v_junction);
          if (config.debug) {
            PLOG(debug) << g_msg->message("  new junction vertex allocated at intersection point");
          }
        }
      } else {
        // Parallel or collinear segments: h2d returns no intersection point.
        // For strictly parallel (non-collinear) segments, no miter junction
        // exists; for collinear (overlapping) segments, prev_end is the shared
        // endpoint and is the geometrically correct junction in both cases.
        junctions.push_back(prev_end);
        if (config.debug) {
          PLOG(debug) << g_msg->message("  segments parallel/collinear: fell back to prev_end");
        }
      }

      if (config.debug) {
        std::ostringstream oss;
        oss << "  junction[" << junctions.size() - 1 << "] = " << junctions.back();
        PLOG(debug) << g_msg->message(oss.str());
      }
    }

    // After processing the last segment, append its tail endpoint as the final
    // junction vertex and record its correspondence with base vertex i+1.
    if (i == static_cast<int>(size) - 2) {
      junctions.push_back(cur_end);
      link_to_tmp.push_back(i + 1);
      if (config.debug) {
        std::ostringstream oss;
        oss << "  last segment: appended tail junction[" << junctions.size() - 1
            << "] = " << cur_end;
        PLOG(debug) << g_msg->message(oss.str());
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
    PLOG(debug) << g_msg->message(oss.str());
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
 * @param link_to_tmp    Index mapping from computeOffsetJunctions.
 *                       Must have the same size as base.
 * @param lines_group    Output: cleared on entry; each element is one valid sub-line.
 * @param link_tos_group Output: cleared on entry; base indices for each sub-line vertex.
 */
static void groupValidSegments(
    const std::vector<PDCELVertex *> &base,
    const std::vector<PDCELVertex *> &vertices_tmp,
    const std::vector<int> &link_to_tmp,
    std::vector<std::vector<PDCELVertex *>> &lines_group,
    std::vector<std::vector<int>> &link_tos_group) {

  MESSAGE_SCOPE(g_msg);

  std::size_t size = base.size();

  // Enforce size consistency: vertices_tmp and link_to_tmp must match base.
  if (vertices_tmp.size() != size || link_to_tmp.size() != size) {
    PLOG(error) << g_msg->message(
      "groupValidSegments: size mismatch — base=" + std::to_string(size)
      + ", vertices_tmp=" + std::to_string(vertices_tmp.size())
      + ", link_to_tmp=" + std::to_string(link_to_tmp.size())
      + "; skipping grouping");
    return;
  }

  // Clear outputs so this function always produces a fresh result regardless
  // of what the caller passed in.
  lines_group.clear();
  link_tos_group.clear();

  if (config.debug) {
    std::ostringstream oss;
    oss << "groupValidSegments: " << size << " base vertices, "
        << static_cast<int>(size) - 1 << " segment(s) to check";
    PLOG(debug) << g_msg->message(oss.str());
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
      PLOG(debug) << g_msg->message(oss.str());
    }

    if (cur_valid) {
      if (!prev_valid) {
        // This segment starts a new run of valid segments — open a new sub-line.
        lines_group.push_back(std::vector<PDCELVertex *>{vertices_tmp[j]});
        link_tos_group.push_back(std::vector<int>{link_to_tmp[j]});
        if (config.debug) {
          std::ostringstream oss;
          oss << "    opened sub-line " << lines_group.size() - 1
              << " at junction vertex " << j;
          PLOG(debug) << g_msg->message(oss.str());
        }
      }
      // Extend the current sub-line with the tail vertex of this segment.
      lines_group.back().push_back(vertices_tmp[j + 1]);
      link_tos_group.back().push_back(link_to_tmp[j + 1]);
    } else if (prev_valid) {
      // This segment is invalid and the previous one was valid — the run just
      // ended; the current sub-line is now complete.
      if (config.debug) {
        std::ostringstream oss;
        oss << "    closed sub-line " << lines_group.size() - 1
            << " before segment " << j;
        PLOG(debug) << g_msg->message(oss.str());
      }
    }

    prev_valid = cur_valid;
  }

  if (config.debug) {
    std::ostringstream oss;
    oss << "groupValidSegments: produced " << lines_group.size() << " sub-line(s)";
    PLOG(debug) << g_msg->message(oss.str());
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
 * @param link_tail  Base-index links for tail_line (modified in place).
 * @param link_head  Base-index links for head_line (modified in place).
 */
static void trimSubLinePair(
    std::vector<PDCELVertex *> &tail_line,
    std::vector<PDCELVertex *> &head_line,
    std::vector<int> &link_tail,
    std::vector<int> &link_head
  ) {

  MESSAGE_SCOPE(g_msg);

  if (config.debug) {
    std::ostringstream oss;
    oss << "trimSubLinePair:"
        << " tail_line=" << tail_line.size() << " vertices"
        << ", head_line=" << head_line.size() << " vertices";
    PLOG(debug) << g_msg->message(oss.str());
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
    PLOG(warning) << g_msg->message(
      "no intersection found between consecutive offset sub-lines; joining at endpoints");
    return;
  }

  if (config.debug) {
    std::ostringstream oss;
    oss << "  found " << isect_segs_tail.size() << " intersection(s)";
    PLOG(debug) << g_msg->message(oss.str());
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
    PLOG(warning) << g_msg->message(
      "intersection index out of range; skipping trim for this sub-line pair");
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
    PLOG(debug) << g_msg->message(oss.str());
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
    /*which_end_1=*/1,    /*which_end_2=*/0,
    /*inner_only_1=*/0,   /*inner_only_2=*/0,
    is_new_tail, is_new_head, TOLERANCE
  );

  if (!junction) {
    PLOG(error) << g_msg->message(
      "getIntersectionVertex returned null; skipping trim for this sub-line pair");
    return;
  }

  if (config.debug) {
    std::ostringstream oss;
    oss << "  junction vertex: " << junction
        << " (is_new_tail=" << is_new_tail
        << ", is_new_head=" << is_new_head << ")";
    PLOG(debug) << g_msg->message(oss.str());
  }

  // Step 4: trim the curves at the junction vertex.
  //   trim(..., 1): keep from the beginning up to junction; discard the tail.
  //   trim(..., 0): keep from junction to the end; discard the head.
  trim(tail_line, junction, 1);
  trim(head_line, junction, 0);

  if (config.debug) {
    std::ostringstream oss;
    oss << "  after trim:"
        << " tail_line=" << tail_line.size() << " vertices"
        << ", head_line=" << head_line.size() << " vertices";
    PLOG(debug) << g_msg->message(oss.str());
  }

  // Step 5: adjust the base-index link vectors to match the trimmed curves.
  //
  // tail_line now ends at junction, which was at segment seg_idx_tail.
  // Keep only the first seg_idx_tail + 1 link entries (0 … seg_idx_tail).
  link_tail.resize(seg_idx_tail + 1);
  //
  // head_line now starts at junction, which was at segment seg_idx_head.
  // Drop the first seg_idx_head - 1 entries (those before the junction segment).
  if (seg_idx_head > 1) {
    link_head.erase(link_head.begin(), link_head.begin() + (seg_idx_head - 1));
  }

  if (config.debug) {
    std::ostringstream oss;
    oss << "  after link adjust:"
        << " link_tail=" << link_tail.size() << " entries"
        << ", link_head=" << link_head.size() << " entries";
    PLOG(debug) << g_msg->message(oss.str());
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
 * @param link_to_2        Output: link_to_2[i] is the index of the offset vertex
 *                         that corresponds to base vertex i.  Overwritten on entry.
 * @param id_pairs         Output: list of {base_idx, offset_idx} pairs.  Cleared on entry.
 * @return int Returns 1 on success, 0 if the input is degenerate or all segments
 *             are invalid.
 */
int offset(const std::vector<PDCELVertex *> &base, int side, double dist,
           std::vector<PDCELVertex *> &offset_vertices, std::vector<int> &link_to_2,
           std::vector<std::vector<int>> &id_pairs) {

  MESSAGE_SCOPE(g_msg);

  std::size_t size = base.size();

  // Precondition: at least 2 vertices are required to form a segment.
  if (size < 2) {
    PLOG(error) << g_msg->message(
      "offset (multi-vertex): base must have at least 2 vertices (got "
      + std::to_string(size) + "); returning empty result");
    return 0;
  }

  // Clear output vectors so callers can safely pass non-empty containers.
  offset_vertices.clear();
  id_pairs.clear();

  if (config.debug) {
    PLOG(debug) << g_msg->message(
      "offset (multi-vertex): size=" + std::to_string(size)
      + " side=" + std::to_string(side)
      + " dist=" + std::to_string(dist));
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

    link_to_2 = {0, 1};

    id_pairs.push_back({0, 0});
    id_pairs.push_back({1, 1});

    return 1;
  }

  // -------------------------------------------------------------------------
  // Step 1: Compute offset junction vertices.
  // -------------------------------------------------------------------------
  if (config.debug) {
    PLOG(debug) << g_msg->message(
      "offset (multi-vertex): step 1 — compute junction vertices");
  }

  std::vector<int> link_to_tmp;
  std::vector<PDCELVertex *> vertices_tmp =
      computeOffsetJunctions(base, side, dist, link_to_tmp);

  // -------------------------------------------------------------------------
  // Step 2: Group valid offset segments into sub-lines.
  // -------------------------------------------------------------------------
  if (config.debug) {
    PLOG(debug) << g_msg->message(
      "offset (multi-vertex): step 2 — group valid segments");
  }

  std::vector<std::vector<PDCELVertex *>> lines_group;
  std::vector<std::vector<int>> link_tos_group;
  groupValidSegments(base, vertices_tmp, link_to_tmp, lines_group, link_tos_group);

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
    PLOG(warning) << g_msg->message(
      "offset (multi-vertex): all offset segments are invalid; returning empty result");
    return 0;
  }

  if (config.debug) {
    PLOG(debug) << g_msg->message(
      "offset (multi-vertex): " + std::to_string(lines_group.size())
      + " sub-line(s) after grouping");
  }

  // -------------------------------------------------------------------------
  // Step 3: Trim adjacent sub-line pairs at their mutual intersection.
  //
  // trimSubLinePair modifies both sub-lines in place, so lines_group and
  // link_tos_group are the authoritative post-trim state.  No separate
  // "trimmed" copy is maintained — Step 5 reads lines_group directly.
  // -------------------------------------------------------------------------
  if (config.debug) {
    PLOG(debug) << g_msg->message(
      "offset (multi-vertex): step 3 — trim adjacent sub-line pairs");
  }

  if (lines_group.size() > 1) {
    for (int line_i = 0; line_i < static_cast<int>(lines_group.size()) - 1; ++line_i) {
      if (config.debug) {
        PLOG(debug) << g_msg->message(
          "offset (multi-vertex): trimming sub-line pair "
          + std::to_string(line_i) + "/" + std::to_string(line_i + 1));
      }
      trimSubLinePair(
        lines_group[line_i], lines_group[line_i + 1],
        link_tos_group[line_i], link_tos_group[line_i + 1]
      );
    }
  }

  // -------------------------------------------------------------------------
  // Step 4: Handle closed-curve head-tail trimming.
  // -------------------------------------------------------------------------
  if (config.debug) {
    PLOG(debug) << g_msg->message(
      "offset (multi-vertex): step 4 — closed-curve head-tail trim");
  }

  if (base.front() == base.back()) {
    if (lines_group.size() > 1) {
      // Multiple sub-lines: trim the tail of the last sub-line against the
      // head of the first sub-line.
      if (config.debug) {
        PLOG(debug) << g_msg->message(
          "offset (multi-vertex): closed curve, multiple sub-lines — trimming tail/head pair");
      }
      trimSubLinePair(
        lines_group.back(), lines_group.front(),
        link_tos_group.back(), link_tos_group.front()
      );
    } else {
      // Single sub-line closed curve: find the self-intersection of the offset
      // curve and extract the valid sub-sequence between the two junction points.
      if (config.debug) {
        PLOG(debug) << g_msg->message(
          "offset (multi-vertex): closed curve, single sub-line — self-intersection trim");
      }

      // isect_segs_back / isect_segs_front: segment indices of each intersection
      // on the back (== front, single-sub-line case) and front views of the curve.
      // params_back / params_front: parametric locations (0–1) on those segments.
      std::vector<int>    isect_segs_back, isect_segs_front;
      std::vector<double> params_back, params_front;
      findAllIntersections(
        lines_group.back(), lines_group.front(),
        isect_segs_back, isect_segs_front, params_back, params_front
      );

      if (config.debug) {
        PLOG(debug) << g_msg->message(
          "offset (multi-vertex): self-intersection: found "
          + std::to_string(isect_segs_back.size()) + " intersection(s)");
      }

      if (isect_segs_back.empty()) {
        PLOG(warning) << g_msg->message(
          "offset (multi-vertex): no self-intersection found; skipping head-tail trim");
      } else {
        // Pick the intersection closest to the tail end of the back sub-line.
        //   which_end=1  — select the intersection nearest the tail (end) of the curve.
        //   inner_only=0 — consider all intersections, not only interior ones.
        // Outputs: seg_idx_back (segment index), isect_idx (position in the
        // intersection arrays for the chosen intersection), u_back (parametric location).
        int    seg_idx_back, isect_idx;
        double u_back = getIntersectionLocation(
          lines_group.back(), isect_segs_back, params_back,
          /*which_end=*/1, /*inner_only=*/0,
          seg_idx_back, isect_idx
        );

        if (config.debug) {
          PLOG(debug) << g_msg->message(
            "offset (multi-vertex): back sub-line intersection: seg="
            + std::to_string(seg_idx_back) + " u=" + std::to_string(u_back));
        }

        if (isect_idx < 0 || isect_idx >= static_cast<int>(isect_segs_front.size())) {
          PLOG(warning) << g_msg->message(
            "offset (multi-vertex): self-intersection index out of range; "
            "skipping head-tail trim");
        } else {
          // Retrieve the matching intersection data on the front sub-line side.
          int    seg_idx_front = isect_segs_front[isect_idx];
          double u_front       = params_front[isect_idx];

          if (config.debug) {
            PLOG(debug) << g_msg->message(
              "offset (multi-vertex): front sub-line intersection: seg="
              + std::to_string(seg_idx_front) + " u=" + std::to_string(u_front));
          }

          // Obtain or create the junction vertex at the self-intersection point.
          //   which_end_1=1  — use the intersection nearest the tail of the back curve.
          //   which_end_2=0  — use the intersection nearest the head of the front curve.
          //   inner_only_1/2=0 — consider all intersections, not only interior ones.
          // is_new_back / is_new_front: output flags — 1 if a new vertex was
          //   heap-allocated for that curve, 0 if an existing endpoint was reused.
          //   In either case getIntersectionVertex inserts the junction into both
          //   curves, so no additional ownership action is needed here.
          int is_new_back, is_new_front;
          PDCELVertex *junction = getIntersectionVertex(
            lines_group.back(), lines_group.front(),
            seg_idx_back, seg_idx_front, u_back, u_front,
            /*which_end_1=*/1,    /*which_end_2=*/0,
            /*inner_only_1=*/0,   /*inner_only_2=*/0,
            is_new_back, is_new_front, TOLERANCE
          );

          if (config.debug) {
            PLOG(debug) << g_msg->message(
              "offset (multi-vertex): junction="
              + (junction ? junction->printString() : std::string("null"))
              + " (is_new_back=" + std::to_string(is_new_back)
              + ", is_new_front=" + std::to_string(is_new_front) + ")");
          }

          if (!junction) {
            PLOG(error) << g_msg->message(
              "offset (multi-vertex): getIntersectionVertex returned null; "
              "skipping self-intersection trim");
          } else {
            // Extract the sub-sequence from the first to the second occurrence of
            // junction (the self-intersection forms a loop; keep the loop portion).
            std::vector<PDCELVertex *> &lg0 = lines_group[0];
            auto it_begin = std::find(lg0.begin(), lg0.end(), junction);

            if (it_begin == lg0.end()) {
              PLOG(warning) << g_msg->message(
                "offset (multi-vertex): junction vertex not found in single "
                "sub-line; skipping trim");
            } else {
              auto it_end = std::find(std::next(it_begin), lg0.end(), junction);
              if (it_end != lg0.end()) {
                // junction appears twice: keep the loop [it_begin, it_end]
                lg0 = std::vector<PDCELVertex *>(it_begin, std::next(it_end));
              } else {
                // junction appears only once: keep from junction to the end
                lg0 = std::vector<PDCELVertex *>(it_begin, lg0.end());
              }

              if (config.debug) {
                PLOG(debug) << g_msg->message(
                  "offset (multi-vertex): single sub-line trimmed to "
                  + std::to_string(lg0.size()) + " vertices");
              }
            }

            // Adjust link index vector for the back sub-line:
            // keep only entries [0 .. seg_idx_back] (the portion up to the junction).
            link_tos_group.back().resize(seg_idx_back + 1);

            // Adjust link index vector for the front sub-line:
            // drop entries that correspond to segments before the junction.
            if (seg_idx_front > 1) {
              link_tos_group.front().erase(
                link_tos_group.front().begin(),
                link_tos_group.front().begin() + (seg_idx_front - 1)
              );
            }

            if (config.debug) {
              PLOG(debug) << g_msg->message(
                "offset (multi-vertex): link indices adjusted after self-intersection trim");
            }
          }
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
  // The pre-assignment of link_to_2 for the last vertex of each sub-line uses
  // offset_vertices.size() *before* the corresponding push_back — it anticipates
  // the index that the next push_back will assign.  No push_back must be inserted
  // between the pre-assignment and the push_back that fills it.
  // -------------------------------------------------------------------------
  if (config.debug) {
    PLOG(debug) << g_msg->message(
      "offset (multi-vertex): step 5 — build output mappings");
  }

  // Use -1 as the "unmapped" sentinel. 0 is a valid offset vertex index and
  // must not be used as a sentinel (it would collide with the first vertex).
  link_to_2 = std::vector<int>(base.size(), -1);

  for (auto i = 0; i < static_cast<int>(lines_group.size()); i++) {
    for (auto j = 0; j < static_cast<int>(lines_group[i].size()) - 1; j++) {
      offset_vertices.push_back(lines_group[i][j]);
      link_to_2[link_tos_group[i][j]] =
          static_cast<int>(offset_vertices.size()) - 1;
    }
    // Pre-assign the index that the last vertex of this sub-line will occupy.
    // It will be pushed by the next iteration's j=0 (as the junction that
    // starts the following sub-line), or by the push_back below for the very last.
    link_to_2[link_tos_group[i].back()] =
        static_cast<int>(offset_vertices.size());
  }
  // Push the final vertex (last of the last sub-line), whose index was
  // pre-assigned in the loop above.
  offset_vertices.push_back(lines_group.back().back());

  // Forward-fill unmapped entries: a base vertex that was trimmed away inherits
  // the nearest offset index from its predecessor (or 0 for the very first).
  for (auto i = 0; i < static_cast<int>(link_to_2.size()); i++) {
    if (link_to_2[i] == -1) {
      link_to_2[i] = (i > 0 && link_to_2[i - 1] >= 0) ? link_to_2[i - 1] : 0;
    }
    id_pairs.push_back({i, link_to_2[i]});
  }

  if (config.debug) {
    PLOG(debug) << g_msg->message("base vertices -- base_link_to_offset_indices");
    for (auto i = 0; i < static_cast<int>(id_pairs.size()); i++) {
      PLOG(debug) << g_msg->message(
        "  " + std::to_string(id_pairs[i][0]) + ": " + base[id_pairs[i][0]]->printString()
        + " -- " + std::to_string(id_pairs[i][1])
      );
    }
  }

  return 1;
}
