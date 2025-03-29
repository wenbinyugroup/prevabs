#pragma once

#include "Material.hpp"
#include "PDCELHalfEdge.hpp"
#include "PDCELHalfEdgeLoop.hpp"
#include "PDCELVertex.hpp"
#include "PGeoLine.hpp"
#include "PBaseLine.hpp"
#include "utilities.hpp"

#include "gmsh_mod/SPoint2.h"
#include "gmsh_mod/SPoint3.h"
#include "gmsh_mod/SVector3.h"
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





double calcPolylineLength(const std::vector<PDCELVertex *>);

double calc_curve_length_of_segment_param_coord_from_start(const std::vector<PDCELVertex *> &, const int &, const double &);

template <typename P>
P calcPointFromParam(const P &p1, const P &p2, const double &u, bool &is_new, const double &tol) {
  // std::cout << "\nfabs(u) = " << fabs(u);
  // std::cout << ", fabs(u - 1) = " << fabs(u - 1);
  // std::cout << ", tol = " << tol << std::endl;
  if (fabs(u) < tol) {
    is_new = false;
    return P(p1);
  } else if (fabs(u - 1) < tol) {
    is_new = false;
    return P(p2);
  } else {
    is_new = true;
    return P(p1 + u * (p2 - p1));
  }
}





// template <typename P>
// P findParamPointOnPolyline(const std::vector<P> &ps, const double &u, bool &is_new, const double &tol) {
//   // Calculate the total length
//   double length = calcPolylineLength(ps);
//   double ulength = u * length;

//   int nlseg = ps.size() - 1;
//   double ui = 0, li;
//   int i;
//   for (i = 0; i < nlseg; ++i) {
//     li = dist(ps[i], ps[i+1]);
//     if (ulength > li) {
//       ulength -= li;
//     }
//     else break;
//   }
//   ui = ulength / li;
//   P newp = calcPointFromParam(ps[i], ps[i+1], ui, is_new, tol);
//   return newp;
// }





PDCELVertex *findParamPointOnPolyline(
  const std::vector<PDCELVertex *>,
  const double &, bool &, int &, const double &
);

PDCELVertex *get_vertex_on_polyline_by_segment_param_coord(std::vector<PDCELVertex *> &polyline, const int &i, const double &u, bool &is_new);


PDCELVertex *findPointOnPolylineByCoordinate(
  const std::vector<PDCELVertex *> &, const std::string ,
  const double ,   double ,double &,
  const int count = 1, const std::string by = "x2"
); 

PDCELVertex *findPointOnPolylineByCoordinate(
  const std::vector<PDCELVertex *> &, const std::string,
  const double , double , 
  const int count = 1, const std::string by = "x2"
);

double findPointOnPolylineByCoordinate(
  const std::vector<PDCELVertex *> &,
  const double ,   double ,
  const int count = 1, const std::string by = "x2" 
);



bool calcLineIntersection2D(
  const double &, const double &, const double &, const double &,
  const double &, const double &, const double &, const double &,
  double &, double &, const double &
  );

bool calc_line_intersection_2d(
  const double &, const double &, const double &, const double &,
  const double &, const double &, const double &, const double &,
  double &, double &, double &, double &
  );

// template <typename P2>
// bool calcLineIntersection2D(
//   const PGeoPoint2 &, const PGeoPoint2 &, const PGeoPoint2 &, const PGeoPoint2 &,
//   double &, double &, const double &
// );

// template <typename P3>
// bool calcLineIntersection2D(
//   const PGeoPoint3 &, const PGeoPoint3 &, const PGeoPoint3 &, const PGeoPoint3 &,
//   double &, double &, const int &, const double &
// );

// bool calcLineIntersection2D(
//   SPoint2, SPoint2, SPoint2, SPoint2,
//   double &, double &, const double &);
// bool calcLineIntersection2D(
//   SPoint3, SPoint3, SPoint3, SPoint3,
//   double &, double &, int &, const double &);
// bool calcLineIntersection2D(
//   PDCELVertex *, PDCELVertex *, PDCELVertex *, PDCELVertex *,
//   double &, double &, const double &);
// bool calc_line_intersection_2d(
//   PDCELVertex *, PDCELVertex *, PDCELVertex *, PDCELVertex *,
//   double &, double &, double &, double &);
bool calc_line_intersection_2d(
  PDCELVertex *ls1v1, PDCELVertex *ls1v2,
  PDCELVertex *ls2v1, PDCELVertex *ls2v2,
  PDCELVertex *&v_intersect, double &u1, double &u2,
  const int &ex11, const int &ex12, const int &ex21, const int &ex22
  );
// bool calcLineIntersection2D(
//   PGeoLineSegment *, PGeoLineSegment *,
//   double &, double &, const double &);











int getTurningSide(SVector3, SVector3);

double calcDistanceSquared(PDCELVertex *, PDCELVertex *);

bool is_close(PDCELVertex *, PDCELVertex *);

void replace_vertex(std::vector<PDCELVertex *> &, PDCELVertex *, PDCELVertex *);

/// Connect a list of baselines into a single one
Baseline *joinCurves(std::list<Baseline *>);
int joinCurves(Baseline *, std::list<Baseline *>);

/// Trim or extend the end of the curve by the line segment
/*!
  \param bl The baseline that will be adjusted
  \param ls The tool line segment
  \param end Indicate which end to be adjusted (0: head; 1: tail)
 */
