#include "geo.hpp"
#include "utilities.hpp"
#include "plog.hpp"
#include <string>


bool calcLineIntersection2D(
  const double &l1p1x, const double &l1p1y, const double &l1p2x, const double &l1p2y,
  const double &l2p1x, const double &l2p1y, const double &l2p2x, const double &l2p2y,
  double &u1, double &u2, const double &tol
  ) {
  // std::cout << "[debug] point l1p1: " << l1p1 << std::endl;
  // std::cout << "[debug] point l1p2: " << l1p2 << std::endl;
  // std::cout << "[debug] point l2p1: " << l2p1 << std::endl;
  // std::cout << "[debug] point l2p2: " << l2p2 << std::endl;
  // std::cout << l1p1 << "-" << l1p2 << " x " << l2p1 << "-" << l2p2 << std::endl;

  // x1 = l1p1[0], y1 = l1p1[1]
  // x2 = l1p2[0], y2 = l1p2[1]
  // x3 = l2p1[0], y3 = l2p1[1]
  // x4 = l2p2[0], y4 = l2p2[1]

  double dnm;
  dnm = (l1p1x - l1p2x) * (l2p1y - l2p2y) -
        (l1p1y - l1p2y) * (l2p1x - l2p2x);
  // std::cout << "dnm = " << dnm << std::endl;
  if (fabs(dnm) <= tol) {
    return false;
  }

  u1 = (l1p1x - l2p1x) * (l2p1y - l2p2y) -
       (l1p1y - l2p1y) * (l2p1x - l2p2x);
  u1 = u1 / dnm;
  // std::cout << "u1 = " << u1 << std::endl;

  u2 = -(l1p1x - l1p2x) * (l1p1y - l2p1y) +
       (l1p1y - l1p2y) * (l1p1x - l2p1x);
  u2 = u2 / dnm;
  // std::cout << "u2 = " << u2 << std::endl;

  return true;
}










// template <typename P2>
bool calcLineIntersection2D(
  const PGeoPoint2 &l1p1, const PGeoPoint2 &l1p2, const PGeoPoint2 &l2p1, const PGeoPoint2 &l2p2,
  double &u1, double &u2, const double &tol) {
  return calcLineIntersection2D(
    l1p1[0], l1p1[1], l1p2[0], l1p2[1],
    l2p1[0], l2p1[1], l2p2[0], l2p2[1],
    u1, u2, tol
  );
}










// template <typename P3>
bool calcLineIntersection2D(
  const PGeoPoint3 &l1p1, const PGeoPoint3 &l1p2, const PGeoPoint3 &l2p1, const PGeoPoint3 &l2p2,
  double &u1, double &u2, const int &plane, const double &tol) {
  int d1, d2;
  if (plane == 0) {
    d1 = 1;
    d2 = 2;
  } else if (plane == 1) {
    d1 = 2;
    d2 = 0;
  } else if (plane == 2) {
    d1 = 0;
    d2 = 1;
  }
  return calcLineIntersection2D(
    l1p1[d1], l1p1[d2], l1p2[d1], l1p2[d2],
    l2p1[d1], l2p1[d2], l2p2[d1], l2p2[d2],
    u1, u2, tol
  );
}









bool calcLineIntersection2D(SPoint2 l1p1, SPoint2 l1p2, SPoint2 l2p1,
                            SPoint2 l2p2, double &u1, double &u2) {
  // std::cout << "[debug] point l1p1: " << l1p1 << std::endl;
  // std::cout << "[debug] point l1p2: " << l1p2 << std::endl;
  // std::cout << "[debug] point l2p1: " << l2p1 << std::endl;
  // std::cout << "[debug] point l2p2: " << l2p2 << std::endl;
  // std::cout << l1p1 << "-" << l1p2 << " x " << l2p1 << "-" << l2p2 << std::endl;

  // x1 = l1p1[0], y1 = l1p1[1]
  // x2 = l1p2[0], y2 = l1p2[1]
  // x3 = l2p1[0], y3 = l2p1[1]
  // x4 = l2p2[0], y4 = l2p2[1]

  double dnm;
  dnm = (l1p1[0] - l1p2[0]) * (l2p1[1] - l2p2[1]) -
        (l1p1[1] - l1p2[1]) * (l2p1[0] - l2p2[0]);
  // std::cout << "dnm = " << dnm << std::endl;
  if (fabs(dnm) <= TOLERANCE) {
    return false;
  }

  u1 = (l1p1[0] - l2p1[0]) * (l2p1[1] - l2p2[1]) -
       (l1p1[1] - l2p1[1]) * (l2p1[0] - l2p2[0]);
  u1 = u1 / dnm;
  // std::cout << "u1 = " << u1 << std::endl;

  u2 = -(l1p1[0] - l1p2[0]) * (l1p1[1] - l2p1[1]) +
       (l1p1[1] - l1p2[1]) * (l1p1[0] - l2p1[0]);
  u2 = u2 / dnm;
  // std::cout << "u2 = " << u2 << std::endl;

  return true;
}










