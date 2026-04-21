#pragma once

#include "Material.hpp"
#include "PDCELHalfEdge.hpp"
#include "PDCELHalfEdgeLoop.hpp"
#include "PDCELVertex.hpp"
#include "PGeoClasses.hpp"
#include "PBaseLine.hpp"
#include "curve.hpp"
#include "geo_common.hpp"
#include "geo_math.hpp"
#include "polyline.hpp"
#include "utilities.hpp"

#include "geo_types.hpp"

// #include "gmsh/STensor3.h"

#include <cmath>
#include <list>
#include <string>
#include <vector>

template <typename P>
double dist(const P &p1, const P &p2) {
  return sqrt((p2 - p1).normSq());
}

// template <typename P>
// double calcPolylineLength(const std::vector<P> &ps) {
//   double len = 0;
//   for (auto i = 1; i < ps.size(); ++i) {
//     len += dist(ps[i], ps[i-1]);
//   }
//   return len;
// }

bool isClose(
  const double&, const double&,
  const double&, const double&,
  double, double);

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

bool calcLineIntersection2D(
  SPoint2, SPoint2, SPoint2, SPoint2,
  double &, double &, const double &);
bool calcLineIntersection2D(
  SPoint3, SPoint3, SPoint3, SPoint3,
  double &, double &, int &, const double &);
bool calcLineIntersection2D(
  PDCELVertex *, PDCELVertex *, PDCELVertex *, PDCELVertex *,
  double &, double &, const double &);
bool calcLineIntersection2D(
  PGeoLineSegment *, PGeoLineSegment *,
  double &, double &, const double &);

void offsetLineSegment(SPoint3 &, SPoint3 &, SVector3 &, double &);
PGeoLineSegment *offsetLineSegment(PGeoLineSegment *, int, double);
PGeoLineSegment *offsetLineSegment(PGeoLineSegment *, SVector3 &);
Baseline *offsetCurve(Baseline *, int, double);

/// Intersect two curves
/*!
  \param curve1 List of points representing the first curve
  \param curve1 List of points representing the second curve
  \param intersect The intersection point
 */
// int intersect(std::vector<PDCELVertex *> &curve1,
//               std::vector<PDCELVertex *> &curve2, PDCELVertex *intersect,
//               const int &keep1, const int &keep2);

int offset(PDCELVertex *v1_base, PDCELVertex *v2_base, int side, double dist,
           PDCELVertex *v1_off, PDCELVertex *v2_off);

/** @ingroup geo
 * Offset a list of vertices by a distance.
 * 
 * @param base Base line (list of vertices).
 * @param side Offset side.
 * @param dist Offset distance.
 * @param offset Resulting line (list of vertices).
 * @param link_to_list Links of corresponding vertices.
 */
int offset(const std::vector<PDCELVertex *> &base, int side, double dist,
           std::vector<PDCELVertex *> &offset, std::vector<int> &link_to_list_2,
           std::vector<std::vector<int>> &id_pairs);

// int offset2(const std::vector<PDCELVertex *> &base, int side, double dist,
//            std::vector<PDCELVertex *> &offset, std::vector<int> &link_offset_indices);

int intersect(PGeoLineSegment *subject, PGeoLineSegment *tool,
              PDCELVertex *&intersect);

/**
 * @brief Finds the intersection of curves within a given tolerance.
 *
 * This function iterates through the half-edges of a given half-edge loop (hel) 
 * and finds the intersection points with the provided vertices. It logs the 
 * process and updates the intersection parameters (u1, u2) and the index (ls_i).
 *
 * @param vertices  A vector of pointers to PDCELVertex objects representing the vertices.
 * @param hel       A pointer to a PDCELHalfEdgeLoop object representing the half-edge loop.
 * @param end       An integer indicating the end condition (0 for beginning, 1 for end).
 * @param ls_i      A reference to an integer that will be updated with the index of the line segment.
 * @param u1        A reference to a double that will be updated with the parametric location of the intersection on the curve.
 * @param u2        A reference to a double that will be updated with the parametric location of the intersection on the half-edge.
 * @param tol       A constant reference to a double representing the tolerance for intersection calculations.
 * @param pmessage  A pointer to a Message object used for logging.
 * @return          A pointer to the PDCELHalfEdge where the intersection was found, or nullptr if no intersection was found.
 */
PDCELHalfEdge *findCurveLoopIntersection(
  const std::vector<PDCELVertex *> &, PDCELHalfEdgeLoop *,
  int, int &, double &, double &, const double &);

Baseline *trimCurveAtLineSegment(
  Baseline *, PGeoLineSegment *, int, double &,
  double &, int &, int &, std::vector<int> &);

int findAllIntersections(
  const std::vector<PDCELVertex *> &c1, const std::vector<PDCELVertex *> &c2,
  std::vector<int> &i1s, std::vector<int> &i2s,
  std::vector<double> &u1s, std::vector<double> &u2s
);

/**
 * @brief Finds the intersection location closest to the specified end.
 *
 * This function iterates through the provided intersection indices and parametric locations
 * to find the intersection location that is closest to the specified end (beginning or ending side).
 * It updates the line segment index (ls_i) and the parametric location (u) accordingly.
 *
 * @param c           A reference to a vector of pointers to PDCELVertex objects representing the curve.
 * @param ii          A constant reference to a vector of integers representing the intersection indices.
 * @param uu          A reference to a vector of doubles representing the parametric locations of intersections.
 * @param which_end   A constant reference to an integer indicating the end to consider (0 for beginning, 1 for end).
 * @param inner_only  A constant reference to an integer indicating whether to consider only inner intersections (1) or not (0).
 * @param ls_i        A reference to an integer that will be updated with the index of the line segment.
 * @param j           A reference to an integer that will be updated with the index of the intersection.
 * @param pmessage    A pointer to a Message object used for logging.
 * @return            A double representing the parametric location of the closest intersection.
 */
double getIntersectionLocation(
  const std::vector<PDCELVertex *> &c,
  const std::vector<int> &ii, std::vector<double> &uu,
  const int &which_end, const int &inner_only,
  int &ls_i, int &j
);

PDCELVertex *getIntersectionVertex(
  std::vector<PDCELVertex *> &c1, std::vector<PDCELVertex *> &c2,
  int &i1, int &i2, const double &u1, const double &u2,
  int &is_new_1, int &is_new_2,
  const double &tol
);
