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

#include "homog2d.hpp"

#include "gmsh_mod/SPoint2.h"
#include "gmsh_mod/SPoint3.h"
#include "gmsh_mod/STensor3.h"
#include "gmsh_mod/SVector3.h"

#include <cmath>
#include <cstdlib>
#include <list>
#include <string>
#include <vector>
#include <sstream>


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
  dr.normalize();
  q1 = (SVector3(p1) + dr * ds).point();
  q2 = (SVector3(p2) + dr * ds).point();
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
  // if (side == "left") {
  //   n = SVector3(1, 0, 0);
  // } else if (side == "right") {
  //   n = SVector3(-1, 0, 0);
  // }
  p = crossprod(n, t).unit() * d;
  // std::cout << "        vector p: " << p << std::endl;

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
 * @param pcurve The original baseline curve to be offset.
 * @param side The side on which to offset the curve (e.g., left or right).
 * @param distance The distance by which to offset the curve.
 * @return A pointer to the new offset baseline curve.
 */
Baseline *offsetCurve(Baseline *pcurve, int side, double distance) {
  Baseline *pcurve_off = new Baseline();

  PGeoLineSegment *psegment;
  if (pcurve->vertices().size() == 2) {
    psegment = new PGeoLineSegment(pcurve->vertices()[0], pcurve->vertices()[1]);
    psegment = offsetLineSegment(psegment, side, distance);

    pcurve_off->addPVertex(psegment->v1());
    pcurve_off->addPVertex(psegment->v2());
  } else {
    PGeoLineSegment *pprev_segment, *pfirst_segment_off;
    for (int i = 0; i < pcurve->vertices().size() - 1; ++i) {
      psegment = new PGeoLineSegment(pcurve->vertices()[i], pcurve->vertices()[i + 1]);
      psegment = offsetLineSegment(psegment, side, distance);

      if (i == 0) {
        pfirst_segment_off = psegment;
        pcurve_off->addPVertex(psegment->v1());
      }

      if (i > 0) {
        double u1, u2;
        bool not_parallel;
        not_parallel = calcLineIntersection2D(psegment, pprev_segment, u1, u2, TOLERANCE);
        if (not_parallel) {
          pcurve_off->addPVertex(psegment->getParametricVertex(u1));
        } else {
          pcurve_off->addPVertex(psegment->v1());
        }
      }

      if (i == pcurve->vertices().size() - 2) {
        pcurve_off->addPVertex(psegment->v2());
      }

      pprev_segment = psegment;
    }

    if (pcurve->vertices().front() == pcurve->vertices().back()) {
      double u1, u2;
      bool not_parallel;
      not_parallel = calcLineIntersection2D(pfirst_segment_off, pprev_segment, u1, u2, TOLERANCE);
      if (not_parallel) {
        pcurve_off->vertices()[0] = pfirst_segment_off->getParametricVertex(u1);
      }
      pcurve_off->vertices()[pcurve_off->vertices().size() - 1] =
          pcurve_off->vertices()[0];
    }
  }

  return pcurve_off;
}









/**
 * @brief Offsets a line segment defined by two vertices by a specified distance.
 *
 * This function calculates a new position for the vertices of a line segment
 * by offsetting them perpendicular to the segment direction by a given distance.
 *
 * @param[in] v1_base Pointer to the first vertex of the original line segment.
 * @param[in] v2_base Pointer to the second vertex of the original line segment.
 * @param[in] side Integer indicating the side of the offset (positive or negative).
 * @param[in] dist Distance by which to offset the line segment.
 * @param[out] v1_off Pointer to the first vertex of the offset line segment (output).
 * @param[out] v2_off Pointer to the second vertex of the offset line segment (output).
 * @return Integer indicating the success of the operation (always returns 1).
 */
