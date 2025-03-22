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
#include "plog.hpp"

#include "gmsh_mod/SPoint2.h"
#include "gmsh_mod/SPoint3.h"
#include "gmsh_mod/STensor3.h"
#include "gmsh_mod/SVector3.h"

#include "homog2d.hpp"

#include <cmath>
#include <cstdlib>
#include <list>
#include <string>
#include <vector>
#include <algorithm>


bool isClose(
  const double& p1x, const double& p1y,
  const double& p2x, const double& p2y,
  double absTol, double relTol) {
    auto isCloseValue = [absTol, relTol](double a, double b) -> bool {
        double diff = std::abs(a - b);
        double maxAbs = std::max(std::abs(a), std::abs(b));
        return diff <= std::max(absTol, relTol * maxAbs);
    };

    return isCloseValue(p1x, p2x) && isCloseValue(p1y, p2y);
}




/**
 * @brief Calculates the total length of a polyline.
 * 
 * This function computes the total length of a polyline represented by a vector of PDCELVertex pointers.
 * It iterates through the vector and sums up the distances between consecutive vertices.
 * 
 * @param ps A vector of pointers to PDCELVertex objects representing the polyline.
 * @return The total length of the polyline as a double.
 */
double calcPolylineLength(const std::vector<PDCELVertex *> ps) {
  double len = 0;
  for (auto i = 1; i < ps.size(); ++i) {
    // len += dist(ps[i]->point(), ps[i-1]->point());
    len += ps[i-1]->point().distance(ps[i]->point());
  }
  return len;
}




/// @brief Calculate the curvilinear length of a parametric coordinate (segment index, in-segment parametric coordinate) from the beginning of the polyline.
/// @param c The polyline.
/// @param i The index of the segment.
/// @param u The in-segment parametric coordinate.
/// @return The curvilinear length.
double calc_curve_length_of_segment_param_coord_from_start(
  const std::vector<PDCELVertex *> &c, const int &i, const double &u
) {
  double length = 0;
  for (auto j = 0; j <= i; j++) {
    double _l = calcDistanceSquared(c[j], c[j+1]);
    double _u = j < i ? 1 : u;
    length += std::sqrt(_l) * _u;
  }

  return length;
}





/**
 * @brief Finds a point on a polyline by a specified coordinate.
 *
 * This function searches through a polyline represented by a vector of PDCELVertex pointers
 * and finds a point that matches the given coordinate within a specified tolerance.
 *
 * @param ps A vector of pointers to PDCELVertex representing the polyline.
 * @param label A string label for the new point.
 * @param loc The coordinate value to search for along the polyline.
 * @param tol The tolerance within which the coordinate value should match.
 * @param param A reference to a double that will be set to the parameter value of the found point.
 * @param count The occurrence count of the coordinate value to find.
 * @param by A string specifying whether to search by "x2" or "x3" coordinate.
 * @return A pointer to the newly created PDCELVertex representing the found point.
 */
