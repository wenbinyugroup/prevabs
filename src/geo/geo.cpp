#include "geo.hpp"

#include "Material.hpp"
#include "PDCELHalfEdge.hpp"
#include "PDCELHalfEdgeLoop.hpp"
#include "PDCELVertex.hpp"
#include "PGeoClasses.hpp"
#include "globalConstants.hpp"
#include "globalVariables.hpp"
#include "PBaseLine.hpp"
#include "overloadOperator.hpp"
#include "utilities.hpp"

#include "gmsh/SPoint2.h"
#include "gmsh/SPoint3.h"
#include "gmsh/STensor3.h"
#include "gmsh/SVector3.h"

#include <cmath>
#include <cstdlib>
#include <list>
#include <string>
#include <vector>


double calcPolylineLength(const std::vector<PDCELVertex *> ps) {
  double len = 0;
  for (auto i = 1; i < ps.size(); ++i) {
    // len += dist(ps[i]->point(), ps[i-1]->point());
    len += ps[i-1]->point().distance(ps[i]->point());
  }
  return len;
}

// This function finds a point and its parametrized location on a baseline
// based on its coordinate (x2 or x3) and the count for intersection
PDCELVertex *findPointOnPolylineByCoordinate(
  const std::vector<PDCELVertex *> &ps, const std::string label,
  const double loc,   double tol,double &param,
  const int count, const std::string by 
) {
  PDCELVertex *pv = new PDCELVertex(label);
  double length = calcPolylineLength(ps);
  double ulength{0};
  int counter{0};
  double left_x2, left_x3, right_x2, right_x3;

  for (auto i = 0; i < ps.size() - 1; i++) {
    if (by == "x2") {
      left_x2 = ps[i]->y(); left_x3 = ps[i]->z();
      right_x2 = ps[i+1]->y(); right_x3 = ps[i+1]->z();
    } else if (by == "x3") {
      // switch inputs to tackle partion by x3
      left_x3 = ps[i]->y(); left_x2 = ps[i]->z();
      right_x3 = ps[i+1]->y(); right_x2 = ps[i+1]->z();
    } else {
      std::cout << markError << " Point should be specified by x2 or x3 coordinate"
          << std::endl;
    }; 
    if (
    (left_x2 <= loc && loc <= right_x2) ||
    (left_x2 >= loc && loc >= right_x2)
    ) {
      counter++;
      // Find the new point
      if (counter == count) {
        if (fabs(loc - left_x2) < tol) {
          pv->setLinkToVertex(ps[i]);
          break;
        }
        else if (fabs(loc - right_x2) < tol) {
          ulength += ps[i]->point().distance(ps[i+1]->point());
          pv->setLinkToVertex(ps[i+1]);
          break;
        }
        else {
          double dy, dz, loc2, dl;
          dl = ps[i]->point().distance(ps[i+1]->point());
          dy = right_x2 - left_x2;
          dz = right_x3 - left_x3;
          loc2 = dz / dy * (loc - left_x2) + left_x3;
          ulength +=  dl * (loc - left_x2) / dy;
          if (by == "x2") {
            pv->setPosition(0, loc, loc2);
          } else {
            pv->setPosition(0, loc2, loc);
          }
          break;
        }
      }
    }
    ulength += ps[i]->point().distance(ps[i+1]->point());
  }
  param = ulength / length;
  return pv;
}

// overloading in case you don't need parametrized location
PDCELVertex *findPointOnPolylineByCoordinate(
  const std::vector<PDCELVertex *> &ps, const std::string label,
  const double loc,   double tol,
  const int count, const std::string by
) {
  double param{0};
  return findPointOnPolylineByCoordinate(ps, label, loc, tol, param, count, by); 
}

// overloading in case you don't need the point
double findPointOnPolylineByCoordinate(
  const std::vector<PDCELVertex *> &ps,
  const double loc,   double tol,
  const int count, const std::string by 
) {
  double param{0};
  std::string label{"newp"};
  findPointOnPolylineByCoordinate(ps, label, loc, tol, param, count, by); 
  return param;
}








PDCELVertex *findParamPointOnPolyline(
  const std::vector<PDCELVertex *> ps,
  const double &u, bool &is_new, int &seg, const double &tol
  ) {
  // Calculate the total length
  double length = calcPolylineLength(ps);
  double ulength = u * length;

  int nlseg = ps.size() - 1;
  double ui = 0, li;
  // int i;
  for (seg = 0; seg < nlseg; ++seg) {
    // li = dist(ps[seg], ps[seg+1]);
    li = ps[seg]->point().distance(ps[seg+1]->point());
    if (ulength > li) {
      ulength -= li;
    }
    else break;
  }
  ui = ulength / li;
  SPoint3 newp = calcPointFromParam(
    ps[seg]->point(), ps[seg+1]->point(), ui, is_new, tol
  );
  if (!is_new) {
    if (ui < 0.5) {
      return ps[seg];
    }
    else if (ui > 0.5) {
      return ps[seg+1];
    }
  }
  PDCELVertex *newv = new PDCELVertex(newp);
  seg += 1;  // index to insert the new vertex
  return newv;
}