bool calcLineIntersection2D(SPoint3 l1p1, SPoint3 l1p2, SPoint3 l2p1,
                            SPoint3 l2p2, double &u1, double &u2, int &plane) {
  int d1, d2;
  if (plane == 0) {
    d1 = 1;
    d2 = 2;
  } else if (plane == 1) {
    d1 = 2;
    d2 = 0;
  } else if (plane == 2) {
    d1 = 0;
    d2 = 1;
  }

  SPoint2 l1p, l1q, l2p, l2q;
  l1p = SPoint2(l1p1[d1], l1p1[d2]);
  l1q = SPoint2(l1p2[d1], l1p2[d2]);
  l2p = SPoint2(l2p1[d1], l2p1[d2]);
  l2q = SPoint2(l2p2[d1], l2p2[d2]);

  return calcLineIntersection2D(l1p, l1q, l2p, l2q, u1, u2);
}










bool calcLineIntersection2D(PDCELVertex *ls1v1, PDCELVertex *ls1v2,
                            PDCELVertex *ls2v1, PDCELVertex *ls2v2, double &u1,
                            double &u2) {
  return calcLineIntersection2D(ls1v1->point2(), ls1v2->point2(),
                                ls2v1->point2(), ls2v2->point2(), u1, u2);
}










bool calcLineIntersection2D(PGeoLineSegment *ls1, PGeoLineSegment *ls2,
                            double &u1, double &u2) {
  SPoint2 ls1p1, ls1p2, ls2p1, ls2p2;
  ls1p1 = ls1->v1()->point2();
  ls1p2 = ls1->v2()->point2();
  ls2p1 = ls2->v1()->point2();
  ls2p2 = ls2->v2()->point2();

  return calcLineIntersection2D(ls1p1, ls1p2, ls2p1, ls2p2, u1, u2);
}








int intersect(PGeoLineSegment *subject, PGeoLineSegment *tool,
              PDCELVertex *intersect) {
  int result;
  double us, ut;
  bool not_parallel;

  not_parallel = calcLineIntersection2D(subject, tool, us, ut);
  if (not_parallel) {
    if (us >= 0 && us <= 1) {
      intersect = subject->getParametricVertex(us);
      result = 1;
    } else if (us < 0) {
      result = -1;
    } else if (us > 1) {
      result = 2;
    }
  } else {
    result = 0; // Parallel case
  }

  return result;
}