void adjustCurveEnd(Baseline *bl, PGeoLineSegment *ls, int end);

/// Calculate the direction vector given an angle in a plane.
/*!
  \param angle The angle in degree of the vector counted from (1, 0)
  \param plane The plane considered (0(x)/1(y)/2(z))
  \return The directional vector
 */
SVector3 getVectorFromAngle(double &angle, const int &plane);

SPoint3 getParametricPoint(const SPoint3 &p1, const SPoint3 &p2, double u);
PDCELVertex *get_vertex_by_param_coord_of_two_vertices(PDCELVertex *v1, PDCELVertex *v2, const double &u, bool &is_new);


double calc_curv_coord_of_vertex_on_polyline(const std::vector<PDCELVertex *> &c, const PDCELVertex *v);

bool isParallel(PGeoLineSegment *, PGeoLineSegment *);
bool isCollinear(PGeoLineSegment *, PGeoLineSegment *);
bool isOverlapped(PGeoLineSegment *, PGeoLineSegment *);

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
           std::vector<std::vector<int>> &id_pairs, Message *pmessage);

int offset_2(const std::vector<PDCELVertex *> &base, int side, double dist,
           std::vector<PDCELVertex *> &offset, Message *pmessage);




SVector3 calcAngleBisectVector(SPoint3 &, SPoint3 &, SPoint3 &);
SVector3 calcAngleBisectVector(SVector3 &, SVector3 &, std::string,
                               std::string);

void calcBoundVertices(std::vector<PDCELVertex *> &, SVector3 &, SVector3 &,
                       Layup *);

void combineVertexLists(std::vector<PDCELVertex *> &,
                        std::vector<PDCELVertex *> &, std::vector<int> &,
                        std::vector<int> &, std::vector<PDCELVertex *> &);

void insert_vertex_by_line_segment_param_coord(
  std::vector<PDCELVertex *> &pl, PDCELVertex *v, const int &lsi, const double &lsu);

// int intersect(PGeoLineSegment *subject, PGeoLineSegment *tool,
//               PDCELVertex *intersect);

PDCELVertex *get_polylines_intersection_close_to(
  std::vector<PDCELVertex *> &polyline_1, std::vector<PDCELVertex *> &polyline_2,
  const double &param_loc_1, const double &param_loc_2,
  const int &ex11, const int &ex12, const int &ex21, const int &ex22,
  int &i1, double &u1, int &i2, double &u2, bool &is_new_1, bool &is_new_2,
  Message *pmessage);

bool calc_all_intersections_between_polyline_and_half_edge_loop(std::vector<PDCELVertex *> &polyline, PDCELHalfEdgeLoop *hel, const int &ex1, const int &ex2, std::vector<int> &i1s, std::vector<int> &i2s, std::vector<double> &u1s, std::vector<double> &u2s, Message *pmessage);

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
// PDCELHalfEdge *findCurvesIntersection(
//   std::vector<PDCELVertex *>, PDCELHalfEdgeLoop *, int, int &, double &, double &, const double &,
//   Message *);
PDCELHalfEdge *find_curves_intersection(
  std::vector<PDCELVertex *> , PDCELHalfEdgeLoop *,
  const int &, const int &, int &, double &, double &,
  Message *
  );

// Baseline *findCurvesIntersection(
//   Baseline *, PGeoLineSegment *, int, double &,
//   double &, int &, int &, std::vector<int> &, std::vector<int> &);

// int findAllIntersections(
//   const std::vector<PDCELVertex *> &c1, const std::vector<PDCELVertex *> &c2,
//   std::vector<int> &i1s, std::vector<int> &i2s,
//   std::vector<double> &u1s, std::vector<double> &u2s
// );

int find_open_polylines_intersections(
  const std::vector<PDCELVertex *> &, const std::vector<PDCELVertex *> &,
  std::vector<int> &, std::vector<int> &,
  std::vector<double> &, std::vector<double> &,
  const int &, const int &, const int &, const int &,
  Message *
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
// double getIntersectionLocation(
//   std::vector<PDCELVertex *> &c,
//   const std::vector<int> &ii, std::vector<double> &uu,
//   const int &which_end, const int &inner_only,
//   int &ls_i, int &j, Message *
// );

int get_intersection_close_to_param_coord(
  const std::vector<PDCELVertex *> &c,
  const std::vector<int> &ii, const std::vector<double> &uu,
  const double &param_coord, const int &ex1, const int &ex2, const int &side,
  Message *pmessage);

int get_intersection_closer_to(
    const std::vector<PDCELVertex *> &,
    const std::vector<int> &, const std::vector<double> &,
    const int &, const bool &,
    Message *);

// PDCELVertex *getIntersectionVertex(
//   std::vector<PDCELVertex *> &c1, std::vector<PDCELVertex *> &c2,
//   int &i1, int &i2, const double &u1, const double &u2,
//   const int &which_end_1, const int &which_end_2,
//   const int &inner_only_1, const int &inner_only_2,
//   int &is_new_1, int &is_new_2,
//   const double &tol
// );

PDCELVertex *get_intersection_vertex(
  std::vector<PDCELVertex *> &, std::vector<PDCELVertex *> &,
  int &, int &, const double &, const double &,
  Message *
);

int trim(std::vector<PDCELVertex *> &c, PDCELVertex *v, const int &remove);

std::vector<std::vector<int>> create_polyline_vertex_pairs(
  const std::vector<PDCELVertex *> &,
  const std::vector<PDCELVertex *> &);