int getTurningSide(SVector3 vec1, SVector3 vec2) {
  SVector3 n;
  n = crossprod(vec1, vec2);

  if (fabs(n[0]) < TOLERANCE) {
    return 0; // collinear
  } else if (n[0] > 0) {
    return 1; // left
  } else {
    return -1; // right
  }
}










double calcDistanceSquared(PDCELVertex *v1, PDCELVertex *v2) {
  double dx = v1->x() - v2->x();
  double dy = v1->y() - v2->y();
  double dz = v1->z() - v2->z();

  return dx * dx + dy * dy + dz * dz;
}










Baseline *joinCurves(std::list<Baseline *> curves) {
  // std::cout << "[debug] joining curves" << std::endl;

  Baseline *bl, *bl_tmp;
  bl = new Baseline(curves.front());
  bl->setName(bl->getName() + "_new");
  curves.pop_front();

  while (!curves.empty()) {
    std::list<Baseline *>::iterator blit;
    for (blit = curves.begin(); blit != curves.end(); ++blit) {
      if ((*blit)->vertices().front() == bl->vertices().front()) {
        // bl_tmp = new Baseline(*blit);
        // bl_tmp->setName(bl_tmp->getName() + "_r");
        bl->join((*blit), 0, true);
        break;
      } else if ((*blit)->vertices().front() == bl->vertices().back()) {
        bl->join((*blit), 1, false);
        break;
      } else if ((*blit)->vertices().back() == bl->vertices().front()) {
        bl->join((*blit), 0, false);
        break;
      } else if ((*blit)->vertices().back() == bl->vertices().back()) {
        // bl_tmp = new Baseline(*blit);
        // bl_tmp->setName(bl_tmp->getName() + "_r");
        bl->join((*blit), 1, true);
        break;
      }
    }
    curves.remove(*blit);
  }

  // std::cout << "line bl:" << std::endl;
  // for (auto v : bl->vertices()) {
  //   std::cout << v->point() << std::endl;
  // }

  return bl;
}









int joinCurves(Baseline *line, std::list<Baseline *> curves) {
  // std::cout << "[debug] joining curves" << std::endl;

  // Baseline *bl, *bl_tmp;
  // bl = new Baseline(curves.front());
  // bl->setName(bl->getName() + "_new");
  for (auto v : curves.front()->vertices()) {
    line->addPVertex(v);
  }
  curves.pop_front();

  while (!curves.empty()) {
    std::list<Baseline *>::iterator blit;
    for (blit = curves.begin(); blit != curves.end(); ++blit) {
      if ((*blit)->vertices().front() == line->vertices().front()) {
        // bl_tmp = new Baseline(*blit);
        // bl_tmp->setName(bl_tmp->getName() + "_r");
        line->join((*blit), 0, true);
        break;
      } else if ((*blit)->vertices().front() == line->vertices().back()) {
        line->join((*blit), 1, false);
        break;
      } else if ((*blit)->vertices().back() == line->vertices().front()) {
        line->join((*blit), 0, false);
        break;
      } else if ((*blit)->vertices().back() == line->vertices().back()) {
        // bl_tmp = new Baseline(*blit);
        // bl_tmp->setName(bl_tmp->getName() + "_r");
        line->join((*blit), 1, true);
        break;
      }
    }
    curves.remove(*blit);
  }

  return 0;
}









void adjustCurveEnd(Baseline *bl, PGeoLineSegment *ls, int end) {
  PGeoLineSegment *ls_end;
  if (end == 0) {
    ls_end = new PGeoLineSegment(bl->vertices().front(),
                                 bl->getTangentVectorBegin());
  } else if (end == 1) {
    ls_end =
        new PGeoLineSegment(bl->vertices().back(), bl->getTangentVectorEnd());
  }

  double u1, u2;
  calcLineIntersection2D(ls_end, ls, u1, u2);
  PDCELVertex *vnew = ls_end->getParametricVertex(u1);

  if (end == 0) {
    bl->vertices()[0] = vnew;
  } else if (end == 1) {
    bl->vertices()[bl->vertices().size() - 1] = vnew;
  }
}










