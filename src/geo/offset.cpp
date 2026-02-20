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
    PGeoLineSegment *ls_prev = nullptr, *ls_first_off;
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
 * is added to vertices_tmp.  The caller receives a flat list of junction
 * vertices and the corresponding link_to_tmp index vector.
 *
 * @param base        Original polyline vertices (size >= 3 guaranteed by caller).
 * @param side        Offset side (+1 / -1).
 * @param dist        Offset distance.
 * @param link_to_tmp Output: index in base[] each vertices_tmp entry corresponds to.
 * @param pmessage    Logger.
 * @return            The junction-vertex list.
 */
static std::vector<PDCELVertex *> computeOffsetJunctions(
    const std::vector<PDCELVertex *> &base,
    int side, double dist,
    std::vector<int> &link_to_tmp,
    Message *pmessage) {

  std::size_t size = base.size();
  link_to_tmp.clear();

  std::vector<PDCELVertex *> vertices_tmp;
  std::vector<PDCELVertex *> allocated_tmp;

  PDCELVertex *v1_tmp = nullptr, *v2_tmp = nullptr;
  PDCELVertex *v1_prev = nullptr, *v2_prev = nullptr;
  PDCELVertex *v_tmp = nullptr;

  for (int i = 0; i < static_cast<int>(size) - 1; ++i) {
    v1_tmp = new PDCELVertex();
    v2_tmp = new PDCELVertex();
    allocated_tmp.push_back(v1_tmp);
    allocated_tmp.push_back(v2_tmp);

    offset(base[i], base[i + 1], side, dist, v1_tmp, v2_tmp);

    link_to_tmp.push_back(i);

    if (i == 0) {
      vertices_tmp.push_back(v1_tmp);
    } else {
      // New intersection method (h2d)
      h2d::Point2d _p1_prev(v1_prev->point2()[0], v1_prev->point2()[1]);
      h2d::Point2d _p2_prev(v2_prev->point2()[0], v2_prev->point2()[1]);
      h2d::Point2d _p1_tmp(v1_tmp->point2()[0], v1_tmp->point2()[1]);
      h2d::Point2d _p2_tmp(v2_tmp->point2()[0], v2_tmp->point2()[1]);

      if (config.debug) {
        std::ostringstream oss;
        oss << "find intersection:\n"
            << "        v1_prev = " << v1_prev << ", v2_prev = " << v2_prev << "\n"
            << "        v1_tmp = " << v1_tmp << ", v2_tmp = " << v2_tmp << "\n"
            << "        prev segment: " << _p1_prev << " - " << _p2_prev << "\n"
            << "        tmp  segment: " << _p1_tmp  << " - " << _p2_tmp;
        PLOG(debug) << pmessage->message(oss.str());
      }

      h2d::Segment seg1(_p1_prev, _p2_prev);
      h2d::Segment seg2(_p1_tmp, _p2_tmp);
      auto res = seg1.intersects(seg2);
      if (res()) {
        auto pts = res.get();

        if (config.debug) {
          std::ostringstream oss;
          oss << "intersection found: " << pts;
          PLOG(debug) << pmessage->message(oss.str());
        }

        if (isClose(pts.getX(), pts.getY(), _p1_prev.getX(), _p1_prev.getY(), ABS_TOL, REL_TOL)) {
          vertices_tmp.push_back(v1_prev);
        } else if (isClose(pts.getX(), pts.getY(), _p2_prev.getX(), _p2_prev.getY(), ABS_TOL, REL_TOL)) {
          vertices_tmp.push_back(v2_prev);
        } else {
          v_tmp = new PDCELVertex(0, pts.getX(), pts.getY());
          vertices_tmp.push_back(v_tmp);
        }
      } else {
        vertices_tmp.push_back(v2_prev);
      }

      if (config.debug) {
        std::ostringstream oss;
        oss << "added vertex: " << vertices_tmp.back();
        PLOG(debug) << pmessage->message(oss.str());
      }
    }

    if (i == static_cast<int>(size) - 2) {
      vertices_tmp.push_back(v2_tmp);
      link_to_tmp.push_back(i + 1);
    }

    v1_prev = v1_tmp;
    v2_prev = v2_tmp;
  }

  // Free per-segment vertices not retained in vertices_tmp.
  for (auto vv : allocated_tmp) {
    if (std::find(vertices_tmp.begin(), vertices_tmp.end(), vv) == vertices_tmp.end()) {
      delete vv;
    }
  }

  return vertices_tmp;
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
 * @param link_to_tmp    Index mapping from computeOffsetJunctions.
 * @param lines_group    Output: each element is one valid sub-line.
 * @param link_tos_group Output: base indices corresponding to each sub-line vertex.
 */
static void groupValidSegments(
    const std::vector<PDCELVertex *> &base,
    const std::vector<PDCELVertex *> &vertices_tmp,
    const std::vector<int> &link_to_tmp,
    std::vector<std::vector<PDCELVertex *>> &lines_group,
    std::vector<std::vector<int>> &link_tos_group) {

  std::size_t size = base.size();
  SVector3 vec_base, vec_off;

  bool check_prev{false}, check_next;
  for (int j = 0; j < static_cast<int>(size) - 1; ++j) {
    vec_base = SVector3(base[j]->point(), base[j + 1]->point());
    vec_off = SVector3(vertices_tmp[j]->point(), vertices_tmp[j + 1]->point());
    if (dot(vec_base, vec_off) <= 0 ||
        (vec_off.normSq() < TOLERANCE * TOLERANCE)) {
      check_next = false;
    } else {
      check_next = true;
      if (!check_prev) {
        lines_group.push_back(std::vector<PDCELVertex *>{vertices_tmp[j]});
        link_tos_group.push_back(std::vector<int>{link_to_tmp[j]});
      }
      lines_group.back().push_back(vertices_tmp[j + 1]);
      link_tos_group.back().push_back(link_to_tmp[j + 1]);
    }
    check_prev = check_next;
  }
}

/**
 * @brief Trim a pair of adjacent offset sub-lines at their mutual intersection.
 *
 * Finds the intersection of sline_a's tail region with sline_b's head region,
 * inserts the intersection vertex into both curves, trims sline_a from the
 * intersection toward its tail and sline_b from the intersection toward its
 * head, and adjusts the base-index link vectors accordingly.
 *
 * @param sline_a  Tail sub-line (modified in place).
 * @param sline_b  Head sub-line (modified in place).
 * @param link_a   Base-index links for sline_a (modified in place).
 * @param link_b   Base-index links for sline_b (modified in place).
 * @param pmessage Logger.
 */
static void trimSubLinePair(
    std::vector<PDCELVertex *> &sline_a,
    std::vector<PDCELVertex *> &sline_b,
    std::vector<int> &link_a,
    std::vector<int> &link_b,
    Message *pmessage) {

  std::vector<int> i1s, i2s;
  std::vector<double> u1s, u2s;
  int ls_i1, ls_i2;
  double ls_u1, ls_u2;
  int is_new_1, is_new_2;
  int j1;

  findAllIntersections(sline_a, sline_b, i1s, i2s, u1s, u2s);

  if (i1s.empty()) {
    PLOG(warning) << pmessage->message(
      "no intersection found between consecutive offset sub-lines; joining at endpoints");
    return;
  }

  ls_u1 = getIntersectionLocation(sline_a, i1s, u1s, 1, 0, ls_i1, j1);
  if (j1 < 0 || j1 >= static_cast<int>(i2s.size())) {
    PLOG(warning) << pmessage->message(
      "intersection index j1 out of range; skipping trim for this sub-line pair");
    return;
  }

  ls_i2 = i2s[j1];
  ls_u2 = u2s[j1];

  PDCELVertex *v0 = getIntersectionVertex(
    sline_a, sline_b,
    ls_i1, ls_i2, ls_u1, ls_u2, 1, 0, 0, 0, is_new_1, is_new_2, TOLERANCE
  );

  trim(sline_a, v0, 1);
  trim(sline_b, v0, 0);

  // Adjust linking indices
  std::size_t n = link_a.size();
  for (auto kk = ls_i1 + 1; kk < n; kk++) {
    link_a.pop_back();
  }
  n = link_b.size();
  for (auto kk = 0; kk < ls_i2 - 1; kk++) {
    link_b.erase(link_b.begin());
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
 * @param base The input vector of base vertices to be offset.
 * @param side The side on which to offset the vertices (e.g., left or right).
 * @param dist The distance by which to offset the vertices.
 * @param offset_vertices The output vector of offset vertices.
 * @param link_to_2 The output vector linking base vertices to offset vertices.
 * @param id_pairs The output vector of pairs of indices linking base vertices to offset vertices.
 * @param pmessage Pointer to a Message object for logging and debugging.
 * @return int Returns 1 on successful offsetting of vertices.
 */
int offset(const std::vector<PDCELVertex *> &base, int side, double dist,
           std::vector<PDCELVertex *> &offset_vertices, std::vector<int> &link_to_2,
           std::vector<std::vector<int>> &id_pairs, Message *pmessage) {

  std::size_t size = base.size();

  // Fast path: 2-vertex polyline
  if (size == 2) {
    PDCELVertex *v1_tmp = new PDCELVertex();
    PDCELVertex *v2_tmp = new PDCELVertex();

    offset(base[0], base[1], side, dist, v1_tmp, v2_tmp);

    offset_vertices.push_back(v1_tmp);
    offset_vertices.push_back(v2_tmp);

    link_to_2.push_back(0);
    link_to_2.push_back(1);

    std::vector<int> i0_tmp{0, 0}, i1_tmp{1, 1};
    id_pairs.push_back(i0_tmp);
    id_pairs.push_back(i1_tmp);

    return 1;
  }

  // Step 1: Compute offset junction vertices
  std::vector<int> link_to_tmp;
  std::vector<PDCELVertex *> vertices_tmp =
      computeOffsetJunctions(base, side, dist, link_to_tmp, pmessage);

  // Step 2: Group valid offset segments into sub-lines
  std::vector<std::vector<PDCELVertex *>> lines_group;
  std::vector<std::vector<int>> link_tos_group;
  groupValidSegments(base, vertices_tmp, link_to_tmp, lines_group, link_tos_group);

  // Step 3: Trim adjacent sub-line pairs at their mutual intersection
  std::vector<std::vector<PDCELVertex *>> trimmed_sublines;
  std::vector<std::vector<int>> trimmed_link_to_base_indices;

  if (lines_group.size() == 1) {
    trimmed_sublines.push_back(lines_group[0]);
    trimmed_link_to_base_indices.push_back(link_tos_group[0]);
  } else {
    for (int line_i = 0; line_i < static_cast<int>(lines_group.size()) - 1; ++line_i) {
      trimSubLinePair(
        lines_group[line_i], lines_group[line_i + 1],
        link_tos_group[line_i], link_tos_group[line_i + 1],
        pmessage
      );
      trimmed_sublines.push_back(lines_group[line_i]);
      trimmed_link_to_base_indices.push_back(link_tos_group[line_i]);
    }
    trimmed_sublines.push_back(lines_group.back());
    trimmed_link_to_base_indices.push_back(link_tos_group.back());
  }

  // Step 4: Handle closed-curve head-tail trimming
  if (base.front() == base.back()) {
    if (lines_group.size() > 1) {
      // Trim the tail of the last sub-line against the head of the first sub-line.
      trimSubLinePair(
        lines_group.back(), lines_group.front(),
        link_tos_group.back(), link_tos_group.front(),
        pmessage
      );
      trimmed_sublines.back() = lines_group.back();
      trimmed_link_to_base_indices.back() = link_tos_group.back();
      trimmed_sublines[0] = lines_group.front();
      trimmed_link_to_base_indices[0] = link_tos_group.front();
    } else {
      // Single sub-line closed curve: find the self-intersection of the offset
      // curve and extract the valid sub-sequence.
      std::vector<int> i1s, i2s;
      std::vector<double> u1s, u2s;
      int ls_i1, ls_i2;
      double ls_u1, ls_u2;
      int is_new_1, is_new_2;
      int j1;

      findAllIntersections(lines_group.back(), lines_group.front(), i1s, i2s, u1s, u2s);

      if (i1s.empty()) {
        PLOG(warning) << pmessage->message(
          "no intersection found between head and tail offset sub-lines; skipping head-tail trim");
      } else {
        ls_u1 = getIntersectionLocation(
          lines_group.back(), i1s, u1s, 1, 0, ls_i1, j1);
        if (j1 < 0 || j1 >= static_cast<int>(i2s.size())) {
          PLOG(warning) << pmessage->message(
            "head-tail intersection index j1 out of range; skipping head-tail trim");
        } else {
          ls_i2 = i2s[j1];
          ls_u2 = u2s[j1];

          PDCELVertex *v0 = getIntersectionVertex(
            lines_group.back(), lines_group.front(),
            ls_i1, ls_i2, ls_u1, ls_u2, 1, 0, 0, 0, is_new_1, is_new_2, TOLERANCE
          );

          // Extract the sub-sequence from the first to the second occurrence of v0.
          std::vector<PDCELVertex *> &lg0 = lines_group[0];
          auto it_begin = std::find(lg0.begin(), lg0.end(), v0);
          if (it_begin != lg0.end()) {
            auto it_end = std::find(std::next(it_begin), lg0.end(), v0);
            if (it_end != lg0.end()) {
              lg0 = std::vector<PDCELVertex *>(it_begin, std::next(it_end));
            } else {
              // v0 appears only once: keep from v0 to the end
              lg0 = std::vector<PDCELVertex *>(it_begin, lg0.end());
            }
          } else {
            PLOG(warning) << pmessage->message(
              "closed curve: intersection vertex not found in single sub-line");
          }

          // Adjust linking indices
          std::size_t n = link_tos_group.back().size();
          for (auto kk = ls_i1 + 1; kk < n; kk++) {
            link_tos_group.back().pop_back();
          }
          n = link_tos_group.front().size();
          for (auto kk = 0; kk < ls_i2 - 1; kk++) {
            link_tos_group.front().erase(link_tos_group.front().begin());
          }
        }
      }

      trimmed_sublines.back() = lines_group.back();
      trimmed_link_to_base_indices.back() = link_tos_group.back();
    }
  }

  // Step 5: Join sub-lines and build output mappings
  // Use -1 as the "unmapped" sentinel. 0 is a valid offset vertex index and
  // must not be used as a sentinel (it would collide with the first vertex).
  link_to_2 = std::vector<int>(base.size(), -1);

  for (auto i = 0; i < static_cast<int>(trimmed_sublines.size()); i++) {
    for (auto j = 0; j < static_cast<int>(trimmed_sublines[i].size()) - 1; j++) {
      offset_vertices.push_back(trimmed_sublines[i][j]);
      link_to_2[trimmed_link_to_base_indices[i][j]] =
          static_cast<int>(offset_vertices.size()) - 1;
    }
    link_to_2[trimmed_link_to_base_indices[i].back()] =
        static_cast<int>(offset_vertices.size());
  }
  offset_vertices.push_back(trimmed_sublines.back().back());

  // Forward-fill unmapped entries: a base vertex that was trimmed away inherits
  // the nearest offset index from its predecessor (or 0 for the very first).
  for (auto i = 0; i < static_cast<int>(link_to_2.size()); i++) {
    if (link_to_2[i] == -1) {
      link_to_2[i] = (i > 0 && link_to_2[i - 1] >= 0) ? link_to_2[i - 1] : 0;
    }
    std::vector<int> id_pair_tmp{i, link_to_2[i]};
    id_pairs.push_back(id_pair_tmp);
  }

  PLOG(debug) << pmessage->message("base vertices -- base_link_to_offset_indices");
  for (auto i = 0; i < static_cast<int>(id_pairs.size()); i++) {
    PLOG(debug) << pmessage->message(
      "  " + std::to_string(id_pairs[i][0]) + ": " + base[id_pairs[i][0]]->printString()
      + " -- " + std::to_string(id_pairs[i][1])
    );
  }

  return 1;
}
