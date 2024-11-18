#include "geo.hpp"

// #include "Material.hpp"
// #include "PDCELHalfEdge.hpp"
// #include "PDCELHalfEdgeLoop.hpp"
// #include "PDCELVertex.hpp"
// #include "PGeoClasses.hpp"
// #include "globalConstants.hpp"
// #include "globalVariables.hpp"
// #include "internalClasses.hpp"
// #include "overloadOperator.hpp"
// #include "utilities.hpp"

// #include "gmsh/SPoint2.h"
// #include "gmsh/SPoint3.h"
// #include "gmsh/STensor3.h"
// #include "gmsh/SVector3.h"

#include <cmath>
#include <cstdlib>
#include <list>
#include <string>
#include <vector>

// int getTurningSide(SVector3 vec1, SVector3 vec2) {
//   SVector3 n;
//   n = crossprod(vec1, vec2);

//   if (fabs(n[0]) < TOLERANCE) {
//     return 0; // collinear
//   } else if (n[0] > 0) {
//     return 1; // left
//   } else {
//     return -1; // right
//   }
// }










// double calcDistanceSquared(PDCELVertex *v1, PDCELVertex *v2) {
//   double dx = v1->x() - v2->x();
//   double dy = v1->y() - v2->y();
//   double dz = v1->z() - v2->z();

//   return dx * dx + dy * dy + dz * dz;
// }










// Baseline *joinCurves(std::list<Baseline *> curves) {
//   // std::cout << "[debug] joining curves" << std::endl;

//   Baseline *bl, *bl_tmp;
//   bl = new Baseline(curves.front());
//   bl->setName(bl->getName() + "_new");
//   curves.pop_front();

//   while (!curves.empty()) {
//     std::list<Baseline *>::iterator blit;
//     for (blit = curves.begin(); blit != curves.end(); ++blit) {
//       if ((*blit)->vertices().front() == bl->vertices().front()) {
//         // bl_tmp = new Baseline(*blit);
//         // bl_tmp->setName(bl_tmp->getName() + "_r");
//         bl->join((*blit), 0, true);
//         break;
//       } else if ((*blit)->vertices().front() == bl->vertices().back()) {
//         bl->join((*blit), 1, false);
//         break;
//       } else if ((*blit)->vertices().back() == bl->vertices().front()) {
//         bl->join((*blit), 0, false);
//         break;
//       } else if ((*blit)->vertices().back() == bl->vertices().back()) {
//         // bl_tmp = new Baseline(*blit);
//         // bl_tmp->setName(bl_tmp->getName() + "_r");
//         bl->join((*blit), 1, true);
//         break;
//       }
//     }
//     curves.remove(*blit);
//   }

//   return bl;
// }










// void adjustCurveEnd(Baseline *bl, PGeoLineSegment *ls, int end) {
//   PGeoLineSegment *ls_end;
//   if (end == 0) {
//     ls_end = new PGeoLineSegment(bl->vertices().front(),
//                                  bl->getTangentVectorBegin());
//   } else if (end == 1) {
//     ls_end =
//         new PGeoLineSegment(bl->vertices().back(), bl->getTangentVectorEnd());
//   }

//   double u1, u2;
//   calcLineIntersection2D(ls_end, ls, u1, u2);
//   PDCELVertex *vnew = ls_end->getParametricVertex(u1);

//   if (end == 0) {
//     bl->vertices()[0] = vnew;
//   } else if (end == 1) {
//     bl->vertices()[bl->vertices().size() - 1] = vnew;
//   }
// }










// SVector3 getVectorFromAngle(double &angle, const int &plane) {
//   SVector3 v;

//   // Map the angle into the range (-90, 270]
//   while (angle <= -90 || angle > 270) {
//     if (angle <= -90) {
//       angle += 360;
//     } else if (angle > 270) {
//       angle -= 360;
//     }
//   }

//   double reverse = 1.0;
//   if (angle > 90) {
//     reverse = -1.0;
//   }

//   double d1, d2;
//   if (angle == 90.0 || angle == 270.0) {
//     d1 = 0.0;
//     d2 = 1.0;
//   } else {
//     d1 = 1.0;
//     d2 = tan(deg2rad(angle));
//   }

//   if (plane == 0) {
//     // y-z plane
//     v = SVector3(0, d1, d2);
//   } else if (plane == 1) {
//     // z-x plane
//     v = SVector3(d2, 0, d1);
//   } else if (plane == 2) {
//     // x-y plane
//     v = SVector3(d1, d2, 0);
//   }

//   return v * reverse;
// }










// SPoint3 getParametricPoint(const SPoint3 &p1, const SPoint3 &p2, double u) {
//   if (fabs(u) < TOLERANCE) {
//     return SPoint3(p1);
//   } else if (fabs(u - 1) < TOLERANCE) {
//     return SPoint3(p2);
//   } else {
//     double x, y, z;

//     x = p1.x() + u * (p2.x() - p1.x());
//     y = p1.y() + u * (p2.y() - p1.y());
//     z = p1.z() + u * (p2.z() - p1.z());