PDCELHalfEdge *findCurvesIntersection(
  std::vector<PDCELVertex *> vertices, PDCELHalfEdgeLoop *hel,
  int end, int &ls_i, double &u1, double &u2, const double &tol,
  Message *pmessage
  ) {
  pmessage->increaseIndent();

  PLOG(debug) << pmessage->message("in function: findCurvesIntersection");

  PDCELHalfEdge *he = nullptr;

  std::vector<PDCELVertex *> tmp_ls; // temporary line segment
  int ls_i_prev;
  PDCELHalfEdge *hei = hel->incidentEdge();
  std::vector<int> c_is, t_is;  // curve indices, tool indices
  std::vector<double> c_us, t_us;  // curve parametric locations, tool parametric locations
  int j0;
  double tmp_c_u, tmp_t_u;  // temporary parametric locations

  if (end == 0) {  // find the intersection at the beginning
    u1 = -INF;
    ls_i_prev = -1;
  }
  else if (end == 1) {  // find the intersection at the end
    u1 = INF;
    ls_i_prev = convertSizeTToInt(vertices.size());
  }

  // Iterate through all line segments of the half edge loop
  do {
    PLOG(debug) << pmessage->message("----------");

    tmp_ls.clear();
    tmp_ls.push_back(hei->source());
    tmp_ls.push_back(hei->target());

    // std::cout << "tmp_ls: " << tmp_ls[0] << ", " << tmp_ls[1] << std::endl;
    PLOG(debug) << pmessage->message(
      "line segment of the half edge loop (tmp_ls): " + tmp_ls[0]->printString() + " -- " + tmp_ls[1]->printString()
      );

    c_is.clear();
    t_is.clear();
    c_us.clear();
    t_us.clear();


    // Find all intersections between the line segment and the curve
    findAllIntersections(
      vertices, tmp_ls, c_is, t_is, c_us, t_us
    );

    PLOG(debug) << pmessage->message("all intersections");
    PLOG(debug) << pmessage->message("curve index (i) -- param loc (u) | tool index (i) -- param loc (u)");

    for (auto k = 0; k < c_is.size(); k++) {
      PLOG(debug) << pmessage->message(
        std::to_string(c_is[k]) + " -- " + std::to_string(c_us[k]) + " | "
        + std::to_string(t_is[k]) + " -- " + std::to_string(t_us[k]));
    }


    // If there is at least one intersection
    if (c_is.size() > 0) {

      // Find the intersection that is the closest to the expected end
      if (end == 0) {
        tmp_c_u = getIntersectionLocation(
          vertices, c_is, c_us, 1, 0, ls_i, j0, pmessage
        );
      }
      else if (end == 1) {
        tmp_c_u = getIntersectionLocation(
          vertices, c_is, c_us, 0, 0, ls_i, j0, pmessage
        );
      }
      tmp_t_u = t_us[j0];

      PLOG(debug) << pmessage->message("closest intersection to end " + std::to_string(end));
      PLOG(debug) << pmessage->message("curve segment index (ls_i) = " + std::to_string(ls_i));
      PLOG(debug) << pmessage->message("prev curve segment index (ls_i_prev) = " + std::to_string(ls_i_prev));
      PLOG(debug) << pmessage->message("curve param loc (tmp_c_u) = " + std::to_string(tmp_c_u));
      PLOG(debug) << pmessage->message("tool param loc (tmp_t_u) = " + std::to_string(tmp_t_u));
      PLOG(debug) << pmessage->message(
        "curve segment: v11 = " + vertices[ls_i]->printString() + " -> "
        + "v12 = " + vertices[ls_i+1]->printString()
      );
      PLOG(debug) << pmessage->message(
        "tool segment: v21 = " + tmp_ls[0]->printString() + " -> "
        + "v22 = " + tmp_ls[1]->printString()
      );
      // PLOG(debug) << pmessage->message("tol = " + std::to_string(tol));
      // PLOG(debug) << pmessage->message("number of vertices of the curve = " + std::to_string(vertices.size()));

      bool update = false;

      // If the intersection is within the tool segment
      PLOG(debug) << pmessage->message("u1 = " + std::to_string(u1));
      PLOG(debug) << pmessage->message("tol = " + std::to_string(tol));
      if (fabs(tmp_t_u) <= tol || (tmp_t_u > 0 && tmp_t_u < 1) || fabs(1 - tmp_t_u) <= tol) {

        // If want the intersection closer to the beginning
        if (end == 0) {

          // If the intersection is before the first vertex
          if (ls_i == 0 && tmp_c_u < 0 && tmp_c_u > u1) {
            update = true;
          }
          else if (fabs(tmp_c_u) <= tol || (tmp_c_u > 0 && tmp_c_u < 1) || fabs(1 - tmp_c_u) <= tol) {
            if (ls_i > ls_i_prev) {
              update = true;
            }
            else if (ls_i == ls_i_prev && tmp_c_u > u1) {
              update = true;
            }
          }

          // if (
          //   (ls_i == 0 && tmp_c_u < 0 && tmp_c_u > u1)  // before the first vertex
          //   || ((fabs(tmp_c_u) <= tol || (tmp_c_u > 0 && tmp_c_u < 1) || fabs(1 - tmp_c_u) <= tol) && ls_i > ls_i_prev)  // inner line segment
          //   || ((fabs(tmp_c_u) <= tol || (tmp_c_u > 0 && tmp_c_u < 1) || fabs(1 - tmp_c_u) <= tol) && ls_i == ls_i_prev && tmp_c_u > u1) // same line segment but inner u
          //   ) {

          //   u1 = tmp_c_u;
          //   u2 = tmp_t_u;
          //   he = hei;
          //   ls_i_prev = ls_i;

          // }

        }

        // If want the intersection closer to the ending
        else if (end == 1) {

          if (ls_i == vertices.size() - 2 && tmp_c_u > 1 && tmp_c_u < u1) {
            update = true;
          }
          else if (fabs(tmp_c_u) <= tol || (tmp_c_u > 0 && tmp_c_u < 1) || fabs(1 - tmp_c_u) <= tol) {
            if (ls_i < ls_i_prev) {
              update = true;
            }
            else if (ls_i == ls_i_prev && tmp_c_u < u1) {
              update = true;
            }
          }

          // if (
          //   ((ls_i == vertices.size() - 2) && tmp_c_u > 1 && tmp_c_u < u1)  // after the last vertex
          //   || ((fabs(tmp_c_u) <= tol || (tmp_c_u > 0 && tmp_c_u < 1) || fabs(1 - tmp_c_u) <= tol) && ls_i < ls_i_prev)  // inner line segment
          //   || ((fabs(tmp_c_u) <= tol || (tmp_c_u > 0 && tmp_c_u < 1) || fabs(1 - tmp_c_u) <= tol) && ls_i == ls_i_prev && tmp_c_u < u1) // same line segment but inner u
          //   ) {

          //   u1 = tmp_c_u;
          //   u2 = tmp_t_u;
          //   he = hei;
          //   ls_i_prev = ls_i;

          // }

        }

      }

      if (update) {

        PLOG(debug) << pmessage->message("update intersection");

        u1 = tmp_c_u;
        u2 = tmp_t_u;
        he = hei;
        ls_i_prev = ls_i;

        PLOG(debug) << pmessage->message("u1 = " + std::to_string(u1));
        PLOG(debug) << pmessage->message("u2 = " + std::to_string(u2));
        PLOG(debug) << pmessage->message("end = " + std::to_string(end));

      }


      ls_i = ls_i_prev;

    }

    hei = hei->next();

  } while (hei != hel->incidentEdge());

  pmessage->decreaseIndent();

  return he;
}










