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

#include "geo_types.hpp"

#include <cmath>
#include <cstdlib>
#include <limits>
#include <list>
#include <string>
#include <vector>
#include <algorithm>

namespace {

const char *toAxisString(PolylineAxis axis) {
  return axis == PolylineAxis::X2 ? "x2" : "x3";
}

const char *toEndString(CurveEnd end) {
  return end == CurveEnd::Begin ? "begin" : "end";
}

}  // namespace

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
double calcPolylineLength(const std::vector<PDCELVertex *> &ps) {
  double len = 0;
  for (auto i = 1; i < ps.size(); ++i) {
    // len += dist(ps[i]->point(), ps[i-1]->point());
    len += ps[i-1]->point().distance(ps[i]->point());
  }
  return len;
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
PDCELVertex *findPolylinePointByCoordinate(
  const std::vector<PDCELVertex *> &ps, const std::string label,
  const double loc,   double tol,double &param,
  const int count, const PolylineAxis axis
) {
  param = 0.0;
  if (ps.size() < 2) {
    std::cout << markError
              << " findPolylinePointByCoordinate: polyline needs at least"
              << " two vertices" << std::endl;
    return nullptr;
  }

  if (count < 1) {
    std::cout << markError
              << " findPolylinePointByCoordinate: count must be >= 1"
              << std::endl;
    return nullptr;
  }

  double length = calcPolylineLength(ps);
  if (length <= tol) {
    std::cout << markError
              << " findPolylinePointByCoordinate: polyline length is zero"
              << std::endl;
    return nullptr;
  }

  PDCELVertex *pv = new PDCELVertex(label);
  double ulength{0};
  int counter{0};
  double left_x2{0.0}, left_x3{0.0}, right_x2{0.0}, right_x3{0.0};
  bool found{false};

  for (std::size_t i = 0; i + 1 < ps.size(); ++i) {
    double dl = ps[i]->point().distance(ps[i+1]->point());
    if (axis == PolylineAxis::X2) {
      left_x2 = ps[i]->y(); left_x3 = ps[i]->z();
      right_x2 = ps[i+1]->y(); right_x3 = ps[i+1]->z();
    } else {
      // switch inputs to tackle partion by x3
      left_x3 = ps[i]->y(); left_x2 = ps[i]->z();
      right_x3 = ps[i+1]->y(); right_x2 = ps[i+1]->z();
    }
    if (
    (left_x2 <= loc && loc <= right_x2) ||
    (left_x2 >= loc && loc >= right_x2)
    ) {
      counter++;
      // Find the new point
      if (counter == count) {
        if (fabs(loc - left_x2) < tol) {
          pv->setPosition(ps[i]->x(), ps[i]->y(), ps[i]->z());
          break;
        }
        else if (fabs(loc - right_x2) < tol) {
          ulength += dl;
          pv->setPosition(ps[i+1]->x(), ps[i+1]->y(), ps[i+1]->z());
          found = true;
          break;
        }
        else {
          double dy, dz, loc2;
          dy = right_x2 - left_x2;
          if (std::abs(dy) < tol) {
            std::cout << markError
                      << " findPolylinePointByCoordinate: coordinate "
                      << toAxisString(axis)
                      << " is constant on the matched segment"
                      << std::endl;
            delete pv;
            return nullptr;
          }
          dz = right_x3 - left_x3;
          loc2 = dz / dy * (loc - left_x2) + left_x3;
          ulength +=  dl * (loc - left_x2) / dy;
          if (axis == PolylineAxis::X2) {
            pv->setPosition(0, loc, loc2);
          } else {
            pv->setPosition(0, loc2, loc);
          }
          found = true;
          break;
        }
      }
    }
    ulength += dl;
  }

  if (!found) {
    std::cout << markError
              << " findPolylinePointByCoordinate: failed to find coordinate "
              << loc << " on polyline by " << toAxisString(axis)
              << " (count = " << count << ")" << std::endl;
    delete pv;
    return nullptr;
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
PDCELVertex *findPolylinePointByCoordinate(
  const std::vector<PDCELVertex *> &ps, const std::string label,
  const double loc,   double tol,
  const int count, const PolylineAxis axis
) {
  double param{0};
  return findPolylinePointByCoordinate(
      ps, label, loc, tol, param, count, axis);
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
double findPolylineParamByCoordinate(
  const std::vector<PDCELVertex *> &ps,
  const double loc,   double tol,
  const int count, const PolylineAxis axis
) {
  double param{0};
  std::string label{"newp"};
  PDCELVertex *pv =
      findPolylinePointByCoordinate(ps, label, loc, tol, param, count, axis);
  if (pv == nullptr) {
    return std::numeric_limits<double>::quiet_NaN();
  }
  delete pv;
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
PDCELVertex *findPolylinePointAtParam(
  const std::vector<PDCELVertex *> &ps,
  const double &u, bool &is_new, int &seg, const double &tol
  ) {
  // Bug fix: li is used after the loop; if ps has fewer than 2 vertices the
  // loop body never executes and li would be uninitialized. Return nullptr so
  // the caller can detect the degenerate input.
  if (ps.size() < 2) {
    is_new = false;
    seg = 0;
    return nullptr;
  }

  if (u < -tol || u > 1.0 + tol) {
    std::cout << markError
              << " findPolylinePointAtParam: parameter u = " << u
              << " is outside [0, 1]" << std::endl;
    is_new = false;
    seg = 0;
    return nullptr;
  }

  // Calculate the total length
  double length = calcPolylineLength(ps);
  if (length <= tol) {
    std::cout << markError
              << " findPolylinePointAtParam: polyline length is zero"
              << std::endl;
    is_new = false;
    seg = 0;
    return nullptr;
  }

  if (u <= tol) {
    is_new = false;
    seg = 0;
    return ps.front();
  }
  if (u >= 1.0 - tol) {
    is_new = false;
    seg = static_cast<int>(ps.size()) - 1;
    return ps.back();
  }

  double ulength = u * length;

  std::size_t nlseg = ps.size() - 1;
  double ui = 0, li = 0.0;
  bool found_segment{false};
  for (std::size_t i = 0; i < nlseg; ++i) {
    li = ps[i]->point().distance(ps[i+1]->point());
    if (li <= tol) {
      continue;
    }
    if (ulength > li) {
      ulength -= li;
    }
    else {
      seg = static_cast<int>(i);
      found_segment = true;
      break;
    }
  }

  if (!found_segment) {
    is_new = false;
    seg = static_cast<int>(ps.size()) - 1;
    return ps.back();
  }

  ui = ulength / li;
  SPoint3 newp = calcPointFromParam(
    ps[seg]->point(), ps[seg+1]->point(), ui, is_new, tol
  );
  if (!is_new) {
    if (ui <= 0.5) {
      return ps[seg];
    }
    return ps[seg+1];
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
  if (curves.empty()) {
    std::cout << markError << " joinCurves: no curves were provided"
              << std::endl;
    return nullptr;
  }

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
    // Bug fix: if no curve in the remaining list shares an endpoint with bl,
    // blit reaches end() and dereferencing it is UB; the outer while loop
    // would also spin forever. Break out and report the disconnected input.
    if (blit == curves.end()) {
      std::cout << markError << " joinCurves: disconnected curve set,"
                << " could not join all segments" << std::endl;
      delete bl;
      return nullptr;
    }
    curves.erase(blit);
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
  if (line == nullptr) {
    std::cout << markError << " joinCurves: output baseline must be valid"
              << std::endl;
    return -1;
  }
  if (line->vertices().empty() == false) {
    std::cout << markError << " joinCurves: output baseline must start empty"
              << std::endl;
    return -1;
  }
  if (curves.empty()) {
    std::cout << markError << " joinCurves: no curves were provided"
              << std::endl;
    return -1;
  }

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
    // Bug fix: same as the Baseline* overload above — guard against a
    // disconnected curve set that would leave blit at end() and loop forever.
    if (blit == curves.end()) {
      std::cout << markError << " joinCurves: disconnected curve set,"
                << " could not join all segments" << std::endl;
      return -1;
    }
    curves.erase(blit);
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
int adjustCurveEnd(Baseline *bl, PGeoLineSegment *ls, CurveEnd end) {
  // Bug fix: previously ls_end was left uninitialized when end is neither 0
  // nor 1, and was never deleted (memory leak). Use a local pointer with an
  // explicit delete before every return path.
  if (bl == nullptr || ls == nullptr) {
    std::cout << markError
              << " adjustCurveEnd: baseline and tool line must be valid"
              << std::endl;
    return -1;
  }
  if (bl->vertices().size() < 2) {
    std::cout << markError
              << " adjustCurveEnd: baseline needs at least two vertices"
              << std::endl;
    return -1;
  }

  PGeoLineSegment *ls_end;
  if (end == CurveEnd::Begin) {
    ls_end = new PGeoLineSegment(bl->vertices().front(),
                                 bl->getTangentVectorBegin());
  } else {
    ls_end =
        new PGeoLineSegment(bl->vertices().back(), bl->getTangentVectorEnd());
  }

  double u1, u2;
  bool has_intersection = calcLineIntersection2D(ls_end, ls, u1, u2, TOLERANCE);
  if (!has_intersection) {
    std::cout << markError
              << " adjustCurveEnd: failed to intersect baseline tangent with"
              << " tool line" << std::endl;
    delete ls_end;
    return -1;
  }
  PDCELVertex *vnew = ls_end->getParametricVertex(u1);
  delete ls_end;

  if (end == CurveEnd::Begin) {
    bl->vertices()[0] = vnew;
  } else {
    bl->vertices()[bl->vertices().size() - 1] = vnew;
  }
  return 0;
}

SVector3 getVectorFromAngle(double angle, AnglePlane plane) {
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

  if (plane == AnglePlane::YZ) {
    // y-z plane
    v = SVector3(0, d1, d2);
  } else if (plane == AnglePlane::ZX) {
    // z-x plane
    v = SVector3(d2, 0, d1);
  } else {
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

SVector3 calcAngleBisectVector(
  const SPoint3 &p0, const SPoint3 &p1, const SPoint3 &p2) {
  // Given three points, p0, p1, p2, calculate the line bisecting the
  // angle of <p1p0p2, and return the vector parallel this line.
  SVector3 v1{p0, p1}, v2{p0, p2}, vb;
  // Make the two vectors unit length so the bisector is unweighted.
  // Bug fix: was `v1 *= 1 / v1.normSq()` which divides by |v|^2 instead of
  // |v|, biasing the bisector toward shorter edges.
  v1.normalize();
  v2.normalize();
  vb = v1 + v2;

  return vb;
}

SVector3 calcAngleBisectVector(
  const SVector3 &v1, const SVector3 &v2,
  const std::string &s1, const std::string &s2) {
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

int calcBoundVertices(std::vector<PDCELVertex *> &vertices,
                      const SVector3 &sv_baseline,
                      const SVector3 &sv_bound, Layup *layup) {
  // Given the baseline direction, bound direction and layup,
  // calculate vertices dividing layers on the bound and
  // store them into the repository `vertices`
  if (layup == nullptr) {
    std::cout << markError
              << " calcBoundVertices: layup must be valid" << std::endl;
    return -1;
  }
  if (vertices.empty()) {
    std::cout << markError
              << " calcBoundVertices: vertices must contain the starting point"
              << std::endl;
    return -1;
  }

  SVector3 bound_dir = sv_bound;
  SVector3 n = crossprod(bound_dir, sv_baseline);
  SVector3 p = crossprod(sv_baseline, n);
  p.normalize();
  bound_dir.normalize();

  // std::cout << "p: " << p.point() << std::endl;

  double dp = dot(bound_dir, p);
  if (std::abs(dp) < TOLERANCE) {
    std::cout << markError << " calcBoundVertices: bound direction is"
              << " perpendicular to the layer normal" << std::endl;
    return -1;
  }

  PDCELVertex *pv_prev, *pv_new;
  double thk, thkp;
  for (int i = 0; i < layup->getLayers().size(); ++i) {
    thk = layup->getLayers()[i].getLamina()->getThickness() *
          layup->getLayers()[i].getStack();
    thkp = thk / dp;
    pv_prev = vertices.back();
    // SPoint3 p_new = pv_prev->point() + (thkp * sv_bound).point();
    // std::cout << "thkp * sv_bound = " << (thkp * sv_bound).point() <<
    // std::endl;
    SPoint3 p_new = (SVector3(pv_prev->point()) + thkp * bound_dir).point();
    pv_new = new PDCELVertex();
    pv_new->setPoint(p_new);
    vertices.push_back(pv_new);
  }
  return 0;
}

int mergeSortedVertexLists(const std::vector<PDCELVertex *> &vl_1,
                           const std::vector<PDCELVertex *> &vl_2,
                           std::vector<int> &vi_1, std::vector<int> &vi_2,
                           std::vector<PDCELVertex *> &vl_c) {
  std::size_t m, n;
  m = vl_1.size();
  n = vl_2.size();
  int k{static_cast<int>(vl_c.size())};

  if (m == 0 && n == 0) {
    return 0;
  }
  if (m == 0) {
    for (std::size_t p = 0; p < n; ++p) {
      vl_c.push_back(vl_2[p]);
      vi_2.push_back(k++);
    }
    return 0;
  }
  if (n == 0) {
    for (std::size_t p = 0; p < m; ++p) {
      vl_c.push_back(vl_1[p]);
      vi_1.push_back(k++);
    }
    return 0;
  }

  // Decide which dimension (y or z) is used to sort
  // Bug fix: bare abs() resolves to C integer abs() when <cstdlib> is
  // included, truncating doubles to int before the absolute value.
  // Use std::abs() (from <cmath>) throughout.
  int d;
  if (std::abs(vl_1.front()->y() - vl_1.back()->y()) >=
      std::abs(vl_1.front()->z() - vl_1.back()->z()))
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
  std::vector<PDCELVertex *> vl_2_ordered = vl_2;
  if (b_reverse_2)
    std::reverse(vl_2_ordered.begin(), vl_2_ordered.end());

  // std::cout << b_reverse_2 << std::endl;

  // std::vector<PDCELVertex *> vl_c;
  int i{0}, j{0};
  while (i < m && j < n) {
    double diff{vl_1[i]->point()[d] - vl_2_ordered[j]->point()[d]};
    if ((order == "asc" && diff < (-TOLERANCE)) ||
        (order == "des" && diff > TOLERANCE)) {
      // vl_c[k] = vl_1[i];
      vl_c.push_back(vl_1[i]);
      vi_1.push_back(k);
      i++;
    } else if ((order == "asc" && diff > TOLERANCE) ||
               (order == "des" && diff < (-TOLERANCE))) {
      // vl_c[k] = vl_2[j];
      vl_c.push_back(vl_2_ordered[j]);
      vi_2.push_back(k);
      j++;
    } else if (std::abs(diff) <= TOLERANCE) {
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
      vl_c.push_back(vl_2_ordered[p]);
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

  return 0;
}

int trimCurveAtVertex(std::vector<PDCELVertex *> &c, PDCELVertex *v,
                      CurveEnd remove) {
  // Trim the curve c at the vertex v
  // Remove the one that is closer to the end remove

  // remove: beginning or ending

  // std::cout << "\n[debug] function: trim\n";

  if (v == nullptr) {
    std::cout << markError << " trim: trim vertex must be valid" << std::endl;
    return -1;
  }

  std::vector<PDCELVertex *>::iterator it = std::find(c.begin(), c.end(), v);
  if (it == c.end()) {
    std::cout << markError << " trim: trim vertex was not found on the curve"
              << std::endl;
    return -1;
  }

  if (remove == CurveEnd::Begin) {
    c.assign(it, c.end());
  } else {
    c.assign(c.begin(), it + 1);
  }

  // std::cout << "resulting curve\n";
  // for (auto v : c) std::cout << v << std::endl;

  return 0;
}