//     return SPoint3(x, y, z);
//   }
// }










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

// bool calcLineIntersection2D(SPoint3 l1p1, SPoint3 l1p2, SPoint3 l2p1,
//                             SPoint3 l2p2, double &u1, double &u2, int &plane) {
//   int d1, d2;
//   if (plane == 0) {
//     d1 = 1;
//     d2 = 2;
//   } else if (plane == 1) {
//     d1 = 2;
//     d2 = 0;
//   } else if (plane == 2) {
//     d1 = 0;
//     d2 = 1;
//   }

//   SPoint2 l1p, l1q, l2p, l2q;
//   l1p = SPoint2(l1p1[d1], l1p1[d2]);
//   l1q = SPoint2(l1p2[d1], l1p2[d2]);
//   l2p = SPoint2(l2p1[d1], l2p1[d2]);
//   l2q = SPoint2(l2p2[d1], l2p2[d2]);

//   return calcLineIntersection2D(l1p, l1q, l2p, l2q, u1, u2);
// }










// bool calcLineIntersection2D(PDCELVertex *ls1v1, PDCELVertex *ls1v2,
//                             PDCELVertex *ls2v1, PDCELVertex *ls2v2, double &u1,
//                             double &u2) {
//   return calcLineIntersection2D(ls1v1->point2(), ls1v2->point2(),
//                                 ls2v1->point2(), ls2v2->point2(), u1, u2);
// }










// bool calcLineIntersection2D(PGeoLineSegment *ls1, PGeoLineSegment *ls2,
//                             double &u1, double &u2) {
//   SPoint2 ls1p1, ls1p2, ls2p1, ls2p2;
//   ls1p1 = ls1->v1()->point2();
//   ls1p2 = ls1->v2()->point2();
//   ls2p1 = ls2->v1()->point2();
//   ls2p2 = ls2->v2()->point2();

//   return calcLineIntersection2D(ls1p1, ls1p2, ls2p1, ls2p2, u1, u2);
// }










// bool isParallel(PGeoLineSegment *ls1, PGeoLineSegment *ls2) {
//   SVector3 vec1, vec2, vecn;
//   vec1 = ls1->toVector();
//   vec2 = ls2->toVector();

//   vecn = crossprod(vec1, vec2);
//   if (vecn.normSq() < TOLERANCE * TOLERANCE) {
//     return true;
//   } else {
//     return false;
//   }
// }










// bool isCollinear(PGeoLineSegment *ls1, PGeoLineSegment *ls2) {
//   if (!isParallel(ls1, ls2)) {
//     return false;
//   }

//   SVector3 vec1, vec2, vecn;
//   vec1 = ls1->toVector();
//   vec2 = SVector3(ls1->v1()->point(), ls2->v1()->point());
//   vecn = crossprod(vec1, vec2);
//   if (vecn.normSq() < TOLERANCE * TOLERANCE) {
//     return true;
//   } else {
//     return false;
//   }
// }











// bool isOverlapped(PGeoLineSegment *ls1, PGeoLineSegment *ls2) {
//   if (ls1->vout()->point() < ls2->vin()->point() ||
//       ls2->vout()->point() < ls1->vin()->point()) {
//     return false;
//   }

//   if (!isCollinear(ls1, ls2)) {
//     return false;
//   }

//   return true;
// }











// void offsetLineSegment(SPoint3 &p1, SPoint3 &p2, SVector3 &dr, double &ds,
//                        SPoint3 &q1, SPoint3 &q2) {
//   dr.normalize();
//   q1 = (SVector3(p1) + dr * ds).point();
//   q2 = (SVector3(p2) + dr * ds).point();
// }











// PGeoLineSegment *offsetLineSegment(PGeoLineSegment *ls, int side, double d) {
//   SVector3 t, n, p;
//   n = SVector3(side, 0, 0);
//   t = ls->toVector();
//   // if (side == "left") {
//   //   n = SVector3(1, 0, 0);
//   // } else if (side == "right") {
//   //   n = SVector3(-1, 0, 0);
//   // }
//   p = crossprod(n, t).unit() * d;
//   // std::cout << "        vector p: " << p << std::endl;

//   return offsetLineSegment(ls, p);
// }











// PGeoLineSegment *offsetLineSegment(PGeoLineSegment *ls, SVector3 &offset) {
//   PDCELVertex *v1, *v2;
//   v1 = new PDCELVertex(ls->v1()->point() + offset.point());
//   v2 = new PDCELVertex(ls->v2()->point() + offset.point());

//   return new PGeoLineSegment(v1, v2);
// }











// Baseline *offsetCurve(Baseline *curve, int side, double distance) {
//   // std::cout << "[debug] offsetCurve" << std::endl;
//   Baseline *curve_off = new Baseline();

//   PGeoLineSegment *ls;
//   if (curve->vertices().size() == 2) {
//     ls = new PGeoLineSegment(curve->vertices()[0], curve->vertices()[1]);
//     ls = offsetLineSegment(ls, side, distance);