Baseline *findCurvesIntersection(
  Baseline *bl, PGeoLineSegment *ls, int end,
  double &u1, double &u2, int &iold, int &inew,
  std::vector<int> &link_to_list, std::vector<int> &base_offset_indices_links
  ) {
  // After adjusting the curve
  // the first kept vertex will be at index iold in the old curve
  // and at the index inew in the new curve

  // std::cout << "\n[debug] findCurvesIntersection" << std::endl;

  // std::cout << "        curve bl:" << std::endl;
  // for (auto v : bl->vertices()) {
  //   std::cout << "        " << v << std::endl;
  // }

  iold = 0;
  inew = 0;

  Baseline *bl_new = new Baseline();
  std::size_t n = bl->vertices().size();

  std::vector<int> link_to_list_new;

  std::list<PDCELVertex *> vlist;
  std::list<int> link_to_list_copy;
  for (int i = 0; i < n; ++i) {
    vlist.push_back(bl->vertices()[i]);
    link_to_list_copy.push_back(link_to_list[i]);
  }
  if (end == 1) {
    vlist.reverse();
    link_to_list_copy.reverse();
  }
  // std::cout << "        vertex list vlist:" << std::endl;
  // for (auto v : vlist) {
  //   std::cout << "        " << v << std::endl;
  // }

  PDCELVertex *v1, *v2, *v_new = nullptr;
  PGeoLineSegment *lsi;
  int link_i1, link_i2;
  bool not_parallel;
  while (!vlist.empty()) {
    v1 = vlist.front();
    vlist.pop_front();
    v2 = vlist.front();
    iold++;

    link_i1 = link_to_list_copy.front();
    link_to_list_copy.pop_front();
    link_i2 = link_to_list_copy.front();

    lsi = new PGeoLineSegment(v1, v2);

    // std::cout << "        line segment lsi: " << lsi << std::endl;

    not_parallel = calcLineIntersection2D(lsi, ls, u1, u2);

    // std::cout << "        u1 = " << u1 << std::endl;

    if (not_parallel) {
      if (u1 > 1) {
        continue;
      } else {
        if (fabs(u1) < TOLERANCE) {
          vlist.push_front(v1);
          link_to_list_copy.push_front(link_i1);
          iold--;
          break;
        } else if (fabs(u1 - 1) < TOLERANCE) {
          break;
        } else if (u1 < 0) {
          // In this case, the new curve is extended
          // along the direction of the tangent at the end.
          // The old vertex v1 will not be added back,
          // instead, the intersecting point will be the new ending vertex.

          // vlist.push_front(v1);
          // iold--;
          v_new = lsi->getParametricVertex(u1);
          vlist.push_front(v_new);

          link_to_list_copy.push_front(0);
          inew++;
          break;
        } else {
          v_new = lsi->getParametricVertex(u1);
          vlist.push_front(v_new);
          link_to_list_copy.push_front(0);
          inew++;
          break;
        }
      }
    } else {
      continue;
    }
  }

  if (vlist.size() < 2) {
    return nullptr;
  }

  while (!vlist.empty()) {
    if (end == 0) {
      bl_new->addPVertex(vlist.front());
      vlist.pop_front();

      link_to_list_new.push_back(link_to_list_copy.front());
      link_to_list_copy.pop_front();
    } else if (end == 1) {
      bl_new->addPVertex(vlist.back());
      vlist.pop_back();

      link_to_list_new.push_back(link_to_list_copy.back());
      link_to_list_copy.pop_back();
      iold = n - 1 - iold;
      inew = bl_new->vertices().size() - 1 - inew;
    }
  }

  link_to_list.swap(link_to_list_new);

  // std::cout << "        baseline bl_new" << std::endl;
  // for (auto v : bl_new->vertices()) {
  //   std::cout << "        " << v << std::endl;
  // }

  return bl_new;
}