SVector3 getVectorFromAngle(double &angle, const int &plane) {
  SVector3 v;

  // Map the angle into the range (-90, 270]
  while (angle <= -90 || angle > 270) {
    if (angle <= -90) {
      angle += 360;
    } else if (angle > 270) {
      angle -= 360;
    }
  }

  double reverse = 1.0;
  if (angle > 90) {
    reverse = -1.0;
  }

  double d1, d2;
  if (angle == 90.0 || angle == 270.0) {
    d1 = 0.0;
    d2 = 1.0;
  } else {
    d1 = 1.0;
    d2 = tan(deg2rad(angle));
  }

  if (plane == 0) {
    // y-z plane
    v = SVector3(0, d1, d2);
  } else if (plane == 1) {
    // z-x plane
    v = SVector3(d2, 0, d1);
  } else if (plane == 2) {
    // x-y plane
    v = SVector3(d1, d2, 0);
  }

  return v * reverse;
}










SPoint3 getParametricPoint(const SPoint3 &p1, const SPoint3 &p2, double u) {
  // std::cout << "\n[debug] function: getParametricPoint\n";

  if (fabs(u) < TOLERANCE) {
    return SPoint3(p1);
  } else if (fabs(u - 1) < TOLERANCE) {
    return SPoint3(p2);
  } else {
    double x, y, z;

    x = p1.x() + u * (p2.x() - p1.x());
    y = p1.y() + u * (p2.y() - p1.y());
    z = p1.z() + u * (p2.z() - p1.z());

    return SPoint3(x, y, z);
  }
}









bool isParallel(PGeoLineSegment *ls1, PGeoLineSegment *ls2) {
  SVector3 vec1, vec2, vecn;
  vec1 = ls1->toVector();
  vec2 = ls2->toVector();

  vecn = crossprod(vec1, vec2);
  if (vecn.normSq() < TOLERANCE * TOLERANCE) {
    return true;
  } else {
    return false;
  }
}










bool isCollinear(PGeoLineSegment *ls1, PGeoLineSegment *ls2) {
  if (!isParallel(ls1, ls2)) {
    return false;
  }

  SVector3 vec1, vec2, vecn;
  vec1 = ls1->toVector();
  vec2 = SVector3(ls1->v1()->point(), ls2->v1()->point());
  vecn = crossprod(vec1, vec2);
  if (vecn.normSq() < TOLERANCE * TOLERANCE) {
    return true;
  } else {
    return false;
  }
}











bool isOverlapped(PGeoLineSegment *ls1, PGeoLineSegment *ls2) {
  if (ls1->vout()->point() < ls2->vin()->point() ||
      ls2->vout()->point() < ls1->vin()->point()) {
    return false;
  }

  if (!isCollinear(ls1, ls2)) {
    return false;
  }

  return true;
}









SVector3 calcAngleBisectVector(SPoint3 &p0, SPoint3 &p1, SPoint3 &p2) {
  // Given three points, p0, p1, p2, calculate the line bisecting the
  // angle of <p1p0p2, and return the vector parallel this line.
  SVector3 v1{p0, p1}, v2{p0, p2}, vb;
  // Make the two vectors the same length
  v1 *= 1 / v1.normSq();
  v2 *= 1 / v2.normSq();
  vb = v1 + v2;

  return vb;
}










SVector3 calcAngleBisectVector(SVector3 &v1, SVector3 &v2, std::string s1,
                               std::string s2) {
  // s1, s2 are the layup side (left or right) of v1, v2, respectively.
  SVector3 n1{1, 0, 0}, n2{1, 0, 0}, p1, p2;
  if (s1 == "right") {
    n1 = -1 * n1;
  }
  p1 = crossprod(n1, v1).unit();

  if (s2 == "right") {
    n2 = -1 * n2;
  }
  p2 = crossprod(n2, v2).unit();

  return p1 + p2;
}










void calcBoundVertices(std::vector<PDCELVertex *> &vertices,
                       SVector3 &sv_baseline, SVector3 &sv_bound,
                       Layup *layup) {
  // Given the baseline direction, bound direction and layup,
  // calculate vertices dividing layers on the bound and
  // store them into the repository `vertices`
  SVector3 n = crossprod(sv_bound, sv_baseline);
  SVector3 p = crossprod(sv_baseline, n);
  p.normalize();
  sv_bound.normalize();

  // std::cout << "p: " << p.point() << std::endl;

  PDCELVertex *pv_prev, *pv_new;
  double thk, thkp;
  for (int i = 0; i < layup->getLayers().size(); ++i) {
    thk = layup->getLayers()[i].getLamina()->getThickness() *
          layup->getLayers()[i].getStack();
    thkp = thk / dot(sv_bound, p);
    pv_prev = vertices.back();
    // SPoint3 p_new = pv_prev->point() + (thkp * sv_bound).point();
    // std::cout << "thkp * sv_bound = " << (thkp * sv_bound).point() <<
    // std::endl;
    SPoint3 p_new = (SVector3(pv_prev->point()) + thkp * sv_bound).point();
    pv_new = new PDCELVertex();
    pv_new->setPoint(p_new);
    vertices.push_back(pv_new);
  }
}