PDCELVertex *findPointOnPolylineByCoordinate(
  const std::vector<PDCELVertex *> &ps, const std::string label,
  const double loc, double tol, double &param,
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


/**
 * @brief Finds a point on a polyline by its coordinate.
 * 
 * This function searches for a point on a given polyline that matches the specified coordinate.
 * 
 * @param ps A vector of pointers to PDCELVertex objects representing the polyline.
 * @param label A string label associated with the point.
 * @param loc The coordinate location to search for.
 * @param tol The tolerance value for the coordinate search.
 * @param count An integer specifying the count of points to consider.
 * @param by A string specifying the method or criteria to use for the search.
 * @return A pointer to the PDCELVertex object that matches the specified coordinate.
 */
PDCELVertex *findPointOnPolylineByCoordinate(
  const std::vector<PDCELVertex *> &ps, const std::string label,
  const double loc,   double tol,
  const int count, const std::string by
) {
  double param{0};
  return findPointOnPolylineByCoordinate(ps, label, loc, tol, param, count, by); 
}


/**
 * @brief Finds a point on a polyline by a given coordinate.
 *
 * This function searches for a point on a polyline defined by a vector of vertices
 * based on a specified coordinate. It returns a parameter value corresponding to
 * the location of the point on the polyline.
 *
 * @param ps A vector of pointers to PDCELVertex objects representing the polyline vertices.
 * @param loc The coordinate value to search for on the polyline.
 * @param tol The tolerance value for the search.
 * @param count The number of points to consider in the search.
 * @param by A string specifying the method or criteria for the search.
 * @return The parameter value corresponding to the location of the point on the polyline.
 */
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








/**
 * @brief Finds a parameter point on a polyline.
 *
 * This function calculates a point on a polyline based on a given parameter `u`.
 * It also determines if the point is a new point or an existing vertex on the polyline.
 *
 * @param ps A vector of pointers to PDCELVertex representing the polyline vertices.
 * @param u The parameter value (0 <= u <= 1) indicating the relative position on the polyline.
 * @param is_new A reference to a boolean that will be set to true if the point is new, false otherwise.
 * @param seg A reference to an integer that will be set to the segment index where the point is found or inserted.
 * @param tol The tolerance value used for determining if the point is new.
 * @return A pointer to the PDCELVertex representing the found or newly created point.
 */
PDCELVertex *findParamPointOnPolyline(
  const std::vector<PDCELVertex *> ps,
  const double &u, bool &is_new, int &seg, const double &tol
  ) {
  // Calculate the total length
  double length = calcPolylineLength(ps);
  double ulength = u * length;

  std::size_t nlseg = ps.size() - 1;
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




bool get_vertex_on_polyline_by_segment_param_coord(
  std::vector<PDCELVertex *> &polyline, const int &i, const double &u,
  PDCELVertex *&v
) {
  PLOG(debug) << "get_vertex_on_polyline_by_segment_param_coord";

  PDCELVertex *v1 = polyline[i];
  PDCELVertex *v2 = polyline[i+1];

  bool is_new = get_vertex_by_param_coord_of_two_vertices(v1, v2, u, v);

  return is_new;
}









int getTurningSide(SVector3 vec1, SVector3 vec2) {
  // Check for zero vectors
  double norm1 = vec1.norm();
  double norm2 = vec2.norm();
  
  if (norm1 < ABS_TOL || norm2 < ABS_TOL) {
    PLOG(warning) << "getTurningSide: near-zero vector detected";
    return 0; // can't determine turning side with zero vectors
  }

  // Normalize vectors for more consistent results
  SVector3 v1 = vec1 * (1.0/norm1);
  SVector3 v2 = vec2 * (1.0/norm2);
  
  SVector3 n = crossprod(v1, v2);
  
  // Use a more robust check with absolute tolerance
  if (fabs(n[0]) < ABS_TOL) {
    // Check if vectors are parallel or anti-parallel
    double _dot = dot(v1, v2);
    if (_dot > 0) {
      return 0; // parallel (same direction)
    } else {
      return 0; // anti-parallel (opposite direction)
    }
  } else if (n[0] > ABS_TOL) {
    return 1; // left turn
  } else if (n[0] < -ABS_TOL) {
    return -1; // right turn
  } else {
    return 0; // too close to determine reliably
  }
}










double calcDistanceSquared(PDCELVertex *v1, PDCELVertex *v2) {
  // Check for null pointers
  if (v1 == nullptr || v2 == nullptr) {
    PLOG(error) << "calcDistanceSquared: null vertex pointer";
    return std::numeric_limits<double>::max();
  }
  
  // Calculate squared differences
  double dx = v1->x() - v2->x();
  double dy = v1->y() - v2->y();
  double dz = v1->z() - v2->z();

  // Return squared distance
  return dx * dx + dy * dy + dz * dz;
}




/// @brief Check if two vertices are close to each other
/// @param v1 The first vertex
/// @param v2 The second vertex
/// @return True if the vertices are close to each other, false otherwise
bool is_close(PDCELVertex *v1, PDCELVertex *v2) {
  return calcDistanceSquared(v1, v2) < ABS_TOL * ABS_TOL;
}





/// @brief Replace a vertex in a vector of vertices with a new vertex
/// @param vertices The vector of vertices
/// @param old_v The old vertex to be replaced
/// @param new_v The new vertex to replace the old vertex
void replace_vertex(
  std::vector<PDCELVertex *> &vertices, PDCELVertex *old_v, PDCELVertex *new_v
) {
  for (auto &v : vertices) {
    if (v == old_v) {
      v = new_v;
      PLOG(debug) << "replaced vertex " << old_v << " with " << new_v;
      return;
    }
  }
  PLOG(error) << "vertex " << old_v << " not found in vector";
}




/**
 * @brief Joins a list of Baseline curves into a single Baseline.
 *
 * This function takes a list of Baseline pointers and joins them into a single
 * Baseline object. The first Baseline in the list is used as the starting point,
 * and subsequent Baselines are joined based on the matching vertices.
 *
 * @param curves A list of Baseline pointers to be joined.
 * @return A pointer to the newly created Baseline that is the result of joining
 *         all the input Baselines.
 */
Baseline *joinCurves(std::list<Baseline *> curves) {
  // std::cout << "[debug] joining curves" << std::endl;

  Baseline *bl;
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









/**
 * @brief Joins multiple Baseline curves into a single Baseline.
 *
 * This function takes a Baseline object and a list of Baseline pointers,
 * and joins the curves in the list into the provided Baseline object.
 * The function modifies the provided Baseline object by adding vertices
 * from the curves and joining them based on their vertex positions.
 *
 * @param line A pointer to the Baseline object that will be modified to include the joined curves.
 * @param curves A list of pointers to Baseline objects that will be joined into the provided Baseline object.
 * @return An integer indicating the success of the operation. Currently, always returns 0.
 */
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









/**
 * @brief Adjusts the end of a curve segment to ensure it intersects with another line segment.
 *
 * This function modifies the end of a baseline curve segment to ensure it intersects with a given line segment.
 * It creates a new line segment from the baseline's vertices and tangent vectors, calculates the intersection
 * point with the given line segment, and updates the baseline's vertices accordingly.
 *
 * @param bl Pointer to the Baseline object containing the curve segment to be adjusted.
 * @param ls Pointer to the PGeoLineSegment object representing the line segment to intersect with.
 * @param end Integer indicating which end of the baseline curve segment to adjust (0 for the beginning, 1 for the end).
 */
void adjustCurveEnd(Baseline *bl, PGeoLineSegment *ls, int end) {

  PLOG(debug) << "adjustCurveEnd";

  PGeoLineSegment *ls_end;
  PDCELVertex *vnew;
  if (end == 0) {
    ls_end = new PGeoLineSegment(
      bl->vertices().front(), bl->getTangentVectorBegin());
    double u1, u2;
    bool is_intersect = calc_line_intersection_2d(
      ls_end->v1(), ls_end->v2(),
      ls->v1(), ls->v2(),
      vnew, u1, u2, 1, 0, 1, 1
    );
    bl->vertices()[0] = vnew;
  } else if (end == 1) {
    ls_end = new PGeoLineSegment(
      bl->vertices().back(), bl->getTangentVectorEnd());
    double u1, u2;
    bool is_intersect = calc_line_intersection_2d(
      ls_end->v1(), ls_end->v2(),
      ls->v1(), ls->v2(),
      vnew, u1, u2, 0, 1, 1, 1
    );
    bl->vertices()[bl->vertices().size() - 1] = vnew;
  }

  // calcLineIntersection2D(ls_end, ls, u1, u2, TOLERANCE);
  // PDCELVertex *vnew = ls_end->getParametricVertex(u1);

  // if (end == 0) {
  //   bl->vertices()[0] = vnew;
  // } else if (end == 1) {
  //   bl->vertices()[bl->vertices().size() - 1] = vnew;
  // }
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




bool get_vertex_by_param_coord_of_two_vertices(
  PDCELVertex *v1, PDCELVertex *v2, const double &u,
  PDCELVertex *v
) {
  PLOG(debug) << "get_vertex_by_param_coord_of_two_vertices";

  bool is_new = false;

  if (is_close(u, 0.0)) {
    // Vertex is the beginning point (v1) of the line segment i
    PLOG(debug) << "  vertex is close to v1";
    is_new = false;
    v = v1;
  }
  else if (is_close(u, 1.0)) {
    // Vertex is the ending point (v2) of the line segment i
    PLOG(debug) << "  vertex is close to v2";
    is_new = false;
    v = v2;
  }
  else {
    // Vertex is in the interior of the line segment i
    PLOG(debug) << "  vertex is new";
    is_new = true;

    double x1 = v1->x(), y1 = v1->y(), z1 = v1->z();
    double x2 = v2->x(), y2 = v2->y(), z2 = v2->z();

    v = new PDCELVertex(
      x1 + u * (x2 - x1),
      y1 + u * (y2 - y1),
      z1 + u * (z2 - z1)
    );
  }

  return is_new;
}









/// @brief Calculate the curvilinear coordinate of a vertex on a polyline.
/// @param c The polyline.
/// @param v The vertex.
/// @return The curvilinear coordinate of the vertex.
double calc_curv_coord_of_vertex_on_polyline(
  const std::vector<PDCELVertex *> &c, const PDCELVertex *v
  ) {
  double length = 0;

  if (v != c.front()) {
    for (auto i = 1; i < c.size(); i++) {
      double _l = calcDistanceSquared(c[i-1], c[i]);

      length += std::sqrt(_l);

      if (v == c[i]) {
        break;
      }
    }
  }

  return length;
}









bool isParallel(PGeoLineSegment *ls1, PGeoLineSegment *ls2) {
  SVector3 vec1, vec2, vecn;
  vec1 = ls1->toVector();
  vec2 = ls2->toVector();

  vecn = crossprod(vec1, vec2);
  if (vecn.normSq() < ABS_TOL * ABS_TOL) {
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
  if (vecn.normSq() < ABS_TOL * ABS_TOL) {
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









/**
 * @brief Calculates the line bisecting the angle of <p1p0p2.
 *
 * Given three points, p0, p1, p2, calculate the line bisecting the
 * angle of <p1p0p2, and return the vector parallel this line.
 *
 * @param[in] p0 The first point.
 * @param[in] p1 The second point.
 * @param[in] p2 The third point.
 *
 * @return The vector parallel to the bisecting line.
 */
SVector3 calcAngleBisectVector(SPoint3 &p0, SPoint3 &p1, SPoint3 &p2) {
  SVector3 v1{p0, p1}, v2{p0, p2}, vb;
  // Make the two vectors the same length
  v1 *= 1 / v1.normSq();
  v2 *= 1 / v2.normSq();
  // Calculate the bisecting vector by adding the two vectors
  vb = v1 + v2;

  return vb;
}










/**
 * @brief Calculates the line bisecting the angle of <p1p0p2.
 *
 * Given two vectors, v1, v2, and the layup side (left or right) of
 * each vector, calculate the line bisecting the angle of <p1p0p2, and
 * return the vector parallel this line.
 *
 * @param[in] v1 The first vector.
 * @param[in] v2 The second vector.
 * @param[in] s1 The layup side (left or right) of v1.
 * @param[in] s2 The layup side (left or right) of v2.
 *
 * @return The vector parallel to the bisecting line.
 */
SVector3 calcAngleBisectVector(SVector3 &v1, SVector3 &v2, std::string s1,
                               std::string s2) {
  // s1, s2 are the layup side (left or right) of v1, v2, respectively.
  SVector3 n1{1, 0, 0}, n2{1, 0, 0}, p1, p2;
  if (s1 == "right") {
    n1 = -1 * n1;
  }
  // Calculate the vector perpendicular to v1 on the layup side s1
  p1 = crossprod(n1, v1).unit();

  if (s2 == "right") {
    n2 = -1 * n2;
  }
  // Calculate the vector perpendicular to v2 on the layup side s2
  p2 = crossprod(n2, v2).unit();

  // Calculate the bisecting vector by adding the two vectors
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
  std::size_t m, n;
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









/// @brief Trim the curve at the vertex v
/// @param c The curve
/// @param v The vertex
/// @param remove The direction to remove (0: beginning, 1: ending)
/// @return 0
int trim(std::vector<PDCELVertex *> &c, PDCELVertex *v, const int &remove) {
  // Trim the curve c at the vertex v
  // Remove the portion that is closer to the specified end (beginning or ending)
  // remove: beginning (0) or ending (1)

  // Check for invalid inputs
  if (c.empty() || v == nullptr || (remove != 0 && remove != 1)) {
    PLOG(warning) << "trim: Invalid input parameters";
    return -1;
  }

  // Check if vertex exists in the curve
  bool vertex_found = false;
  for (auto vertex : c) {
    if (vertex == v) {
      vertex_found = true;
      break;
    }
  }
  
  if (!vertex_found) {
    PLOG(warning) << "trim: Vertex not found in curve";
    return -1;
  }

  // Split the curve
  std::vector<PDCELVertex *> tmp_c1, tmp_c2;
  int s = 0;

  for (size_t i = 0; i < c.size(); i++) {
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

  // Ensure we found the vertex and split the curve
  if (tmp_c1.size() <= 1 || tmp_c2.size() <= 1) {
    PLOG(warning) << "trim: Vertex at curve endpoint or curve too short";
    return -1;
  }

  c.clear();
  if (remove == 0) {
    c.assign(tmp_c2.begin(), tmp_c2.end());
  }
  else if (remove == 1) {
    c.assign(tmp_c1.begin(), tmp_c1.end());
  }

  return 0;
}



std::vector<std::vector<int>> create_polyline_vertex_pairs(
  const std::vector<PDCELVertex *> &base_curve,
  const std::vector<PDCELVertex *> &paired_curve) {

    std::vector<std::vector<int>> pairs;
    if (base_curve.empty() || paired_curve.empty()) return pairs;

    // Start from the beginning of the paired_curve
    int last_paired_index = 0;
    
    // Iterate over each point on the base curve
    for (size_t i = 0; i < base_curve.size(); ++i) {
        int best_index = last_paired_index;

        h2d::Point2d base_point(base_curve[i]->y(), base_curve[i]->z());
        h2d::Point2d paired_point(
          paired_curve[last_paired_index]->y(), paired_curve[last_paired_index]->z());

        // double best_dist = squaredDistance(base_curve[i], paired_curve[last_paired_index]);
        double best_dist = h2d::dist(base_point, paired_point);
        double prev_dist = best_dist;
        
        // Iterate over the paired_curve starting from the last paired index + 1
        for (size_t j = last_paired_index + 1; j < paired_curve.size(); ++j) {
            h2d::Point2d curr_paired_point(paired_curve[j]->y(), paired_curve[j]->z());
            // double curr_dist = squaredDistance(base_curve[i], paired_curve[j]);
            double curr_dist = h2d::dist(base_point, curr_paired_point);
            // If the current point is closer, update the best_index and distance.
            if (curr_dist < prev_dist) {
                best_index = static_cast<int>(j);
                best_dist = curr_dist;
                prev_dist = curr_dist;
            } else {
                // If the current distance is larger than the previous one, stop the search.
                break;
            }
        }
        
        // Update last_paired_index for the next iteration over base_curve.
        last_paired_index = best_index;
        pairs.push_back({static_cast<int>(i), best_index});
    }
    
    return pairs;

  }