int findAllIntersections(
  const std::vector<PDCELVertex *> &c1, const std::vector<PDCELVertex *> &c2,
  std::vector<int> &i1s, std::vector<int> &i2s,
  std::vector<double> &u1s, std::vector<double> &u2s
) {

  // Find all intersections

  // i1s, i2s: indices of line segments having intersection
  // u1s, u2s: non-dimensional location of the intersection on the line segment

  // std::cout << "\n[debug] function: findAllIntersections\n";

  PDCELVertex *v11, *v12, *v21, *v22;

  for (int i = 0; i < c1.size() - 1; ++i) {

    v11 = c1[i];
    v12 = c1[i + 1];

    for (int j = 0; j < c2.size() - 1; ++j) {

      v21 = c2[j];
      v22 = c2[j + 1];

      // Check intersection
      bool not_parallel;
      double u1, u2;

      // std::cout << "        find intersection:" << std::endl;
      // std::cout << "        v11 = " << v11 << ", v12 = " << v12 <<
      // std::endl; std::cout << "        v21 = " << v21 << ", v22 = " << v22
      // << std::endl;

      not_parallel = calcLineIntersection2D(v11, v12, v21, v22, u1, u2);

      // std::cout << "line_" << line_i << "_seg_" << i+1 << " and ";
      // std::cout << "line_" << line_i+1 << "_seg_" << j+1 << ": ";
      // std::cout << "u1=" << u1 << " and u2=" << u2 << std::endl;
      // std::cout << "        not_parallel = " << not_parallel << ", u1 = "
      // << u1 << ", u2 = " << u2 << std::endl;

      if (not_parallel) {
        if (
          // 9 cases
          (u1 >= 0 && u1 <= 1 && u2 >= 0 && u2 <= 1)  // inner x inner
          || (i == 0 && u1 < 0 && j == 0 && u2 < 0) // before head x before head
          || (i == 0 && u1 < 0 && u2 >= 0 && u2 <= 1)  // before head x inner
          || (i == 0 && u1 < 0 && j == c2.size() - 2 && u2 > 1) // before head x after tail
          || (i == c1.size() - 2 && u1 > 1 && j == 0 && u2 < 0) // after tail x before head
          || (i == c1.size() - 2 && u1 > 1 && u2 >= 0 && u2 <= 1)  // after tail x inner
          || (i == c1.size() - 2 && u1 > 1 && j == c2.size() - 2 && u2 > 1) // after tail x after tail
          || (u1 >= 0 && u1 <= 1 && j == 0 && u2 < 0)  // inner x before head
          || (u1 >= 0 && u1 <= 1 && j == c2.size() - 2 && u2 > 1)  // inner x after tail
          ) {

          i1s.push_back(i);
          i2s.push_back(j);
          u1s.push_back(u1);
          u2s.push_back(u2);

          // found = true;
          // std::cout << "        found intersection:" << std::endl;
          // std::cout << "        v11 = " << v11 << ", v12 = " << v12 << std::endl;
          // std::cout << "        v21 = " << v21 << ", v22 = " << v22 << std::endl;
          // std::cout << "        not_parallel = " << not_parallel
          //           << ", u1 = " << u1 << ", u2 = " << u2 << std::endl;

        }

      }

    }

  }

  // for (auto i = 0; i < i1s.size(); i++) {
  //   std::cout << "intersection " << i << std::endl;
  //   std::cout << "  i1 = " << i1s[i] << ", v11 = " << c1[i1s[i]] << ", v12 = " << c1[i1s[i]+1] << ", u1 = " << u1s[i] << std::endl;
  //   std::cout << "  i2 = " << i2s[i] << ", v21 = " << c2[i2s[i]] << ", v22 = " << c2[i2s[i]+1] << ", u2 = " << u2s[i] << std::endl;
  // }

  return 0;

}