int offset(PDCELVertex *v1_base, PDCELVertex *v2_base, int side, double dist,
           PDCELVertex *v1_off, PDCELVertex *v2_off) {
  SVector3 direction, normal, tangential;
  normal = SVector3(side, 0, 0);
  tangential = SVector3(v1_base->point(), v2_base->point());

  direction = crossprod(normal, tangential).unit() * dist;

  SPoint3 p1, p2;
  p1 = v1_base->point() + direction.point();
  p2 = v2_base->point() + direction.point();

  v1_off->setPoint(p1);
  v2_off->setPoint(p2);

  return 1;
}










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
  pmessage->increaseIndent();

  std::stringstream ss;

  PLOG(debug) << pmessage->message("offsetting a polyline");
  // std::cout << "\n[debug] offset" << std::endl;

  std::size_t size = base.size();
  // std::cout << "        base.size() = " << size << std::endl;
  PDCELVertex *v_tmp, *v1_tmp, *v2_tmp, *v1_prev, *v2_prev;
  // PGeoLineSegment *ls, *ls_prev, *ls_first;
  std::vector<int> link_to_tmp;

  // The base curve has only two vertices
  if (size == 2) {
    PLOG(debug) << pmessage->message("the base curve has only two vertices");

    // ls = new PGeoLineSegment(curve->vertices()[0], curve->vertices()[1]);
    v1_tmp = new PDCELVertex();
    v2_tmp = new PDCELVertex();

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

  std::vector<PDCELVertex *> vertices_tmp;

  // Initialize all vertices as un-degenerated
  // std::vector<int> degen_flags_tmp(size, 1);
  // std::vector<int> undegen_counts, degen_counts;




  //
  // Step 1: Offset each line segment, and
  // calculate intersections between every two neighbors
  PLOG(debug) << pmessage->message("1. offset each line segment");

  // PGeoLineSegment *ls_prev, *ls_first;
  for (int i = 0; i < size - 1; ++i) {
    // std::cout << "        line seg: " << i+1 << std::endl;
    v1_tmp = new PDCELVertex();
    v2_tmp = new PDCELVertex();

    offset(base[i], base[i + 1], side, dist, v1_tmp, v2_tmp);

    // std::cout << "        vertex v1_tmp = " << v1_tmp << std::endl;
    // std::cout << "        vertex v2_tmp = " << v2_tmp << std::endl;
    // std::cout << base[i] << "-" << base[i + 1] << " -> ";
    // std::cout << v1_tmp << "-" << v2_tmp << std::endl;

    link_to_tmp.push_back(i);

    if (i == 0) {
      // ls_first = ls;
      vertices_tmp.push_back(v1_tmp);
    }
    else {
      // Calculate intersection

      // std::cout << "        find intersection:" << std::endl;
      // std::cout << "        v1_prev = " << v1_prev << ", v2_prev = " << v2_prev << std::endl;
      // std::cout << "        v1_tmp = " << v1_tmp << ", v2_tmp = " << v2_tmp << std::endl;

      PLOG(debug) << pmessage->message("calculate intersection");
      PLOG(debug) << pmessage->message(
        "v1_prev: " + v1_prev->printString() + ", v2_prev: " + v2_prev->printString());
      PLOG(debug) << pmessage->message(
        "v1_tmp: " + v1_tmp->printString() + ", v2_tmp: " + v2_tmp->printString());

      // Old intersection method
      // double u1, u2;
      // bool not_parallel;
      // not_parallel =
      //     calcLineIntersection2D(v1_prev, v2_prev, v1_tmp, v2_tmp, u1, u2, TOLERANCE);
      // // std::cout << "        not_parallel = " << not_parallel << ", u1 = " <<
      // // u1 << ", u2 = " << u2 << std::endl;
      // if (not_parallel) {
      //   // vertices_tmp.push_back(ls->getParametricVertex(u1));
      //   v_tmp = new PDCELVertex(
      //       getParametricPoint(v1_prev->point(), v2_prev->point(), u1));
      //   vertices_tmp.push_back(v_tmp);
      // }
      // else {
      //   vertices_tmp.push_back(v2_prev);
      // }
      // Old intersection method (end)

      // New intersection method (h2d)
      h2d::Point2d _p1_prev(v1_prev->point2()[0], v1_prev->point2()[1]);
      h2d::Point2d _p2_prev(v2_prev->point2()[0], v2_prev->point2()[1]);
      h2d::Point2d _p1_tmp(v1_tmp->point2()[0], v1_tmp->point2()[1]);
      h2d::Point2d _p2_tmp(v2_tmp->point2()[0], v2_tmp->point2()[1]);

      ss.str("");
      ss << "Points: " << _p1_prev << " and " << _p2_prev << std::endl;
      PLOG(debug) << pmessage->message(ss.str());
      ss.str("");
      ss << "Points: " << _p1_tmp << " and " << _p2_tmp << std::endl;
      PLOG(debug) << pmessage->message(ss.str());

      h2d::Segment seg1(_p1_prev, _p2_prev);
      h2d::Segment seg2(_p1_tmp, _p2_tmp);

      ss.str("");
      ss << "Segments: " << seg1 << " and " << seg2 << std::endl;
      PLOG(debug) << pmessage->message(ss.str());

      auto res = seg1.intersects(seg2);

      ss.str("");
      ss << "res = " << res() << std::endl;
      PLOG(debug) << pmessage->message(ss.str());

      if ( res() ) {
        auto pts = res.get();

        ss.str("");
        ss << "  intersection points: " << pts << std::endl;
        PLOG(debug) << pmessage->message(ss.str());

        // Check the distance between the intersection point and the segment ends
        if (isClose(pts.getX(), pts.getY(), _p1_prev.getX(), _p1_prev.getY(), ABS_TOL, REL_TOL)) {
          vertices_tmp.push_back(v1_prev);
        }
        else if (isClose(pts.getX(), pts.getY(), _p2_prev.getX(), _p2_prev.getY(), ABS_TOL, REL_TOL)) {
          vertices_tmp.push_back(v2_prev);
        }
        else {
          v_tmp = new PDCELVertex(0, pts.getX(), pts.getY());
          vertices_tmp.push_back(v_tmp);
        }

      }
      else {
        vertices_tmp.push_back(v2_prev);
      }
      // New intersection method (h2d) (end)

      ss.str("");
      ss << "        added vertex: " << vertices_tmp.back() << std::endl;
      PLOG(debug) << pmessage->message(ss.str());

    }

    if (i == size - 2) {
      vertices_tmp.push_back(v2_tmp);
      link_to_tmp.push_back(i + 1);
    }
    // ls_prev = ls;
    v1_prev = v1_tmp;
    v2_prev = v2_tmp;
  }

  // std::cout << "        link to indices:" << std::endl;
  // for (auto i : link_to_tmp) {
  //   std::cout << "        " << i << std::endl;
  // }

  // If this curve is closed
  // if (base.front() == base.back()) {
  //   double u1, u2;
  //   bool not_parallel;
  //   PDCELVertex *v;
  //   not_parallel = calcLineIntersection2D(v1_prev, v2_prev, vertices_tmp[0],
  //                                         vertices_tmp[1], u1, u2);
  //   // std::cout << "        not_parallel = " << not_parallel << ", u1 = " << u1
  //   // << ", u2 = " << u2 << std::endl;
  //   if (not_parallel) {
  //     v_tmp = new PDCELVertex(
  //         getParametricPoint(v1_prev->point(), v2_prev->point(), u1));
  //     vertices_tmp[0] = v_tmp;
  //   }
  //   vertices_tmp[size - 1] = vertices_tmp[0];
  // }

  // std::cout << "\n        list vertices_tmp: " << std::endl;
  // for (auto i = 0; i < vertices_tmp.size(); i++) {
  //   std::cout << "        " << i << ": " << vertices_tmp[i] << std::endl;
  // }




  //
  // Step 2: Check degenerated cases (zero length and inversed line segments)
  PLOG(debug) << pmessage->message("2. check degenerated cases");

  SVector3 vec_base, vec_off;

  // Eliminate reversed direction line segments
  // The result is a group of sub-lines with correct orientation
  std::vector<std::vector<PDCELVertex *>> lines_group;
  std::vector<std::vector<int>> link_tos_group;
  // std::vector<std::vector<int>> degen_flags_group;

  // int line_i = -1;
  // int degen_i = -1;
  // int undegen_count = 0;
  // int degen_count = 0;

  // Mark if the orientation is correct
  bool check_prev{false}, check_next;
  for (int j = 0; j < size - 1; ++j) {
    // std::cout << "        index j = " << j << std::endl;
    vec_base = SVector3(base[j]->point(), base[j + 1]->point());
    vec_off = SVector3(vertices_tmp[j]->point(), vertices_tmp[j + 1]->point());
    if (dot(vec_base, vec_off) <= 0 ||
        (vec_off.normSq() < TOLERANCE * TOLERANCE)) {
      // Offset line segment is in the wrong direction or too short
      // This is the end of the current sub-line
      check_next = false;
    }
    else {
      check_next = true;
      if (!check_prev) {
        // This means that we are starting a new sub-line
        std::vector<PDCELVertex *> lines_group_i;
        // line_i += 1;
        lines_group_i.push_back(vertices_tmp[j]);
        lines_group.push_back(lines_group_i);

        std::vector<int> link_tos_group_i;
        link_tos_group_i.push_back(link_to_tmp[j]);
        link_tos_group.push_back(link_tos_group_i);
      }
      lines_group.back().push_back(vertices_tmp[j + 1]);
      link_tos_group.back().push_back(link_to_tmp[j + 1]);
    }
    // std::cout << "        check_next = " << check_next << std::endl;
    check_prev = check_next;
  }

  // std::cout << "\n        lines_group: " << lines_group.size() << std::endl;
  // for (int i = 0; i < lines_group.size(); ++i) {
  //   std::cout << "        line " << i << std::endl;
  //   for (int j = 0; j < lines_group[i].size(); ++j) {
  //     std::cout << "        " << lines_group[i][j] << " links to " <<
  //     link_tos_group[i][j] << std::endl;
  //   }
  // }





  //
  // Step 3: Find intersections between neighboring sub-lines for more than 2 lines
  PLOG(debug) << pmessage->message("3. find intersections between neighboring sub-lines");
  //
  // Here use a brute force method
  // Since the intersection should be found in a local region
  // (tail part of the previous one and head part of the next one)
  std::size_t size_i, size_i1;
  std::vector<PDCELVertex *> sline_i, sline_i1;
  PDCELVertex *v0 = nullptr, *v0_prev = lines_group[0][0];
  bool found;
  std::vector<int> link_i, link_i1;
  // int link11, link12, link21, link22;
  int i = 0, j = 0, i_prev = 0;
  int trim_index_begin_this = 0, trim_index_begin_next;
  std::vector<std::vector<PDCELVertex *> > trimmed_sublines;
  std::vector<std::vector<int> > trimmed_link_to_base_indices;

  std::vector<PDCELVertex *> tmp_trimmed_subline;
  std::vector<int> tmp_trimmed_link_to_base_index;

  int ls_i1, ls_i2;



  // Only one sub-line, no trim
  // No operations needed
  // (except possible trimming by itself at two ends, i.e., closed line)
  if (lines_group.size() == 1) {
    trimmed_sublines.push_back(lines_group[0]);
    trimmed_link_to_base_indices.push_back(link_tos_group[0]);
  }

  else if (lines_group.size() > 1) {
    for (int line_i = 0; line_i < lines_group.size() - 1; ++line_i) {
      // std::cout << "\n        find intersection between line "
      //           << line_i << " and line " << line_i + 1 << std::endl;

      tmp_trimmed_subline.clear();
      tmp_trimmed_link_to_base_index.clear();

      sline_i = lines_group[line_i];
      sline_i1 = lines_group[line_i + 1];

      link_i = link_tos_group[line_i];
      link_i1 = link_tos_group[line_i + 1];

      size_i = sline_i.size();
      size_i1 = sline_i1.size();

      found = false;
      v0 = nullptr;

      //
      double ls_u1, ls_u2;
      std::vector<int> i1s, i2s;
      std::vector<double> u1s, u2s;
      int is_new_1, is_new_2;
      int j1;

      findAllIntersections(lines_group[line_i], lines_group[line_i + 1], i1s, i2s, u1s, u2s);

      ls_u1 = getIntersectionLocation(
        lines_group[line_i], i1s, u1s, 1, 0, ls_i1, j1, pmessage);
      // std::cout << "j1 = " << j1 << std::endl;
      // ls_u2 = getIntersectionLocation(
      //   lines_group[line_i + 1], i2s, u2s, 0, 0, ls_i2);
      ls_i2 = i2s[j1];
      ls_u2 = u2s[j1];

      v0 = getIntersectionVertex(
        lines_group[line_i], lines_group[line_i + 1],
        ls_i1, ls_i2, ls_u1, ls_u2, 1, 0, 0, 0, is_new_1, is_new_2, TOLERANCE
      );

      trim(lines_group[line_i], v0, 1);
      trim(lines_group[line_i + 1], v0, 0);

      // Adjust linking indices
      std::size_t n = link_tos_group[line_i].size();
      for (auto kk = ls_i1 + 1; kk < n; kk++) {
        link_tos_group[line_i].pop_back();
      }
      n = link_tos_group[line_i + 1].size();
      for (auto kk = 0; kk < ls_i2 - 1; kk++) {
        link_tos_group[line_i + 1].erase(link_tos_group[line_i + 1].begin());
      }

      trimmed_sublines.push_back(lines_group[line_i]);
      trimmed_link_to_base_indices.push_back(link_tos_group[line_i]);
      //

    }

    // For the last subline
    trimmed_sublines.push_back(lines_group.back());
    trimmed_link_to_base_indices.push_back(link_tos_group.back());

  }

  

  // std::cout << "trimmed_sublines:" << std::endl;
  // for (auto i = 0; i < trimmed_sublines.size(); i++) {
  //   std::cout << "i = " << i << std::endl;
  //   for (auto j = 0; j < trimmed_sublines[i].size(); j++) {
  //     std::cout << " " << trimmed_sublines[i][j] << std::endl;
  //   }
  // }
  // std::cout << std::endl;
  // std::cout << "trimmed_link_to_base_indices:";
  // for (auto i = 0; i < trimmed_link_to_base_indices.size(); i++) {
  //   std::cout << "i = " << i << std::endl;
  //   for (auto j = 0; j < trimmed_link_to_base_indices[i].size(); j++) {
  //     std::cout << " " << trimmed_link_to_base_indices[i][j] << std::endl;
  //   }
  // }
  // std::cout << std::endl;




  // If this curve is closed
  if (base.front() == base.back()) {
    // std::cout << "\n[debug] handling head-tail offsets\n";

    // tmp_trimmed_subline.clear();
    // tmp_trimmed_link_to_base_index.clear();

    sline_i = lines_group.back();  // tail
    sline_i1 = lines_group.front();  // head

    link_i = link_tos_group.back();
    link_i1 = link_tos_group.front();

    // size_i = sline_i.size();
    // size_i1 = sline_i1.size();

    // found = false;

    //
    double ls_u1, ls_u2;
    std::vector<int> i1s, i2s;
    std::vector<double> u1s, u2s;
    int is_new_1, is_new_2;
    int j1;

    findAllIntersections(
      lines_group.back(), lines_group.front(), i1s, i2s, u1s, u2s
    );
    // std::cout << "\ni1 -- u1 -- i2 -- u2\n";
    // for (auto k = 0; k < i1s.size(); k++) {
    //   std::cout << i1s[k] << " -- " << u1s[k]
    //   << " -- " << i2s[k] << " -- " << u2s[k] << std::endl;
    // }

    ls_u1 = getIntersectionLocation(
      lines_group.back(), i1s, u1s, 1, 0, ls_i1, j1, pmessage);
    // ls_u2 = getIntersectionLocation(lines_group.front(), i2s, u2s, 0, 0, ls_i2);
    // std::cout << "\nj1 = " << j1 << std::endl;
    ls_i2 = i2s[j1];
    ls_u2 = u2s[j1];
    // std::cout << "\nls_i1 = " << ls_i1 << ", " << "ls_i2 = " << ls_i2 << std::endl;

    v0 = getIntersectionVertex(
      lines_group.back(), lines_group.front(),
      ls_i1, ls_i2, ls_u1, ls_u2, 1, 0, 0, 0, is_new_1, is_new_2, TOLERANCE
    );
    // std::cout << "\nv0 = " << v0 << std::endl;

    // std::cout << "\n        lines_group: " << lines_group.size() << std::endl;
    // for (int i = 0; i < lines_group.size(); ++i) {
    //   std::cout << "        line " << i << std::endl;
    //   for (int j = 0; j < lines_group[i].size(); ++j) {
    //     std::cout << "        " << j << ": " << lines_group[i][j] << " links to " <<
    //     link_tos_group[i][j] << std::endl;
    //   }
    // }

    if (lines_group.size() > 1) {
      trim(lines_group.back(), v0, 1);
      trim(lines_group.front(), v0, 0);
    }
    else {
      std::vector<PDCELVertex *> _tmp;
      bool keep = false, check;
      for (auto v : lines_group[0]) {
        check = true;
        if (check && !keep && v == v0) {
          keep = true;
          check = false;
        }
        if (keep) {
          _tmp.push_back(v);
        }
        if (check && keep && v == v0) {
          keep = false;
          check = false;
        }
      }
      lines_group[0] = _tmp;
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

    // std::cout << "        lines_group: " << lines_group.size() << std::endl;
    // for (int i = 0; i < lines_group.size(); ++i) {
    //   std::cout << "        line " << i << std::endl;
    //   for (int j = 0; j < lines_group[i].size(); ++j) {
    //     std::cout << "        " << lines_group[i][j] << " links to " <<
    //     link_tos_group[i][j] << std::endl;
    //   }
    // }

    trimmed_sublines.pop_back();
    trimmed_sublines.push_back(lines_group.back());
    trimmed_link_to_base_indices.pop_back();
    // std::cout << "link_tos_group.back() =";
    // for (auto i : link_tos_group.back()) {
    //   std::cout << " " << i;
    // }
    // std::cout << std::endl;
    trimmed_link_to_base_indices.push_back(link_tos_group.back());

    if (lines_group.size() > 1) {
      trimmed_sublines[0] = lines_group.front();
      trimmed_link_to_base_indices[0] = link_tos_group.front();
    }

  }


  // Step 4: Join all sub-lines
  PLOG(debug) << pmessage->message("4. join all sub-lines");

  // std::vector<PDCELVertex *> tmp_offset_vertices;
  // std::vector<int> tmp_offset_link_to_base_indices;
  std::vector<int> tmp_base_link_to_offset_indices(base.size(), 0);
  link_to_2 = std::vector<int>(base.size(), 0);

  for (auto i = 0; i < trimmed_sublines.size(); i++) {
    for (auto j = 0; j < trimmed_sublines[i].size() - 1; j++) {
      offset_vertices.push_back(trimmed_sublines[i][j]);
      // tmp_offset_link_to_base_indices.push_back(trimmed_link_to_base_indices[i][j]);
      // std::cout << "trimmed_link_to_base_indices[i][j] = " << trimmed_link_to_base_indices[i][j] << std::endl;
      link_to_2[trimmed_link_to_base_indices[i][j]] = offset_vertices.size() - 1;
    }
    // std::cout << "trimmed_link_to_base_indices[i].back() = " << trimmed_link_to_base_indices[i].back() << std::endl;
    link_to_2[trimmed_link_to_base_indices[i].back()] = offset_vertices.size();
  }
  offset_vertices.push_back((trimmed_sublines.back()).back());
  // tmp_offset_link_to_base_indices.push_back((trimmed_link_to_base_indices.back()).back());
  // tmp_base_link_to_offset_indices[(trimmed_link_to_base_indices.back()).back()] = tmp_offset_vertices.size() - 1;

  // std::cout << "\n[debug] base vertices -- base_link_to_offset_indices\n";
  // for (auto i = 0; i < base.size(); i++) {
  //   std::cout << "        " << i << ": " << base[i]
  //   << " -- " << link_to_2[i] << std::endl;
  // }

  for (auto i = 0; i < link_to_2.size(); i++) {
    if (i > 0 && link_to_2[i] == 0) {
      link_to_2[i] = link_to_2[i-1];
    }

    std::vector<int> id_pair_tmp{i, link_to_2[i]};
    id_pairs.push_back(id_pair_tmp);
  }

  // std::cout << "\n[debug] tmp_offset_vertices -- tmp_offset_link_to_base_indices\n";
  // for (auto i = 0; i < tmp_offset_vertices.size(); i++) {
  //   std::cout << "        " << tmp_offset_vertices[i]
  //   << " -- " << tmp_offset_link_to_base_indices[i] << std::endl;
  // }

  // std::cout << "\n[debug] offset_vertices: " << std::endl;
  // for (auto i = 0; i < offset_vertices.size(); i++) {
  //   std::cout << "        " << i << ": " << offset_vertices[i] << std::endl;
  // }

  // std::cout << "\n[debug] base vertices -- base_link_to_offset_indices\n";
  PLOG(debug) << pmessage->message("base vertices -- base_link_to_offset_indices");
  for (auto i = 0; i < id_pairs.size(); i++) {

    // ss.str("");
    // ss << "        " << i << ": " << base[i]
    // << " -- " << link_to_2[i] << std::endl;
    PLOG(debug) << pmessage->message(
      "  " + std::to_string(id_pairs[i][0]) + ": " + base[id_pairs[i][0]]->printString()
      + " -- " + std::to_string(id_pairs[i][1])
    );
  }

  pmessage->decreaseIndent();

  return 1;
}





//
// New offset function
//

/**
 * @brief Handle the case when the base curve has only two vertices.
 *
 * @param[in] base The base curve.
 * @param[in] side The side of the offset (positive or negative).
 * @param[in] dist The distance by which to offset the line segment.
 * @param[out] offset_vertices The offset curve (output).
 * @param[in] pmessage The message object.
 */
void handle_two_vertex_case(const std::vector<PDCELVertex *> &base, int side, double dist,
                         std::vector<PDCELVertex *> &offset_vertices, Message *pmessage)
{
  PLOG(debug) << pmessage->message("handling the case when the base curve has only two vertices");

  // Create two new vertices
  PDCELVertex *v1_tmp = new PDCELVertex();
  PDCELVertex *v2_tmp = new PDCELVertex();

  // Calculate the offset line segment
  offset(base[0], base[1], side, dist, v1_tmp, v2_tmp);

  // Add the two vertices to the offset curve
  PLOG(debug) << pmessage->message("adding two vertices to the offset curve");
  offset_vertices.push_back(v1_tmp);
  offset_vertices.push_back(v2_tmp);

  // Print the two vertices
  PLOG(debug) << pmessage->message("two vertices:");
  PLOG(debug) << pmessage->message("  " + v1_tmp->printString());
  PLOG(debug) << pmessage->message("  " + v2_tmp->printString());
}




/**
 * @brief Handle the intersection of two line segments.
 *
 * @param[in] v1_prev The previous vertex.
 * @param[in] v2_prev The previous vertex.
 * @param[in] v1_tmp The current vertex.
 * @param[in] v2_tmp The current vertex.
 * @param[out] vertices_tmp The vertices of the offset curve (output).
 * @param[in] pmessage The message object.
 */
void handle_segment_intersection(PDCELVertex *v1_prev, PDCELVertex *v2_prev,
                               PDCELVertex *v1_tmp, PDCELVertex *v2_tmp,
                               std::vector<PDCELVertex *> &vertices_tmp,
                               Message *pmessage)
{
  PLOG(debug) << pmessage->message("calculate intersection");
  PLOG(debug) << pmessage->message("  previous segment: " + v1_prev->printString() + " -- " + v2_prev->printString());
  PLOG(debug) << pmessage->message("  current segment: " + v1_tmp->printString() + " -- " + v2_tmp->printString());

  // Calculate the intersection of the two line segments
  h2d::Point2d p1_prev(v1_prev->point2()[0], v1_prev->point2()[1]);
  h2d::Point2d p2_prev(v2_prev->point2()[0], v2_prev->point2()[1]);
  h2d::Point2d p1_tmp(v1_tmp->point2()[0], v1_tmp->point2()[1]);
  h2d::Point2d p2_tmp(v2_tmp->point2()[0], v2_tmp->point2()[1]);

  h2d::Segment seg_prev(p1_prev, p2_prev);
  h2d::Segment seg_curr(p1_tmp, p2_tmp);
  auto res = seg_prev.intersects(seg_curr);

  PDCELVertex *v_new = nullptr;
  if (res())
  {
    // If there is an intersection, calculate the intersection point
    auto pts = res.get();
    PLOG(debug) << pmessage->message("  intersection point: (" + std::to_string(pts.getX()) + ", " + std::to_string(pts.getY()) + ")");
    if (isClose(pts.getX(), pts.getY(), p1_prev.getX(), p1_prev.getY(), ABS_TOL, REL_TOL))
    {
      v_new = v1_prev;
      PLOG(debug) << pmessage->message("  using previous vertex: " + v1_prev->printString());
    }
    else if (isClose(pts.getX(), pts.getY(), p2_prev.getX(), p2_prev.getY(), ABS_TOL, REL_TOL))
    {
      v_new = v2_prev;
      PLOG(debug) << pmessage->message("  using previous vertex: " + v2_prev->printString());
    }
    else
    {
      // Create a new vertex at the intersection point
      v_new = new PDCELVertex(0, pts.getX(), pts.getY());
      PLOG(debug) << pmessage->message("  created new vertex: " + v_new->printString());
    }
  }
  else
  {
    // If there is no intersection, use the previous vertex
    v_new = v2_prev;
    PLOG(debug) << pmessage->message("  using previous vertex: " + v2_prev->printString());
  }

  // Add the new vertex to the offset curve
  vertices_tmp.push_back(v_new);
}



/**
 * @brief Process all line segments in the base curve and calculate the offset curve.
 *
 * @param[in] base The base curve.
 * @param[in] side The side of the offset (positive or negative).
 * @param[in] dist The distance by which to offset the line segment.
 * @param[out] vertices_tmp The vertices of the offset curve (output).
 * @param[in] pmessage The message object.
 */
void process_all_segments(const std::vector<PDCELVertex *> &base, int side, double dist,
                        std::vector<PDCELVertex *> &vertices_tmp, Message *pmessage)
{
  PDCELVertex *v1_prev = nullptr, *v2_prev = nullptr;

  // Iterate over all line segments in the base curve
  for (int i = 0; i < base.size() - 1; ++i)
  {
    PLOG(debug) << pmessage->message("processing line segment " + std::to_string(i+1) + "/" + std::to_string(base.size()-1));
    PDCELVertex *v1_tmp = new PDCELVertex();
    PDCELVertex *v2_tmp = new PDCELVertex();

    // Calculate the offset curve for the current line segment
    PLOG(debug) << pmessage->message("  calculating offset curve");
    offset(base[i], base[i + 1], side, dist, v1_tmp, v2_tmp);

    // If it is the first line segment, add the first vertex to the output
    if (i == 0)
    {
      PLOG(debug) << pmessage->message("  adding first vertex to output");
      vertices_tmp.push_back(v1_tmp);
    }
    else
    {
      // Handle the intersection of the two line segments
      PLOG(debug) << pmessage->message("  handling intersection of two line segments");
      handle_segment_intersection(v1_prev, v2_prev, v1_tmp, v2_tmp, vertices_tmp, pmessage);
    }

    // If it is the last line segment, add the last vertex to the output
    if (i == base.size() - 2)
    {
      PLOG(debug) << pmessage->message("  adding last vertex to output");
      vertices_tmp.push_back(v2_tmp);
    }

    // Store the current vertices for the next iteration
    v1_prev = v1_tmp;
    v2_prev = v2_tmp;
  }
}

/**
 * @brief Groups the vertices of the offset curve into sublines.
 * @details
 * This function takes a base curve and a set of vertices, and groups the
 * vertices into sublines such that the orientation of the sublines is the
 * same as the orientation of the base curve.
 * @param[in] base The base curve.
 * @param[in] vertices_tmp The vertices of the offset curve.
 * @param[out] lines_group The grouped sublines.
 * @param[in] pmessage The message object.
 */
void group_valid_sublines(const std::vector<PDCELVertex *> &base,
                        const std::vector<PDCELVertex *> &vertices_tmp,
                        std::vector<std::vector<PDCELVertex *>> &lines_group,
                        Message *pmessage)
{
  PLOG(debug) << pmessage->message("grouping valid sublines");
  SVector3 vec_base, vec_off;
  bool check_prev = false;
  for (int j = 0; j < base.size() - 1; ++j)
  {
    vec_base = SVector3(base[j]->point(), base[j + 1]->point());
    vec_off = SVector3(vertices_tmp[j]->point(), vertices_tmp[j + 1]->point());

    PLOG(debug) << pmessage->message("  checking line segment " + std::to_string(j+1) + "/" + std::to_string(base.size()-1));
    // Check if the orientation of the subline is correct
    bool check_next = (dot(vec_base, vec_off) > 0) && (vec_off.normSq() >= TOLERANCE * TOLERANCE);

    PLOG(debug) << pmessage->message("  check_next: " + std::to_string(check_next));
    // If the orientation is correct, add the vertex to the current subline
    if (check_next)
    {
      if (!check_prev)
      {
        // Start a new subline
        lines_group.push_back({vertices_tmp[j]});
        PLOG(debug) << pmessage->message("  starting new subline: " + std::to_string(lines_group.size()));
      }
      // Add the vertex to the current subline
      lines_group.back().push_back(vertices_tmp[j + 1]);
      PLOG(debug) << pmessage->message("  adding vertex to current subline: " + std::to_string(lines_group.back().size()));
    }
    // Remember the orientation of the previous subline
    check_prev = check_next;
  }
}

/**
 * @brief Trim and connect sublines in the offset curve.
 *
 * This function takes a list of sublines and trims them to remove any
 * intersections between adjacent sublines. The trimmed sublines are then
 * added to the output list.
 *
 * @param[in] lines_group The list of sublines.
 * @param[out] trimmed_sublines The trimmed sublines.
 * @param[in] pmessage The message object.
 */
void trim_and_connect_sublines(std::vector<std::vector<PDCELVertex *>> &lines_group,
                            std::vector<std::vector<PDCELVertex *>> &trimmed_sublines,
                            Message *pmessage)
{
  for (int line_i = 0; line_i < lines_group.size() - 1; ++line_i)
  {
    auto &sline_i = lines_group[line_i];
    auto &sline_i1 = lines_group[line_i + 1];

    PLOG(debug) << pmessage->message("trimming and connecting sublines " + std::to_string(line_i) + " and " + std::to_string(line_i + 1));

    // Find intersections and trim sublines
    std::vector<int> i1s, i2s;
    std::vector<double> u1s, u2s;
    findAllIntersections(sline_i, sline_i1, i1s, i2s, u1s, u2s);

    int ls_i1, j1;
    double ls_u1 = getIntersectionLocation(sline_i, i1s, u1s, 1, 0, ls_i1, j1, pmessage);

    int ls_i2 = i2s[j1];
    double ls_u2 = u2s[j1];

    int is_new_1, is_new_2;
    PDCELVertex *v0 = getIntersectionVertex(sline_i, sline_i1, ls_i1, ls_i2, ls_u1, ls_u2,
                                            1, 0, 0, 0, is_new_1, is_new_2, TOLERANCE);

    PLOG(debug) << pmessage->message("  trimming subline " + std::to_string(line_i) + " at " + std::to_string(ls_u1));
    trim(sline_i, v0, 1);

    PLOG(debug) << pmessage->message("  trimming subline " + std::to_string(line_i + 1) + " at " + std::to_string(ls_u2));
    trim(sline_i1, v0, 0);

    trimmed_sublines.push_back(sline_i);
  }
  trimmed_sublines.push_back(lines_group.back());
}




/**
 * @brief Trim and connect sublines in the offset curve for a closed curve.
 *
 * This function takes a list of sublines and trims them to remove any
 * intersections between adjacent sublines. The trimmed sublines are then
 * added to the output list.
 *
 * Note that this function is only used for closed curves.
 *
 * @param[in] base The base curve.
 * @param[in] lines_group The list of sublines.
 * @param[out] trimmed_sublines The trimmed sublines.
 * @param[in] pmessage The message object.
 */
void process_closed_curve_case(const std::vector<PDCELVertex *> &base,
                            std::vector<std::vector<PDCELVertex *>> &lines_group,
                            std::vector<std::vector<PDCELVertex *>> &trimmed_sublines,
                            Message *pmessage)
{
  // Get the last and first subline
  auto &sline_last = lines_group.back();
  auto &sline_first = lines_group.front();

  PLOG(debug) << pmessage->message("  processing last and first sublines");

  // Find intersections between last and first sublines
  std::vector<int> i1s, i2s;
  std::vector<double> u1s, u2s;
  findAllIntersections(sline_last, sline_first, i1s, i2s, u1s, u2s);

  // Get the intersection location
  int ls_i1, j1;
  double ls_u1 = getIntersectionLocation(sline_last, i1s, u1s, 1, 0, ls_i1, j1, pmessage);
  int ls_i2 = i2s[j1];
  double ls_u2 = u2s[j1];

  PLOG(debug) << pmessage->message("  intersection location: " + std::to_string(ls_u1));

  // Create the intersection vertex
  int is_new_1, is_new_2;
  PDCELVertex *v0 = getIntersectionVertex(sline_last, sline_first, ls_i1, ls_i2, ls_u1, ls_u2,
                                          1, 0, 0, 0, is_new_1, is_new_2, TOLERANCE);

  PLOG(debug) << pmessage->message("  intersection vertex: " + v0->printString());

  // Trim the sublines
  trim(sline_last, v0, 1);
  trim(sline_first, v0, 0);

  PLOG(debug) << pmessage->message("  trimmed sublines");

  // Update trimmed sublines
  if (!trimmed_sublines.empty())
  {
    trimmed_sublines.back() = sline_last;
    trimmed_sublines.front() = sline_first;
  }
}






/**
 * @brief Offset a polyline.
 *
 * @param[in] base The base polyline.
 * @param[in] side The side to offset to (0 for left, 1 for right).
 * @param[in] dist The offset distance.
 * @param[out] offset The offset polyline.
 * @param[in] pmessage The message object.
 * @return 1 if successful, 0 otherwise.
 */
int offset_2(const std::vector<PDCELVertex *> &base, int side, double dist,
           std::vector<PDCELVertex *> &offset_vertices, Message *pmessage) {
  pmessage->increaseIndent();
  PLOG(debug) << pmessage->message("offsetting a polyline");

  if (base.size() == 2) {
      handle_two_vertex_case(base, side, dist, offset_vertices, pmessage);
      pmessage->decreaseIndent();
      return 1;
  }

  // Process all segments
  std::vector<PDCELVertex*> vertices_tmp;
  process_all_segments(base, side, dist, vertices_tmp, pmessage);

  PLOG(debug) << pmessage->message("  processed all segments");

  // Group valid sublines
  std::vector<std::vector<PDCELVertex*>> lines_group;
  group_valid_sublines(base, vertices_tmp, lines_group, pmessage);

  PLOG(debug) << pmessage->message("  grouped valid sublines");

  // Trim and connect sublines
  std::vector<std::vector<PDCELVertex*>> trimmed_sublines;
  if (lines_group.size() > 1) {
      trim_and_connect_sublines(lines_group, trimmed_sublines,
                              pmessage);
  } else {
      trimmed_sublines = lines_group;
  }

  PLOG(debug) << pmessage->message("  trimmed and connected sublines");

  // Process closed curve case
  if (!base.empty() && base.front() == base.back()) {
      process_closed_curve_case(base, lines_group, trimmed_sublines,
                              pmessage);
  }

  PLOG(debug) << pmessage->message("  processed closed curve case");

  pmessage->decreaseIndent();
  return 1;
}