//     curve_off->addPVertex(ls->v1());
//     curve_off->addPVertex(ls->v2());
//   } else {
//     PGeoLineSegment *ls_prev, *ls_first_off;
//     for (int i = 0; i < curve->vertices().size() - 1; ++i) {
//       ls = new PGeoLineSegment(curve->vertices()[i], curve->vertices()[i + 1]);
//       ls = offsetLineSegment(ls, side, distance);

//       if (i == 0) {
//         ls_first_off = ls;
//         curve_off->addPVertex(ls->v1());
//       }

//       if (i > 0) {
//         // Calculate intersection
//         double u1, u2;
//         bool not_parallel;
//         not_parallel = calcLineIntersection2D(ls, ls_prev, u1, u2);
//         if (not_parallel) {
//           curve_off->addPVertex(ls->getParametricVertex(u1));
//         } else {
//           curve_off->addPVertex(ls->v1());
//         }
//       }

//       if (i == curve->vertices().size() - 2) {
//         curve_off->addPVertex(ls->v2());
//       }

//       ls_prev = ls;
//     }

//     if (curve->vertices().front() == curve->vertices().back()) {
//       double u1, u2;
//       bool not_parallel;
//       PDCELVertex *v;
//       not_parallel = calcLineIntersection2D(ls_first_off, ls_prev, u1, u2);
//       if (not_parallel) {
//         curve_off->vertices()[0] = ls_first_off->getParametricVertex(u1);
//       }
//       curve_off->vertices()[curve_off->vertices().size() - 1] =
//           curve_off->vertices()[0];
//     }
//   }

//   // std::cout << "        baseline curve_off" << std::endl;
//   // for (auto v : curve_off->vertices()) {
//   //   std::cout << "        " << v << std::endl;
//   // }

//   return curve_off;
// }

// //
// //
// //






// // int intersect(std::vector<PDCELVertex *> &curve1,
// //               std::vector<PDCELVertex *> &curve2, PDCELVertex *intersect,
// //               const int &keep1, const int &keep2) {

// // }

// //
// //
// //






// int offset(PDCELVertex *v1_base, PDCELVertex *v2_base, int side, double dist,
//            PDCELVertex *v1_off, PDCELVertex *v2_off) {
//   // std::cout << "[debug] offset a line segment:" << std::endl;
//   // std::cout << "        " << v1_base << std::endl;
//   // std::cout << "        " << v2_base << std::endl;

//   SVector3 dir, n, t;
//   n = SVector3(side, 0, 0);
//   t = SVector3(v1_base->point(), v2_base->point());

//   dir = crossprod(n, t).unit() * dist;
//   // std::cout << "        vector dir = " << dir << std::endl;

//   SPoint3 p1, p2;
//   p1 = v1_base->point() + dir.point();
//   p2 = v2_base->point() + dir.point();
//   // std::cout << "        point p1 = " << p1 << std::endl;

//   v1_off->setPoint(p1);
//   v2_off->setPoint(p2);
//   // v1_off = new PDCELVertex(p1[0], p1[1], p1[2]);
//   // v2_off = new PDCELVertex(p2[0], p2[1], p2[2]);

//   return 1;
// }










// int offset(const std::vector<PDCELVertex *> &base, int side, double dist,
//            std::vector<PDCELVertex *> &vertices, std::vector<int> &link_to) {
//   // std::cout << "\n[debug] offset" << std::endl;

//   int size = base.size();
//   // std::cout << "        base.size() = " << size << std::endl;
//   PDCELVertex *v_tmp, *v1_tmp, *v2_tmp, *v1_prev, *v2_prev;
//   // PGeoLineSegment *ls, *ls_prev, *ls_first;
//   std::vector<int> link_to_tmp;

//   if (size == 2) {
//     // ls = new PGeoLineSegment(curve->vertices()[0], curve->vertices()[1]);
//     v1_tmp = new PDCELVertex();
//     v2_tmp = new PDCELVertex();

//     offset(base[0], base[1], side, dist, v1_tmp, v2_tmp);

//     vertices.push_back(v1_tmp);
//     vertices.push_back(v2_tmp);

//     link_to.push_back(0);
//     link_to.push_back(1);

//     return 1;
//   }

//   std::vector<PDCELVertex *> vertices_tmp;

//   // Initialize all vertices as un-degenerated
//   // std::vector<int> degen_flags_tmp(size, 1);
//   // std::vector<int> undegen_counts, degen_counts;




//   //
//   // Step 1: Offset each line segment, and
//   // calculate intersections between every two neighbors
//   // std::cout << "\n[debug] offset: step 1" << std::endl;

//   // PGeoLineSegment *ls_prev, *ls_first;
//   for (int i = 0; i < size - 1; ++i) {
//     // std::cout << "        line seg: " << i+1 << std::endl;
//     v1_tmp = new PDCELVertex();
//     v2_tmp = new PDCELVertex();

//     offset(base[i], base[i + 1], side, dist, v1_tmp, v2_tmp);