double getIntersectionLocation(
  std::vector<PDCELVertex *> &c,
  const std::vector<int> &ii, std::vector<double> &uu,
  const int &which_end, const int &inner_only,
  int &ls_i, int &j, Message *pmessage
) {
  // Find the intersection location that is the closest to the expected end
  pmessage->increaseIndent();

  // PLOG(debug) << pmessage->message("in function: getIntersectionLocation");

  ls_i = ii[0];
  double u = uu[0];
  j = 0;

  for (auto k = 1; k < ii.size(); k++) {

    if ((inner_only && uu[k] >= 0 && uu[k] <= 1) || !inner_only) {
      if (which_end == 0) {
        // Closer to the beginning side
        if (ii[k] < ls_i) {
          ls_i = ii[k];
          u = uu[k];
          j = k;
        }
        else if (ii[k] == ls_i) {
          if (uu[k] < u) {
            u = uu[k];
            j = k;
          }

        }

      }
      else if (which_end == 1) {
        // Closer to the ending side
        if (ii[k] > ls_i) {
          ls_i = ii[k];
          u = uu[k];
          j = k;
        }
        else if (ii[k] == ls_i) {
          if (uu[k] > u) {
            u = uu[k];
            j = k;
          }

        }

      }
    }

  }


  // PLOG(debug) << pmessage->message(
  //   "ls_i = " + std::to_string(ls_i)
  //   + ", u = " + std::to_string(u)
  //   + ", v1 = " + c[ls_i]->printString()
  //   + ", v2 = " + c[ls_i+1]->printString()
  // );

  pmessage->decreaseIndent();

  return u;
}







