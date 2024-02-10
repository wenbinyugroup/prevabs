#pragma once

// #include "Material.hpp"
// #include "PDCELHalfEdge.hpp"
// #include "PDCELHalfEdgeLoop.hpp"
// #include "PDCELVertex.hpp"
#include "PGeoClasses.hpp"
// #include "internalClasses.hpp"

// #include "gmsh/SPoint2.h"
// #include "gmsh/SPoint3.h"
// #include "gmsh/STensor3.h"
// #include "gmsh/SVector3.h"

#include <cmath>
#include <list>
#include <string>
#include <vector>

// int getTurningSide(SVector3, SVector3);

// double calcDistanceSquared(PDCELVertex *, PDCELVertex *);

/// Connect a list of baselines into a single one
// Baseline *joinCurves(std::list<Baseline *>);

/// Trim or extend the end of the curve by the line segment
/*!
  \param bl The baseline that will be adjusted
  \param ls The tool line segment
  \param end Indicate which end to be adjusted (0: head; 1: tail)
 */
// void adjustCurveEnd(Baseline *bl, PGeoLineSegment *ls, int end);

/// Calculate the direction vector given an angle in a plane.
/*!
  \param angle The angle in degree of the vector counted from (1, 0)
  \param plane The plane considered (0(x)/1(y)/2(z))
  \return The directional vector
 */
// SVector3 getVectorFromAngle(double &angle, const int &plane);

// SPoint3 getParametricPoint(const SPoint3 &p1, const SPoint3 &p2, double u);

template <typename P>
double dist(const P &p1, const P &p2) {
  return sqrt((p2 - p1).normSq());
}





template <typename P>
double calcPolylineLength(const std::vector<P> &ps) {
  double len = 0;
  for (auto i = 1; i < ps.size(); ++i) {
    len += dist(ps[i], ps[i-1]);
  }
  return len;
}





template <typename P>
P calcPointFromParam(const P &p1, const P &p2, const double &u, const double &tol) {
  if (fabs(u) < tol) {
    return P(p1);
  } else if (fabs(u - 1) < tol) {
    return P(p2);
  } else {
    return P(p1 + u * (p2 - p1));
  }
}





template <typename P>
P findParamPointOnPolyline(const std::vector<P> &ps, const double &u, const double &tol) {
  // Calculate the total length
  double length = calcPolylineLength(ps);
  double ulength = u * length;

  int nlseg = ps.size() - 1;
  double ui = 0, li;
  int i;
  for (i = 0; i < nlseg; ++i) {
    li = dist(ps[i], ps[i+1]);
    if (ulength > li) {
      ulength -= li;
    }
    else break;
  }
  ui = ulength / li;
  P newp = calcPointFromParam(ps[i], ps[i+1], ui, tol);
  return newp;
}





bool calcLineIntersection2D(
  const double &, const double &, const double &, const double &,
  const double &, const double &, const double &, const double &,
  double &, double &, const double &
  );

// template <typename P2>
bool calcLineIntersection2D(
  const PGeoPoint2 &, const PGeoPoint2 &, const PGeoPoint2 &, const PGeoPoint2 &,
  double &, double &, const double &
);

// template <typename P3>
bool calcLineIntersection2D(
  const PGeoPoint3 &, const PGeoPoint3 &, const PGeoPoint3 &, const PGeoPoint3 &,
  double &, double &, const int &, const double &
);

// bool calcLineIntersection2D(SPoint3, SPoint3, SPoint3, SPoint3, double &,
//                             double &, int &);
// bool calcLineIntersection2D(PDCELVertex *, PDCELVertex *, PDCELVertex *,
//                             PDCELVertex *, double &, double &);
// bool calcLineIntersection2D(PGeoLineSegment *, PGeoLineSegment *, double &,
//                             double &);

// bool isParallel(PGeoLineSegment *, PGeoLineSegment *);
// bool isCollinear(PGeoLineSegment *, PGeoLineSegment *);
// bool isOverlapped(PGeoLineSegment *, PGeoLineSegment *);

// void offsetLineSegment(SPoint3 &, SPoint3 &, SVector3 &, double &);
// PGeoLineSegment *offsetLineSegment(PGeoLineSegment *, int, double);
// PGeoLineSegment *offsetLineSegment(PGeoLineSegment *, SVector3 &);
// Baseline *offsetCurve(Baseline *, int, double);

/// Intersect two curves
/*!
  \param curve1 List of points representing the first curve
  \param curve1 List of points representing the second curve
  \param intersect The intersection point
 */
// int intersect(std::vector<PDCELVertex *> &curve1,
//               std::vector<PDCELVertex *> &curve2, PDCELVertex *intersect,
//               const int &keep1, const int &keep2);

// int offset(PDCELVertex *v1_base, PDCELVertex *v2_base, int side, double dist,
//            PDCELVertex *v1_off, PDCELVertex *v2_off);

/*!
  Offset a list of vertices by a distance.

  @param base Base line (list of vertices).
  @param side Offset side.
  @param dist Offset distance.
  @param offset Resulting line (list of vertices).
  @param link_to_list Links of corresponding vertices.
*/
// int offset(const std::vector<PDCELVertex *> &base, int side, double dist,
//            std::vector<PDCELVertex *> &offset, std::vector<int> &link_to_list);

// SVector3 calcAngleBisectVector(SPoint3 &, SPoint3 &, SPoint3 &);
// SVector3 calcAngleBisectVector(SVector3 &, SVector3 &, std::string,
//                                std::string);

// void calcBoundVertices(std::vector<PDCELVertex *> &, SVector3 &, SVector3 &,
//                        Layup *);

// void combineVertexLists(std::vector<PDCELVertex *> &,
//                         std::vector<PDCELVertex *> &, std::vector<int> &,
//                         std::vector<int> &, std::vector<PDCELVertex *> &);

// int intersect(PGeoLineSegment *subject, PGeoLineSegment *tool,
//               PDCELVertex *intersect);

// PDCELHalfEdge *findCurvesIntersection(Baseline *, PDCELHalfEdgeLoop *, int,
//                                       double &, double &);

// Baseline *findCurvesIntersection(Baseline *, PGeoLineSegment *, int, double &,
//                                  double &, int &, int &, std::vector<int> &);