//     // std::cout << "        vertex v1_tmp = " << v1_tmp << std::endl;
//     // std::cout << "        vertex v2_tmp = " << v2_tmp << std::endl;
//     // std::cout << base[i] << "-" << base[i + 1] << " -> ";
//     // std::cout << v1_tmp << "-" << v2_tmp << std::endl;

//     link_to_tmp.push_back(i);

//     if (i == 0) {
//       // ls_first = ls;
//       vertices_tmp.push_back(v1_tmp);
//     }
//     else {
//       // Calculate intersection
//       double u1, u2;
//       bool not_parallel;
//       // std::cout << "        find intersection:" << std::endl;
//       // std::cout << "        v1_prev = " << v1_prev << ", v2_prev = " <<
//       // v2_prev << std::endl; std::cout << "        v1_tmp = " << v1_tmp << ",
//       // v2_tmp = " << v2_tmp << std::endl;
//       not_parallel =
//           calcLineIntersection2D(v1_prev, v2_prev, v1_tmp, v2_tmp, u1, u2);
//       // std::cout << "        not_parallel = " << not_parallel << ", u1 = " <<
//       // u1 << ", u2 = " << u2 << std::endl;
//       if (not_parallel) {
//         // vertices_tmp.push_back(ls->getParametricVertex(u1));
//         v_tmp = new PDCELVertex(
//             getParametricPoint(v1_prev->point(), v2_prev->point(), u1));
//         vertices_tmp.push_back(v_tmp);
//       }
//       else {
//         vertices_tmp.push_back(v2_prev);
//       }
//       // std::cout << "        added vertex: " << vertices_tmp.back() <<
//       // std::endl;
//     }

//     if (i == size - 2) {
//       vertices_tmp.push_back(v2_tmp);
//       link_to_tmp.push_back(i + 1);
//     }
//     // ls_prev = ls;
//     v1_prev = v1_tmp;
//     v2_prev = v2_tmp;
//   }

//   // std::cout << "        link to indices:" << std::endl;
//   // for (auto i : link_to_tmp) {
//   //   std::cout << "        " << i << std::endl;
//   // }

//   // If this curve is closed
//   if (base.front() == base.back()) {
//     double u1, u2;
//     bool not_parallel;
//     PDCELVertex *v;
//     not_parallel = calcLineIntersection2D(v1_prev, v2_prev, vertices_tmp[0],
//                                           vertices_tmp[1], u1, u2);
//     // std::cout << "        not_parallel = " << not_parallel << ", u1 = " << u1
//     // << ", u2 = " << u2 << std::endl;
//     if (not_parallel) {
//       v_tmp = new PDCELVertex(
//           getParametricPoint(v1_prev->point(), v2_prev->point(), u1));
//       vertices_tmp[0] = v_tmp;
//     }
//     vertices_tmp[size - 1] = vertices_tmp[0];
//   }

//   // std::cout << "        list vertices_tmp: " << std::endl;
//   // for (auto v : vertices_tmp) {
//   //   std::cout << "        " << v << std::endl;
//   // }




//   //
//   // Step 2: Check degenerated cases (zero length and inversed line segments)
//   // std::cout << "\n[debug] offset: step 2" << std::endl;
//   SVector3 vec_base, vec_off;

//   // Eliminate reversed direction line segments
//   // The result is a group of sub-lines with correct orientation
//   std::vector<std::vector<PDCELVertex *>> lines_group;
//   std::vector<std::vector<int>> link_tos_group;
//   // std::vector<std::vector<int>> degen_flags_group;

//   // int line_i = -1;
//   // int degen_i = -1;
//   // int undegen_count = 0;
//   // int degen_count = 0;

//   // Mark if the orientation is correct
//   bool check_prev{false}, check_next;
//   for (int j = 0; j < size - 1; ++j) {
//     // std::cout << "        index j = " << j << std::endl;
//     vec_base = SVector3(base[j]->point(), base[j + 1]->point());
//     vec_off = SVector3(vertices_tmp[j]->point(), vertices_tmp[j + 1]->point());
//     if (dot(vec_base, vec_off) <= 0 ||
//         (vec_off.normSq() < TOLERANCE * TOLERANCE)) {
//       // Offset line segment is in the wrong direction or too short
//       // This is the end of the current sub-line
//       check_next = false;
//     }
//     else {
//       check_next = true;
//       if (!check_prev) {
//         // This means that we are starting a new sub-line
//         std::vector<PDCELVertex *> lines_group_i;
//         // line_i += 1;
//         lines_group_i.push_back(vertices_tmp[j]);
//         lines_group.push_back(lines_group_i);

//         std::vector<int> link_tos_group_i;
//         link_tos_group_i.push_back(link_to_tmp[j]);
//         link_tos_group.push_back(link_tos_group_i);
//       }
//       lines_group.back().push_back(vertices_tmp[j + 1]);
//       link_tos_group.back().push_back(link_to_tmp[j + 1]);
//     }
//     // std::cout << "        check_next = " << check_next << std::endl;
//     check_prev = check_next;
//   }
//   // std::cout << "        lines_group:" << std::endl;
//   // for (int i = 0; i < lines_group.size(); ++i) {
//   //   std::cout << "        line " << i << std::endl;
//   //   for (int j = 0; j < lines_group[i].size(); ++j) {
//   //     std::cout << "        " << lines_group[i][j] << " links to " <<
//   //     link_tos_group[i][j] << std::endl;
//   //   }
//   // }