PDCELVertex *getIntersectionVertex(
  std::vector<PDCELVertex *> &c1, std::vector<PDCELVertex *> &c2,
  int &i1, int &i2, const double &u1, const double &u2,
  const int &which_end_1, const int &which_end_2,
  const int &inner_only_1, const int &inner_only_2,
  int &is_new_1, int &is_new_2,
  const double &tol
) {
  // Get wanted intersection vertex from all options

  // i1s, i2s: indices of line segments having intersection
  // u1s, u2s: non-dimensional location of the intersection on the line segment
  // which_end_1, which_end_2: choose the intersection closer to beginning (0) or ending (1)
  // inner_only_1, inner_only_2: whether consider intersections between two ends (1) or not (0)

  // std::cout << "\n[debug] function: getIntersectionVertex\n";

  PDCELVertex *ip = nullptr; // The intersection vertex
  // PDCELVertex *ip2 = nullptr;

  // Create/Get the intersection vertex
  PDCELVertex *v11 = c1[i1];
  PDCELVertex *v12 = c1[i1 + 1];
  PDCELVertex *v21 = c2[i2];
  PDCELVertex *v22 = c2[i2 + 1];

  // std::cout << "v11 = " << v11 << ", v12 = " << v12 << std::endl;
  // std::cout << "v21 = " << v21 << ", v22 = " << v22 << std::endl;

  bool insert1, insert2;
  // int ii1, ii2;

  // The intersecting point is an existing point of either curve
  if (fabs(u1) <= tol || fabs(1 - u1) <= tol ||
      fabs(u2) <= tol || fabs(1 - u2) <= tol) {

    // For curve c1
    if (fabs(u1) <= tol) {
      // Intersecting point is the beginning point (v11) of the line
      // segment i (v11-v12) of c1
      insert1 = false;
      is_new_1 = 0;
      // i1 = i1;
      ip = v11;
    }
    else if (fabs(1 - u1) <= tol) {
      // Intersecting point is the ending point (v12) of the line
      // segment i (v11-v12) of c1
      insert1 = false;
      is_new_1 = 0;
      i1 += 1;
      ip = v12;
    }
    else {
      // Intersecting point is in the middle of the line segment i
      // of c1, insertion is needed
      insert1 = true;
      is_new_1 = 1;
      i1 += 1;
    }

    if (fabs(u2) <= tol) {
      // Intersecting point is the beginning point (v21) of the line
      // segment j (v21-v22) of c2
      insert2 = false;
      is_new_2 = 0;
      // ii2 = i2;
      ip = v21;
    }
    else if (fabs(1 - u2) <= tol) {
      // Intersecting point is the beginning point (v22) of the line
      // segment j (v21-v22) of c2
      insert2 = false;
      is_new_2 = 0;
      i2 += 1;
      ip = v22;
    }
    else {
      // Intersecting point is in the middle of the line segment i
      // of c2, insertion is needed
      insert2 = true;
      is_new_2 = 1;
      i2 += 1;
    }
  }

  else {
    // The last case, the intersection is at the middle of two sub-lines
    // Calculate the new point
    insert1 = true;
    insert2 = true;
    is_new_1 = 1;
    is_new_2 = 1;
    i1 += 1;
    i2 += 1;

    ip = new PDCELVertex(
        getParametricPoint(v11->point(), v12->point(), u1));
    // ip2 = new PDCELVertex(
    //     getParametricPoint(v21->point(), v22->point(), u2));

    // std::cout << "        new vertex v0 = " << v0 << std::endl;
  }

  // std::cout << "intersecting point ip1: " << ip << std::endl;
  // std::cout << "intersecting point ip2: " << ip2 << std::endl;

  // Insert the intersection vertex (if needed)
  if (insert1) {
    c1.insert(c1.begin()+i1, ip);
  }
  else {
    c1[i1] = ip;
  }

  if (insert2) {
    c2.insert(c2.begin()+i2, ip);
  } else {
    c2[i2] = ip;
  }

  // std::cout << "updated curve 1\n";
  // for (auto v : c1) {
  //   std::cout << v;
  //   if (v == ip) {
  //     std::cout << " intersect i = " << i1;
  //   }
  //   std::cout << std::endl;
  // }
  // std::cout << "updated curve 2\n";
  // for (auto v : c2) {
  //   std::cout << v;
  //   if (v == ip) {
  //     std::cout << " intersect i = " << i2;
  //   }
  //   std::cout << std::endl;
  // }

  return ip;

}