void combineVertexLists(std::vector<PDCELVertex *> &vl_1,
                        std::vector<PDCELVertex *> &vl_2,
                        std::vector<int> &vi_1, std::vector<int> &vi_2,
                        std::vector<PDCELVertex *> &vl_c) {
  int m, n;
  m = vl_1.size();
  n = vl_2.size();

  // Decide which dimension (y or z) is used to sort
  int d;
  if (abs(vl_1.front()->y() - vl_1.back()->y()) >=
      abs(vl_1.front()->z() - vl_1.back()->z()))
    d = 1; // d is x2 (i.e. y)
  else
    d = 2; // d is x3 (i.e. z)

  // std::cout << d << std::endl;

  std::string order;
  if ((vl_1.front()->point()[d] - vl_1.back()->point()[d]) < 0.0)
    order = "asc";
  else
    order = "des";

  // std::cout << order << std::endl;

  bool b_reverse_2;
  if ((vl_1.front()->point()[d] - vl_1.back()->point()[d]) *
          (vl_2.front()->point()[d] - vl_2.back()->point()[d]) >
      0.0)
    b_reverse_2 = false;
  else
    b_reverse_2 = true;
  if (b_reverse_2)
    std::reverse(vl_2.begin(), vl_2.end());

  // std::cout << b_reverse_2 << std::endl;

  // std::vector<PDCELVertex *> vl_c;
  int i{0}, j{0}, k{0};
  while (i < m && j < n) {
    double diff{vl_1[i]->point()[d] - vl_2[j]->point()[d]};
    if ((order == "asc" && diff < (-TOLERANCE)) ||
        (order == "des" && diff > TOLERANCE)) {
      // vl_c[k] = vl_1[i];
      vl_c.push_back(vl_1[i]);
      vi_1.push_back(k);
      i++;
    } else if ((order == "asc" && diff > TOLERANCE) ||
               (order == "des" && diff < (-TOLERANCE))) {
      // vl_c[k] = vl_2[j];
      vl_c.push_back(vl_2[j]);
      vi_2.push_back(k);
      j++;
    } else if (abs(diff) <= TOLERANCE) {
      // vl_c[k] = vl_1[i];
      vl_c.push_back(vl_1[i]);
      vi_1.push_back(k);
      vi_2.push_back(k);
      i++;
      j++;
    }
    k++;
  }
  if (i < m) {
    for (int p = i; p < m; ++p) {
      // vl_c[k] = vl_1[p];
      vl_c.push_back(vl_1[p]);
      vi_1.push_back(k);
      k++;
    }
  } else if (j < n) {
    for (int p = j; p < n; ++p) {
      // vl_c[k] = vl_2[p];
      vl_c.push_back(vl_2[p]);
      vi_2.push_back(k);
      k++;
    }
  }

  // std::cout << "vl_c:" << std::endl;
  // for (auto v : vl_c) {
  //   std::cout << v << std::endl;
  // }

  // std::cout << "vi_1:" << std::endl;
  // for (auto i : vi_1) {
  //   std::cout << i << std::endl;
  // }

  // std::cout << "vi_2:" << std::endl;
  // for (auto i : vi_2) {
  //   std::cout << i << std::endl;
  // }

  if (b_reverse_2)
    std::reverse(vi_2.begin(), vi_2.end());

  return;
}









int trim(std::vector<PDCELVertex *> &c, PDCELVertex *v, const int &remove) {
  // Trim the curve c at the vertex v
  // Remove the one that is closer to the end remove

  // remove: beginning (0) or ending (1)

  // std::cout << "\n[debug] function: trim\n";

  // Split the curve
  std::vector<PDCELVertex *> tmp_c1, tmp_c2;

  int s = 0;

  for (auto i = 0; i < c.size(); i++) {
    if (c[i] == v) {
      tmp_c1.push_back(c[i]);
      tmp_c2.push_back(c[i]);
      s = 1;
    }
    else {
      if (s == 0) {
        tmp_c1.push_back(c[i]);
      }
      else if (s == 1) {
        tmp_c2.push_back(c[i]);
      }
    }
  }

  c.clear();
  if (remove == 0) {
    for (auto v : tmp_c2) c.push_back(v);
  }
  else if (remove == 1) {
    for (auto v : tmp_c1) c.push_back(v);
  }

  // std::cout << "resulting curve\n";
  // for (auto v : c) std::cout << v << std::endl;

  return 0;
}