//   //
//   // Step 3: Find intersections between neighboring sub-lines for more than 2 lines
//   // std::cout << "\n[debug] offset: step 3" << std::endl;
//   //
//   // Here use a brute force method
//   // Since the intersection should be found in a local region
//   // (tail part of the previous one and head part of the next one)
//   int size_i, size_i1 = lines_group.back().size();
//   std::vector<PDCELVertex *> sline_i, sline_i1;
//   PDCELVertex *v11, *v12, *v21, *v22, *v0 = nullptr;
//   bool found;
//   std::vector<int> link_i, link_i1;
//   // int link11, link12, link21, link22;
//   int i = 0, j = 0, i_prev = 0;
//   for (int line_i = 0; line_i < lines_group.size() - 1; ++line_i) {
//     // std::cout << "        find intersection between line "
//     //           << line_i << " and line " << line_i + 1 << std::endl;
//     sline_i = lines_group[line_i];
//     sline_i1 = lines_group[line_i + 1];

//     link_i = link_tos_group[line_i];
//     link_i1 = link_tos_group[line_i + 1];

//     size_i = sline_i.size();
//     size_i1 = sline_i1.size();

//     found = false;
//     v0 = nullptr;

//     for (int jj = 0; jj < size_i - 1; ++jj) {
//       v12 = sline_i[size_i - 1 - jj];
//       // sline_i.pop_back();
//       v11 = sline_i[size_i - 2 - jj];

//       // link11 = link_i[size_i - 1 - jj];
//       // link_i.pop_back();
//       // link12 = link_i[size_i - 2 - jj];

//       for (int ii = 0; ii < size_i1 - 1; ++ii) {
//         v21 = sline_i1[ii];
//         // sline_i1.pop_front();
//         v22 = sline_i1[ii + 1];

//         // link21 = link_i1[ii];
//         // link_i1.pop_front();
//         // link22 = link_i1[ii + 1];

//         // Check intersection
//         bool not_parallel;
//         double u1, u2;
//         // std::cout << "        find intersection:" << std::endl;
//         // std::cout << "        v11 = " << v11 << ", v12 = " << v12 <<
//         // std::endl; std::cout << "        v21 = " << v21 << ", v22 = " << v22
//         // << std::endl;
//         not_parallel = calcLineIntersection2D(v11, v12, v21, v22, u1, u2);
//         // std::cout << "line_" << line_i << "_seg_" << jj+1 << " and ";
//         // std::cout << "line_" << line_i+1 << "_seg_" << ii+1 << ": ";
//         // std::cout << "u1=" << u1 << " and u2=" << u2 << std::endl;
//         // std::cout << "        not_parallel = " << not_parallel << ", u1 = "
//         // << u1 << ", u2 = " << u2 << std::endl;
//         if (not_parallel) {
//           if ((u1 >= 0 && u1 <= 1 && u2 >= 0 && u2 <= 1) || (jj == 0 && u1 > 1) || (ii == 0 && u2 < 0)) {
//             found = true;
//             // std::cout << "        found intersection:" << std::endl;
//             // std::cout << "        v11 = " << v11 << ", v12 = " << v12 << std::endl;
//             // std::cout << "        v21 = " << v21 << ", v22 = " << v22 << std::endl;
//             // std::cout << "        not_parallel = " << not_parallel
//             //           << ", u1 = " << u1 << ", u2 = " << u2 << std::endl;
//             if (fabs(u1) <= TOLERANCE || fabs(1 - u1) <= TOLERANCE ||
//                 fabs(u2) <= TOLERANCE || fabs(1 - u2) <= TOLERANCE) {
//               if (fabs(u1) <= TOLERANCE) {
//                 j = jj + 1;
//                 // sline_i1.push_front(v11);
//                 // link_i1.push_front(link11);

//                 // break;
//               }
//               else if (fabs(1 - u1) <= TOLERANCE) {
//                 j = jj;
//                 // sline_i.push_back(v12);
//                 // sline_i1.push_front(v12);

//                 // link_i.push_back(link12);
//                 // link_i1.push_front(link12);

//                 // break;
//               }
//               else {
//                 j = jj + 1;
//               }

//               if (fabs(u2) <= TOLERANCE) {
//                 i = ii;
//                 // sline_i.push_back(v21);
//                 // sline_i1.push_front(v21);

//                 // link_i.push_back(link21);
//                 // link_i1.push_front(link21);

//                 // break;
//               }
//               else if (fabs(1 - u2) <= TOLERANCE) {
//                 i = ii + 1;
//                 // sline_i.push_back(v22);

//                 // link_i.push_back(link22);

