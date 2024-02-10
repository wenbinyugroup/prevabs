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

#include "gmsh/SPoint2.h"
#include "gmsh/SPoint3.h"
#include "gmsh/STensor3.h"
#include "gmsh/SVector3.h"

#include <cmath>
#include <cstdlib>
#include <list>
#include <string>
#include <vector>


void offsetLineSegment(SPoint3 &p1, SPoint3 &p2, SVector3 &dr, double &ds,
                       SPoint3 &q1, SPoint3 &q2) {
  dr.normalize();
  q1 = (SVector3(p1) + dr * ds).point();
  q2 = (SVector3(p2) + dr * ds).point();
}











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











PGeoLineSegment *offsetLineSegment(PGeoLineSegment *ls, SVector3 &offset) {
  PDCELVertex *v1, *v2;
  v1 = new PDCELVertex(ls->v1()->point() + offset.point());
  v2 = new PDCELVertex(ls->v2()->point() + offset.point());

  return new PGeoLineSegment(v1, v2);
}











Baseline *offsetCurve(Baseline *curve, int side, double distance) {
  // std::cout << "[debug] offsetCurve" << std::endl;
  Baseline *curve_off = new Baseline();

  PGeoLineSegment *ls;
  if (curve->vertices().size() == 2) {
    ls = new PGeoLineSegment(curve->vertices()[0], curve->vertices()[1]);
    ls = offsetLineSegment(ls, side, distance);

    curve_off->addPVertex(ls->v1());
    curve_off->addPVertex(ls->v2());
  } else {
    PGeoLineSegment *ls_prev, *ls_first_off;
    for (int i = 0; i < curve->vertices().size() - 1; ++i) {
      ls = new PGeoLineSegment(curve->vertices()[i], curve->vertices()[i + 1]);
      ls = offsetLineSegment(ls, side, distance);

      if (i == 0) {
        ls_first_off = ls;
        curve_off->addPVertex(ls->v1());
      }

      if (i > 0) {
        // Calculate intersection
        double u1, u2;
        bool not_parallel;
        not_parallel = calcLineIntersection2D(ls, ls_prev, u1, u2);
        if (not_parallel) {
          curve_off->addPVertex(ls->getParametricVertex(u1));
        } else {
          curve_off->addPVertex(ls->v1());
        }
      }

      if (i == curve->vertices().size() - 2) {
        curve_off->addPVertex(ls->v2());
      }

      ls_prev = ls;
    }

    if (curve->vertices().front() == curve->vertices().back()) {
      double u1, u2;
      bool not_parallel;
      PDCELVertex *v;
      not_parallel = calcLineIntersection2D(ls_first_off, ls_prev, u1, u2);
      if (not_parallel) {
        curve_off->vertices()[0] = ls_first_off->getParametricVertex(u1);
      }
      curve_off->vertices()[curve_off->vertices().size() - 1] =
          curve_off->vertices()[0];
    }
  }

  // std::cout << "        baseline curve_off" << std::endl;
  // for (auto v : curve_off->vertices()) {
  //   std::cout << "        " << v << std::endl;
  // }

  return curve_off;
}









int offset(PDCELVertex *v1_base, PDCELVertex *v2_base, int side, double dist,
           PDCELVertex *v1_off, PDCELVertex *v2_off) {
  // std::cout << "[debug] offset a line segment:" << std::endl;
  // std::cout << "        " << v1_base << std::endl;
  // std::cout << "        " << v2_base << std::endl;

  SVector3 dir, n, t;
  n = SVector3(side, 0, 0);
  t = SVector3(v1_base->point(), v2_base->point());

  dir = crossprod(n, t).unit() * dist;
  // std::cout << "        vector dir = " << dir << std::endl;

  SPoint3 p1, p2;
  p1 = v1_base->point() + dir.point();
  p2 = v2_base->point() + dir.point();
  // std::cout << "        point p1 = " << p1 << std::endl;

  v1_off->setPoint(p1);
  v2_off->setPoint(p2);
  // v1_off = new PDCELVertex(p1[0], p1[1], p1[2]);
  // v2_off = new PDCELVertex(p2[0], p2[1], p2[2]);

  return 1;
}










int offset(const std::vector<PDCELVertex *> &base, int side, double dist,
           std::vector<PDCELVertex *> &offset_vertices, std::vector<int> &link_to_2,
           std::vector<std::vector<int>> &id_pairs, Message *pmessage) {
  // std::cout << "\n[debug] offset" << std::endl;

  int size = base.size();
  // std::cout << "        base.size() = " << size << std::endl;
  PDCELVertex *v_tmp, *v1_tmp, *v2_tmp, *v1_prev, *v2_prev;
  // PGeoLineSegment *ls, *ls_prev, *ls_first;
  std::vector<int> link_to_tmp;

  if (size == 2) {
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
  // std::cout << "\n[debug] offset: step 1" << std::endl;

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
      double u1, u2;
      bool not_parallel;
      // std::cout << "        find intersection:" << std::endl;
      // std::cout << "        v1_prev = " << v1_prev << ", v2_prev = " <<
      // v2_prev << std::endl; std::cout << "        v1_tmp = " << v1_tmp << ",
      // v2_tmp = " << v2_tmp << std::endl;
      not_parallel =
          calcLineIntersection2D(v1_prev, v2_prev, v1_tmp, v2_tmp, u1, u2);
      // std::cout << "        not_parallel = " << not_parallel << ", u1 = " <<
      // u1 << ", u2 = " << u2 << std::endl;
      if (not_parallel) {
        // vertices_tmp.push_back(ls->getParametricVertex(u1));
        v_tmp = new PDCELVertex(
            getParametricPoint(v1_prev->point(), v2_prev->point(), u1));
        vertices_tmp.push_back(v_tmp);
      }
      else {
        vertices_tmp.push_back(v2_prev);
      }
      // std::cout << "        added vertex: " << vertices_tmp.back() <<
      // std::endl;
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
  // std::cout << "\n[debug] offset: step 2" << std::endl;
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
  // std::cout << "\n[debug] offset: step 3" << std::endl;
  //
  // Here use a brute force method
  // Since the intersection should be found in a local region
  // (tail part of the previous one and head part of the next one)
  int size_i, size_i1;
  std::vector<PDCELVertex *> sline_i, sline_i1;
  PDCELVertex *v11, *v12, *v21, *v22, *v0 = nullptr, *v0_prev = lines_group[0][0];
  bool found;
  std::vector<int> link_i, link_i1;
  // int link11, link12, link21, link22;
  int i = 0, j = 0, i_prev = 0;
  int trim_index_begin_this = 0, trim_index_begin_next, trim_index_end_this;
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
      int n = link_tos_group[line_i].size();
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
    int n = link_tos_group.back().size();
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
  // std::cout << "\n[debug] offset: step 4" << std::endl;
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
    // std::cout << "        " << i << ": " << base[i]
    // << " -- " << link_to_2[i] << std::endl;
    PLOG(debug) << pmessage->message(
      "  " + std::to_string(id_pairs[i][0]) + ": " + base[id_pairs[i][0]]->printString()
      + " -- " + std::to_string(id_pairs[i][1])
    );
  }

  return 1;
}