//                 // break;
//               }
//               else {
//                 i = ii + 1;
//               }
//               break;
//             }
//             else {
//               j = jj + 1;
//               i = ii + 1;
//               // The last case, calculate the intersection
//               v0 = new PDCELVertex(
//                   getParametricPoint(v11->point(), v12->point(), u1));
//               // std::cout << "        new vertex v0 = " << v0 << std::endl;
//               // sline_i.push_back(v0);
//               // sline_i1.push_front(v0);

//               // link_i.push_back(link_i.back() + 1);
//               // link_i1.push_front(link_i1.front() - 1);
//             }
//           }
//           else {
//             continue;
//           }
//         }
//         else {
//           continue;
//         }
//         if (found) {
//           break;
//         }
//       }

//       if (found) {
//         // std::cout << "        i = " << i << std::endl;
//         // std::cout << "        j = " << j << std::endl;
//         // std::cout << "        i_prev = " << i_prev << std::endl;
//         for (int kk = i_prev; kk <= size_i - 1 - j; ++kk) {
//           vertices.push_back(sline_i[kk]);
//           link_to.push_back(link_i[kk]);
//         }
//         if (v0 != nullptr) {
//           vertices.push_back(v0);
//           link_to.push_back(link_to.back() + 1);
//         }
//         i_prev = i;
//         break;
//       }
//     }
//   }

//   for (int kk = i_prev; kk < size_i1; ++kk) {
//     vertices.push_back(lines_group.back()[kk]);
//     link_to.push_back(link_tos_group.back()[kk]);
//   }

//   // for (auto v : vertices) {
//   //   std::cout << "        " << v << std::endl;
//   // }

//   //
//   // Step 4: Join all sub-lines
//   // std::cout << "        step 4" << std::endl;
//   // // for (auto line : lines_group) {
//   // for (int i = 0; i < lines_group.size(); ++i) {
//   //   while (!lines_group[i].empty()) {
//   //     if (lines_group[i].front() != vertices.back()) {
//   //       vertices.push_back(lines_group[i].front());
//   //       link_to.push_back(link_tos_group[i].front());
//   //     }
//   //     lines_group[i].pop_front();
//   //     link_tos_group[i].pop_front();
//   //   }
//   // }

//   return 1;
// }










// SVector3 calcAngleBisectVector(SPoint3 &p0, SPoint3 &p1, SPoint3 &p2) {
//   // Given three points, p0, p1, p2, calculate the line bisecting the
//   // angle of <p1p0p2, and return the vector parallel this line.
//   SVector3 v1{p0, p1}, v2{p0, p2}, vb;
//   // Make the two vectors the same length
//   v1 *= 1 / v1.normSq();
//   v2 *= 1 / v2.normSq();
//   vb = v1 + v2;

//   return vb;
// }










// SVector3 calcAngleBisectVector(SVector3 &v1, SVector3 &v2, std::string s1,
//                                std::string s2) {
//   // s1, s2 are the layup side (left or right) of v1, v2, respectively.
//   SVector3 n1{1, 0, 0}, n2{1, 0, 0}, p1, p2;
//   if (s1 == "right") {
//     n1 = -1 * n1;
//   }
//   p1 = crossprod(n1, v1).unit();

//   if (s2 == "right") {
//     n2 = -1 * n2;
//   }
//   p2 = crossprod(n2, v2).unit();

//   return p1 + p2;
// }










// void calcBoundVertices(std::vector<PDCELVertex *> &vertices,
//                        SVector3 &sv_baseline, SVector3 &sv_bound,
//                        Layup *layup) {
//   // Given the baseline direction, bound direction and layup,
//   // calculate vertices dividing layers on the bound and
//   // store them into the repository `vertices`
//   SVector3 n = crossprod(sv_bound, sv_baseline);
//   SVector3 p = crossprod(sv_baseline, n);
//   p.normalize();
//   sv_bound.normalize();

//   // std::cout << "p: " << p.point() << std::endl;

//   PDCELVertex *pv_prev, *pv_new;
//   double thk, thkp;
//   for (int i = 0; i < layup->getLayers().size(); ++i) {
//     thk = layup->getLayers()[i].getLamina()->getThickness() *
//           layup->getLayers()[i].getStack();
//     thkp = thk / dot(sv_bound, p);
//     pv_prev = vertices.back();
//     // SPoint3 p_new = pv_prev->point() + (thkp * sv_bound).point();
//     // std::cout << "thkp * sv_bound = " << (thkp * sv_bound).point() <<
//     // std::endl;
//     SPoint3 p_new = (SVector3(pv_prev->point()) + thkp * sv_bound).point();
//     pv_new = new PDCELVertex();
//     pv_new->setPoint(p_new);
//     vertices.push_back(pv_new);
//   }
// }










// void combineVertexLists(std::vector<PDCELVertex *> &vl_1,
//                         std::vector<PDCELVertex *> &vl_2,
//                         std::vector<int> &vi_1, std::vector<int> &vi_2,
//                         std::vector<PDCELVertex *> &vl_c) {
//   int m, n;
//   m = vl_1.size();
//   n = vl_2.size();

//   // Decide which dimension (y or z) is used to sort
//   int d;
//   if (abs(vl_1.front()->y() - vl_1.back()->y()) >=
//       abs(vl_1.front()->z() - vl_1.back()->z()))
//     d = 1; // d is x2 (i.e. y)
//   else
//     d = 2; // d is x3 (i.e. z)

//   // std::cout << d << std::endl;

//   std::string order;
//   if ((vl_1.front()->point()[d] - vl_1.back()->point()[d]) < 0.0)
//     order = "asc";
//   else
//     order = "des";

//   // std::cout << order << std::endl;

//   bool b_reverse_2;
//   if ((vl_1.front()->point()[d] - vl_1.back()->point()[d]) *
//           (vl_2.front()->point()[d] - vl_2.back()->point()[d]) >
//       0.0)
//     b_reverse_2 = false;
//   else
//     b_reverse_2 = true;
//   if (b_reverse_2)
//     std::reverse(vl_2.begin(), vl_2.end());

//   // std::cout << b_reverse_2 << std::endl;

//   // std::vector<PDCELVertex *> vl_c;
//   int i{0}, j{0}, k{0};
//   while (i < m && j < n) {
//     double diff{vl_1[i]->point()[d] - vl_2[j]->point()[d]};
//     if ((order == "asc" && diff < (-TOLERANCE)) ||
//         (order == "des" && diff > TOLERANCE)) {
//       // vl_c[k] = vl_1[i];
//       vl_c.push_back(vl_1[i]);
//       vi_1.push_back(k);
//       i++;
//     } else if ((order == "asc" && diff > TOLERANCE) ||
//                (order == "des" && diff < (-TOLERANCE))) {
//       // vl_c[k] = vl_2[j];
//       vl_c.push_back(vl_2[j]);
//       vi_2.push_back(k);
//       j++;
//     } else if (abs(diff) <= TOLERANCE) {
//       // vl_c[k] = vl_1[i];
//       vl_c.push_back(vl_1[i]);
//       vi_1.push_back(k);
//       vi_2.push_back(k);
//       i++;
//       j++;
//     }
//     k++;
//   }
//   if (i < m) {
//     for (int p = i; p < m; ++p) {
//       // vl_c[k] = vl_1[p];
//       vl_c.push_back(vl_1[p]);
//       vi_1.push_back(k);
//       k++;
//     }
//   } else if (j < n) {
//     for (int p = j; p < n; ++p) {
//       // vl_c[k] = vl_2[p];
//       vl_c.push_back(vl_2[p]);
//       vi_2.push_back(k);
//       k++;
//     }
//   }

//   // std::cout << "vl_c:" << std::endl;
//   // for (auto v : vl_c) {
//   //   std::cout << v << std::endl;
//   // }

//   // std::cout << "vi_1:" << std::endl;
//   // for (auto i : vi_1) {
//   //   std::cout << i << std::endl;
//   // }

//   // std::cout << "vi_2:" << std::endl;
//   // for (auto i : vi_2) {
//   //   std::cout << i << std::endl;
//   // }

//   if (b_reverse_2)
//     std::reverse(vi_2.begin(), vi_2.end());

//   return;
// }










// int intersect(PGeoLineSegment *subject, PGeoLineSegment *tool,
//               PDCELVertex *intersect) {
//   int result;
//   double us, ut;
//   bool not_parallel;

//   not_parallel = calcLineIntersection2D(subject, tool, us, ut);
//   if (not_parallel) {
//     if (us >= 0 && us <= 1) {
//       intersect = subject->getParametricVertex(us);
//       result = 1;
//     } else if (us < 0) {
//       result = -1;
//     } else if (us > 1) {
//       result = 2;
//     }
//   } else {
//     result = 0; // Parallel case
//   }

//   return result;
// }










// PDCELHalfEdge *findCurvesIntersection(Baseline *bl, PDCELHalfEdgeLoop *hel,
//                                       int end, double &u1, double &u2) {
//   PDCELHalfEdge *he = nullptr;

//   PGeoLineSegment *ls_end, *ls_tool;
//   if (end == 0) {
//     ls_end = new PGeoLineSegment(bl->vertices()[1], bl->vertices()[0]);
//   } else if (end == 1) {
//     ls_end = new PGeoLineSegment(bl->vertices()[bl->vertices().size() - 2],
//                                  bl->vertices()[bl->vertices().size() - 1]);
//   }

//   u1 = INF;
//   PDCELHalfEdge *hei = hel->incidentEdge();
//   double u1_tmp, u2_tmp;
//   bool not_parallel;
//   do {
//     ls_tool = new PGeoLineSegment(hei->source(), hei->target());
//     not_parallel = calcLineIntersection2D(ls_end, ls_tool, u1_tmp, u2_tmp);
//     if (!not_parallel) {
//       // If two line segments are parallel
//       // check if they are in the same line
//       SVector3 vec1, vec2;
//       vec1 = SVector3(ls_end->v1()->point(), ls_tool->v1()->point());
//       vec2 = SVector3(ls_end->v1()->point(), ls_tool->v2()->point());
//       if (crossprod(vec1, vec2).normSq() < TOLERANCE * TOLERANCE) {
//         if (!(ls_end->vout()->point() < ls_tool->vin()->point() ||
//               ls_tool->vout()->point() < ls_end->vin()->point())) {
//           not_parallel = true;
//           if (vec1.normSq() < vec2.normSq()) {
//             u1_tmp = ls_end->getParametricLocation(ls_tool->v1());
//             u2_tmp = 0;
//           } else {
//             u1_tmp = ls_end->getParametricLocation(ls_tool->v2());
//             u2_tmp = 1;
//           }
//         }
//       }
//     }

//     if (not_parallel && u2_tmp >= 0 && u2_tmp <= 1) {
//       if (u1_tmp >= 0 && u1_tmp < u1) {
//         u1 = u1_tmp;
//         u2 = u2_tmp;
//         he = hei;
//       }
//     }

//     hei = hei->next();
//   } while (hei != hel->incidentEdge());

//   return he;
// }










// Baseline *findCurvesIntersection(Baseline *bl, PGeoLineSegment *ls, int end,
//                                  double &u1, double &u2, int &iold, int &inew,
//                                  std::vector<int> &link_to_list) {
//   // After adjusting the curve
//   // the first kept vertex will be at index iold in the old curve
//   // and at the index inew in the new curve

//   // std::cout << "[debug] findCurvesIntersection" << std::endl;

//   // std::cout << "        curve bl:" << std::endl;
//   // for (auto v : bl->vertices()) {
//   //   std::cout << "        " << v << std::endl;
//   // }

//   iold = 0;
//   inew = 0;

//   Baseline *bl_new = new Baseline();
//   int n = bl->vertices().size();

//   std::vector<int> link_to_list_new;

//   std::list<PDCELVertex *> vlist;
//   std::list<int> link_to_list_copy;
//   for (int i = 0; i < n; ++i) {
//     vlist.push_back(bl->vertices()[i]);
//     link_to_list_copy.push_back(link_to_list[i]);
//   }
//   if (end == 1) {
//     vlist.reverse();
//     link_to_list_copy.reverse();
//   }
//   // std::cout << "        vertex list vlist:" << std::endl;
//   // for (auto v : vlist) {
//   //   std::cout << "        " << v << std::endl;
//   // }

//   PDCELVertex *v1, *v2, *v_new = nullptr;
//   PGeoLineSegment *lsi;
//   int link_i1, link_i2;
//   bool not_parallel;
//   while (!vlist.empty()) {
//     v1 = vlist.front();
//     vlist.pop_front();
//     v2 = vlist.front();
//     iold++;

//     link_i1 = link_to_list_copy.front();
//     link_to_list_copy.pop_front();
//     link_i2 = link_to_list_copy.front();

//     lsi = new PGeoLineSegment(v1, v2);

//     // std::cout << "        line segment lsi: " << lsi << std::endl;

//     not_parallel = calcLineIntersection2D(lsi, ls, u1, u2);

//     // std::cout << "        u1 = " << u1 << std::endl;

//     if (not_parallel) {
//       if (u1 > 1) {
//         continue;
//       } else {
//         if (fabs(u1) < TOLERANCE) {
//           vlist.push_front(v1);
//           link_to_list_copy.push_front(link_i1);
//           iold--;
//           break;
//         } else if (fabs(u1 - 1) < TOLERANCE) {
//           break;
//         } else if (u1 < 0) {
//           // In this case, the new curve is extended
//           // along the direction of the tangent at the end.
//           // The old vertex v1 will not be added back,
//           // instead, the intersecting point will be the new ending vertex.

//           // vlist.push_front(v1);
//           // iold--;
//           v_new = lsi->getParametricVertex(u1);
//           vlist.push_front(v_new);

//           link_to_list_copy.push_front(0);
//           inew++;
//           break;
//         } else {
//           v_new = lsi->getParametricVertex(u1);
//           vlist.push_front(v_new);
//           link_to_list_copy.push_front(0);
//           inew++;
//           break;
//         }
//       }
//     } else {
//       continue;
//     }
//   }

//   if (vlist.size() < 2) {
//     return nullptr;
//   }

//   while (!vlist.empty()) {
//     if (end == 0) {
//       bl_new->addPVertex(vlist.front());
//       vlist.pop_front();

//       link_to_list_new.push_back(link_to_list_copy.front());
//       link_to_list_copy.pop_front();
//     } else if (end == 1) {
//       bl_new->addPVertex(vlist.back());
//       vlist.pop_back();

//       link_to_list_new.push_back(link_to_list_copy.back());
//       link_to_list_copy.pop_back();
//       iold = n - 1 - iold;
//       inew = bl_new->vertices().size() - 1 - inew;
//     }
//   }

//   link_to_list.swap(link_to_list_new);

//   // std::cout << "        baseline bl_new" << std::endl;
//   // for (auto v : bl_new->vertices()) {
//   //   std::cout << "        " << v << std::endl;
//   // }

//   return bl_new;
// }
