#include "globalConstants.hpp"
#include "geo.hpp"
#include "utilities.hpp"
#include "plog.hpp"
#include <string>
#include <sstream>
#include "homog2d.hpp"


/**
 * @brief Calculates the intersection of two 2D lines.
 *
 * This function determines whether two lines in 2D space intersect and, if so, 
 * calculates the intersection parameters u1 and u2.
 *
 * @param l1p1x The x-coordinate of the first point of the first line.
 * @param l1p1y The y-coordinate of the first point of the first line.
 * @param l1p2x The x-coordinate of the second point of the first line.
 * @param l1p2y The y-coordinate of the second point of the first line.
 * @param l2p1x The x-coordinate of the first point of the second line.
 * @param l2p1y The y-coordinate of the first point of the second line.
 * @param l2p2x The x-coordinate of the second point of the second line.
 * @param l2p2y The y-coordinate of the second point of the second line.
 * @param u1 Reference to a double where the intersection parameter for the first line will be stored.
 * @param u2 Reference to a double where the intersection parameter for the second line will be stored.
 * @param tol The tolerance value used to determine if the lines are parallel.
 * @return true if the lines intersect, false if they are parallel within the given tolerance.
 */
bool calcLineIntersection2D(
  const double &l1p1x, const double &l1p1y, const double &l1p2x, const double &l1p2y,
  const double &l2p1x, const double &l2p1y, const double &l2p2x, const double &l2p2y,
  double &u1, double &u2, const double &tol
  ) {

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




/**
 * @brief Calculates the intersection of two 2D lines.
 *
 * This function determines if two lines in 2D space intersect and calculates the
 * intersection points. It also calculates the parametric coordinates of the
 * intersection point on each line.
 *
 * @param l1p1x The x-coordinate of the first point of the first line.
 * @param l1p1y The y-coordinate of the first point of the first line.
 * @param l1p2x The x-coordinate of the second point of the first line.
 * @param l1p2y The y-coordinate of the second point of the first line.
 * @param l2p1x The x-coordinate of the first point of the second line.
 * @param l2p1y The y-coordinate of the first point of the second line.
 * @param l2p2x The x-coordinate of the second point of the second line.
 * @param l2p2y The y-coordinate of the second point of the second line.
 * @param ipx Reference to a double where the x-coordinate of the intersection point will be stored.
 * @param ipy Reference to a double where the y-coordinate of the intersection point will be stored.
 * @param u1 Reference to a double where the parametric coordinate of the intersection point on the first line will be stored.
 * @param u2 Reference to a double where the parametric coordinate of the intersection point on the second line will be stored.
 * @return true if the lines intersect, false otherwise.
 */
bool calc_line_intersection_2d(
  const double &l1p1x, const double &l1p1y, const double &l1p2x, const double &l1p2y,
  const double &l2p1x, const double &l2p1y, const double &l2p2x, const double &l2p2y,
  double &ipx, double &ipy, double &u1, double &u2
  ) {

  h2d::Point2d l1p1(l1p1x, l1p1y);
  h2d::Point2d l1p2(l1p2x, l1p2y);
  h2d::Point2d l2p1(l2p1x, l2p1y);
  h2d::Point2d l2p2(l2p2x, l2p2y);

  h2d::Line2d l1(l1p1, l1p2);
  h2d::Line2d l2(l2p1, l2p2);

  auto res = l1.intersects(l2);

  if (!res()) {
    return false;
  }

  // Intersect
  auto pts = res.get();
  ipx = pts.getX();
  ipy = pts.getY();

  // Calculate parametric coordinates
  u1 = h2d::dist(l1p1, pts) / h2d::dist(l1p1, l1p2);
  u2 = h2d::dist(l2p1, pts) / h2d::dist(l2p1, l2p2);

  return true;

}




/**
 * @brief Calculates the intersection of two 2D lines.
 *
 * This function determines if two lines in 2D space intersect and calculates the intersection points.
 *
 * @param l1p1 The first point of the first line.
 * @param l1p2 The second point of the first line.
 * @param l2p1 The first point of the second line.
 * @param l2p2 The second point of the second line.
 * @param u1 Output parameter that will hold the intersection parameter for the first line.
 * @param u2 Output parameter that will hold the intersection parameter for the second line.
 * @param tol The tolerance value used for intersection calculations.
 * @return True if the lines intersect, false otherwise.
 */
// bool calcLineIntersection2D(
//   const PGeoPoint2 &l1p1, const PGeoPoint2 &l1p2,
//   const PGeoPoint2 &l2p1, const PGeoPoint2 &l2p2,
//   double &u1, double &u2, const double &tol
//   ) {

//   return calcLineIntersection2D(
//     l1p1[0], l1p1[1], l1p2[0], l1p2[1],
//     l2p1[0], l2p1[1], l2p2[0], l2p2[1],
//     u1, u2, tol
//   );

// }





/**
 * @brief Calculates the intersection of two lines in 2D space.
 *
 * This function determines the intersection point of two lines defined by their endpoints
 * in a specified 2D plane. The lines are represented by their endpoints in 3D space, and
 * the function projects them onto the specified plane before calculating the intersection.
 *
 * @param l1p1 The first endpoint of the first line (3D point).
 * @param l1p2 The second endpoint of the first line (3D point).
 * @param l2p1 The first endpoint of the second line (3D point).
 * @param l2p2 The second endpoint of the second line (3D point).
 * @param u1 Output parameter that will hold the intersection parameter for the first line.
 * @param u2 Output parameter that will hold the intersection parameter for the second line.
 * @param plane The plane onto which the lines are projected (0 for XY, 1 for YZ, 2 for ZX).
 * @param tol The tolerance value for determining intersection.
 * @return True if the lines intersect, false otherwise.
 */
// bool calcLineIntersection2D(
//   const PGeoPoint3 &l1p1, const PGeoPoint3 &l1p2,
//   const PGeoPoint3 &l2p1, const PGeoPoint3 &l2p2,
//   double &u1, double &u2, const int &plane, const double &tol
//   ) {
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
//   return calcLineIntersection2D(
//     l1p1[d1], l1p1[d2], l1p2[d1], l1p2[d2],
//     l2p1[d1], l2p1[d2], l2p2[d1], l2p2[d2],
//     u1, u2, tol
//   );
// }




// bool calcLineIntersection2D(
//   SPoint2 l1p1, SPoint2 l1p2, SPoint2 l2p1, SPoint2 l2p2,
//   double &u1, double &u2, const double &tol
//   ) {

//   return calcLineIntersection2D(
//     l1p1[0], l1p1[1], l1p2[0], l1p2[1],
//     l2p1[0], l2p1[1], l2p2[0], l2p2[1],
//     u1, u2, tol
//   );

// }




/**
 * @brief Calculates the intersection of two 2D lines projected from 3D space.
 *
 * This function determines the intersection point of two lines in 2D space,
 * which are projections of 3D lines onto a specified plane. The plane is
 * defined by the `plane` parameter, where 0 represents the XY plane, 1
 * represents the YZ plane, and 2 represents the ZX plane.
 *
 * @param l1p1 The first point of the first line in 3D space.
 * @param l1p2 The second point of the first line in 3D space.
 * @param l2p1 The first point of the second line in 3D space.
 * @param l2p2 The second point of the second line in 3D space.
 * @param u1 Reference to a double where the parameter of the intersection point
 *           on the first line will be stored.
 * @param u2 Reference to a double where the parameter of the intersection point
 *           on the second line will be stored.
 * @param plane The plane onto which the 3D lines are projected (0 for XY, 1 for YZ, 2 for ZX).
 * @param tol The tolerance value for determining intersection.
 * @return True if the lines intersect within the given tolerance, false otherwise.
 */
// bool calcLineIntersection2D(
//   SPoint3 l1p1, SPoint3 l1p2, SPoint3 l2p1, SPoint3 l2p2,
//   double &u1, double &u2, int &plane, const double &tol
//   ) {

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

//   return calcLineIntersection2D(l1p, l1q, l2p, l2q, u1, u2, tol);

// }




/**
 * @brief Calculates the intersection of two 2D line segments.
 *
 * This function determines if two line segments, defined by their endpoints, intersect in 2D space.
 * It returns true if the line segments intersect, and false otherwise.
 *
 * @param ls1v1 Pointer to the first vertex of the first line segment.
 * @param ls1v2 Pointer to the second vertex of the first line segment.
 * @param ls2v1 Pointer to the first vertex of the second line segment.
 * @param ls2v2 Pointer to the second vertex of the second line segment.
 * @param u1 Reference to a double where the intersection parameter for the first line segment will be stored.
 * @param u2 Reference to a double where the intersection parameter for the second line segment will be stored.
 * @param tol The tolerance value used for determining intersection.
 * @return true if the line segments intersect, false otherwise.
 */
// bool calcLineIntersection2D(
//   PDCELVertex *ls1v1, PDCELVertex *ls1v2,
//   PDCELVertex *ls2v1, PDCELVertex *ls2v2,
//   double &u1, double &u2, const double &tol
//   ) {

//   return calcLineIntersection2D(
//     ls1v1->point2(), ls1v2->point2(), ls2v1->point2(), ls2v2->point2(), u1, u2, tol);

// }




// bool calc_line_intersection_2d(
//   PDCELVertex *ls1v1, PDCELVertex *ls1v2,
//   PDCELVertex *ls2v1, PDCELVertex *ls2v2,
//   double &ipx, double &ipy, double &u1, double &u2
//   ) {

//   return calc_line_intersection_2d(
//     ls1v1->point2()[0], ls1v1->point2()[1], ls1v2->point2()[0], ls1v2->point2()[1],
//     ls2v1->point2()[0], ls2v1->point2()[1], ls2v2->point2()[0], ls2v2->point2()[1],
//     ipx, ipy, u1, u2);

// }


/**
 * @brief Calculates the intersection of two 2D line segments and returns the intersection vertex.
 *
 * This function determines if two line segments, defined by their endpoints, intersect in 2D space.
 * It returns true if the line segments intersect, and false otherwise.
 * If the line segments intersect, the intersection vertex is stored in v_intersect.
 *
 * @param ls1v1 Pointer to the first vertex of the first line segment.
 * @param ls1v2 Pointer to the second vertex of the first line segment.
 * @param ls2v1 Pointer to the first vertex of the second line segment.
 * @param ls2v2 Pointer to the second vertex of the second line segment.
 * @param v_intersect Pointer to the intersection vertex.
 * @param u1 Reference to a double where the intersection parameteric coordinate for the first line segment will be stored.
 * @param u2 Reference to a double where the intersection parameteric coordinate for the second line segment will be stored.
 * @param ex11 Reference to an int indicating if considering the extension of the first line segment before the starting point (1) or not (0).
 * @param ex12 Reference to an int indicating if considering the extension of the first line segment after the ending point (1) or not (0).
 * @param ex21 Reference to an int indicating if considering the extension of the second line segment before the starting point (1) or not (0).
 * @param ex22 Reference to an int indicating if considering the extension of the second line segment after the ending point (1) or not (0).
 * @return true if the line segments intersect, false otherwise.
 */
bool calc_line_intersection_2d(
  PDCELVertex *ls1v1, PDCELVertex *ls1v2,
  PDCELVertex *ls2v1, PDCELVertex *ls2v2,
  PDCELVertex *v_intersect, double &u1, double &u2,
  const int &ex11, const int &ex12, const int &ex21, const int &ex22
  ) {
  
  PLOG(debug) << "calc_line_intersection_2d"
    << " line 1: " << ls1v1 << " - " << ls1v2
    << " line 2: " << ls2v1 << " - " << ls2v2;

  double ipx, ipy;
  bool is_intersect = calc_line_intersection_2d(
    ls1v1->point2()[0], ls1v1->point2()[1], ls1v2->point2()[0], ls1v2->point2()[1],
    ls2v1->point2()[0], ls2v1->point2()[1], ls2v2->point2()[0], ls2v2->point2()[1],
    ipx, ipy, u1, u2);

  v_intersect = nullptr;

  if (!is_intersect) {
    return false;
  }

  if (u1 >= 0 && u1 <= 1 && u2 >= 0 && u2 <= 1) {
    // Intersection is inside both line segments
    v_intersect = new PDCELVertex(0, ipx, ipy);
  } else if (ex11 && ex12 && ex21 && ex22) {
    // Consider the two line segments as infinite lines
    v_intersect = new PDCELVertex(0, ipx, ipy);
  } else if (ex11 && u1 < 0) {
    // Extend the first line segment before the starting point
    if (ex21 && u2 < 0) {
      // Extend the second line segment before the starting point
      v_intersect = new PDCELVertex(0, ipx, ipy);
    } else if (u2 >= 0 && u2 <= 1) {
      // Intersection is inside the second line segment
      v_intersect = new PDCELVertex(0, ipx, ipy);
    } else if (ex22 && u2 > 1) {
      // Extend the second line segment after the ending point
      v_intersect = new PDCELVertex(0, ipx, ipy);
    } else if (is_close(u2, 0.0)) {
      // Intersection is very close to the starting point of the second line segment
      v_intersect = ls2v1;
    } else if (is_close(u2, 1.0)) {
      // Intersection is very close to the ending point of the second line segment
      v_intersect = ls2v2;
    } else {
      // Two line segments intersect outside the desired range
      return false;
    }
  } else if (u1 >= 0 && u1 <= 1) {
    // Intersection is inside the first line segment
    if (ex21 && u2 < 0) {
      // Extend the second line segment before the starting point
      v_intersect = new PDCELVertex(0, ipx, ipy);
    } else if (u2 >= 0 && u2 <= 1) {
      // Intersection is inside the second line segment
      v_intersect = new PDCELVertex(0, ipx, ipy);
    } else if (ex22 && u2 > 1) {
      // Extend the second line segment after the ending point
      v_intersect = new PDCELVertex(0, ipx, ipy);
    } else {
      // Two line segments intersect outside the desired range
      return false;
    }
  } else if (ex12 && u1 > 0) {
    // Extend the first line segment after the ending point
    if (ex21 && u2 < 0) {
      // Extend the second line segment before the starting point
      v_intersect = new PDCELVertex(0, ipx, ipy);
    } else if (u2 >= 0 && u2 <= 1) {
      // Intersection is inside the second line segment
      v_intersect = new PDCELVertex(0, ipx, ipy);
    } else if (ex22 && u2 > 1) {
      // Extend the second line segment after the ending point
      v_intersect = new PDCELVertex(0, ipx, ipy);
    } else if (is_close(u2, 0.0)) {
      // Intersection is very close to the starting point of the second line segment
      v_intersect = ls2v1;
    } else if (is_close(u2, 1.0)) {
      // Intersection is very close to the ending point of the second line segment
      v_intersect = ls2v2;
    } else {
      // Two line segments intersect outside the desired range
      return false;
    }
  } else {
    // Two line segments intersect outside the desired range
    return false;
  }

  if (is_close(u1, 0.0)) {
    v_intersect = ls1v1;
  } else if (is_close(u1, 1.0)) {
    v_intersect = ls1v2;
  } else if (is_close(u2, 0.0)) {
    v_intersect = ls2v1;
  } else if (is_close(u2, 1.0)) {
    v_intersect = ls2v2;
  }

  PLOG(debug) << "  intersection point: " << v_intersect;

  return is_intersect;
}






// bool calcLineIntersection2D(
//   PGeoLineSegment *ls1, PGeoLineSegment *ls2,
//   double &u1, double &u2, const double &tol
//   ) {

//   SPoint2 ls1p1, ls1p2, ls2p1, ls2p2;
//   ls1p1 = ls1->v1()->point2();
//   ls1p2 = ls1->v2()->point2();
//   ls2p1 = ls2->v1()->point2();
//   ls2p2 = ls2->v2()->point2();

//   return calcLineIntersection2D(ls1p1, ls1p2, ls2p1, ls2p2, u1, u2, tol);

// }








/**
 * @brief Computes the intersection of two line segments.
 *
 * This function calculates the intersection point of two given line segments
 * and returns a result code indicating the nature of the intersection.
 *
 * @param subject Pointer to the first line segment.
 * @param tool Pointer to the second line segment.
 * @param intersect Pointer to the vertex where the intersection point will be stored.
 * @return An integer result code:
 *         - 1 if the segments intersect within the bounds of the first segment.
 *         - -1 if the intersection point is before the start of the first segment.
 *         - 2 if the intersection point is after the end of the first segment.
 *         - 0 if the segments are parallel and do not intersect.
 */
// int intersect(PGeoLineSegment *subject, PGeoLineSegment *tool,
//               PDCELVertex *intersect) {
//   int result;
//   double us, ut;
//   bool not_parallel;

//   not_parallel = calcLineIntersection2D(subject, tool, us, ut, TOLERANCE);
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









/**
 * @brief Finds the intersection between curves and a half-edge loop.
 *
 * This function iterates through all line segments of the given half-edge loop
 * and finds the intersection points between the line segments and the provided
 * curves. It returns the half-edge where the intersection occurs.
 *
 * @param vertices A vector of pointers to PDCELVertex objects representing the vertices of the curves.
 * @param hel A pointer to the PDCELHalfEdgeLoop object representing the half-edge loop.
 * @param end An integer indicating whether to find the intersection at the beginning (0) or the end (1).
 * @param ls_i A reference to an integer that will store the index of the curve segment where the intersection occurs.
 * @param u1 A reference to a double that will store the parametric location of the intersection on the curve.
 * @param u2 A reference to a double that will store the parametric location of the intersection on the tool segment.
 * @param tol A constant reference to a double representing the tolerance for intersection calculations.
 * @param pmessage A pointer to a Message object used for logging and debugging.
 * @return A pointer to the PDCELHalfEdge where the intersection occurs, or nullptr if no intersection is found.
 */
// PDCELHalfEdge *findCurvesIntersection(
//   std::vector<PDCELVertex *> vertices, PDCELHalfEdgeLoop *hel,
//   int end, int &ls_i, double &u1, double &u2, const double &tol,
//   Message *pmessage
//   ) {
//   pmessage->increaseIndent();

//   PLOG(debug) << pmessage->message("in function: findCurvesIntersection");

//   PDCELHalfEdge *he = nullptr;

//   std::vector<PDCELVertex *> tmp_ls; // temporary line segment
//   int ls_i_prev;
//   PDCELHalfEdge *hei = hel->incidentEdge();
//   std::vector<int> c_is, t_is;  // curve indices, tool indices
//   std::vector<double> c_us, t_us;  // curve parametric locations, tool parametric locations
//   int j0;
//   double tmp_c_u, tmp_t_u;  // temporary parametric locations

//   if (end == 0) {  // find the intersection at the beginning
//     u1 = -INF;
//     ls_i_prev = -1;
//   }
//   else if (end == 1) {  // find the intersection at the end
//     u1 = INF;
//     ls_i_prev = convertSizeTToInt(vertices.size());
//   }

//   // Iterate through all line segments of the half edge loop
//   do {
//     PLOG(debug) << pmessage->message("----------");

//     tmp_ls.clear();
//     tmp_ls.push_back(hei->source());
//     tmp_ls.push_back(hei->target());

//     // std::cout << "tmp_ls: " << tmp_ls[0] << ", " << tmp_ls[1] << std::endl;
//     PLOG(debug) << pmessage->message(
//       "line segment of the half edge loop (tmp_ls): " + tmp_ls[0]->printString() + " -- " + tmp_ls[1]->printString()
//       );

//     c_is.clear();
//     t_is.clear();
//     c_us.clear();
//     t_us.clear();


//     // Find all intersections between the line segment and the curve
//     findAllIntersections(
//       vertices, tmp_ls, c_is, t_is, c_us, t_us
//     );

//     PLOG(debug) << pmessage->message("all intersections");
//     PLOG(debug) << pmessage->message("curve index (i) -- param loc (u) | tool index (i) -- param loc (u)");

//     for (auto k = 0; k < c_is.size(); k++) {
//       PLOG(debug) << pmessage->message(
//         std::to_string(c_is[k]) + " -- " + std::to_string(c_us[k]) + " | "
//         + std::to_string(t_is[k]) + " -- " + std::to_string(t_us[k]));
//     }


//     // If there is at least one intersection
//     if (c_is.size() > 0) {

//       // Find the intersection that is the closest to the expected end
//       if (end == 0) {
//         tmp_c_u = getIntersectionLocation(
//           vertices, c_is, c_us, 1, 0, ls_i, j0, pmessage
//         );
//       }
//       else if (end == 1) {
//         tmp_c_u = getIntersectionLocation(
//           vertices, c_is, c_us, 0, 0, ls_i, j0, pmessage
//         );
//       }
//       tmp_t_u = t_us[j0];

//       PLOG(debug) << pmessage->message("closest intersection to end " + std::to_string(end));
//       PLOG(debug) << pmessage->message("curve segment index (ls_i) = " + std::to_string(ls_i));
//       PLOG(debug) << pmessage->message("prev curve segment index (ls_i_prev) = " + std::to_string(ls_i_prev));
//       PLOG(debug) << pmessage->message("curve param loc (tmp_c_u) = " + std::to_string(tmp_c_u));
//       PLOG(debug) << pmessage->message("tool param loc (tmp_t_u) = " + std::to_string(tmp_t_u));
//       PLOG(debug) << pmessage->message(
//         "curve segment: v11 = " + vertices[ls_i]->printString() + " -> "
//         + "v12 = " + vertices[ls_i+1]->printString()
//       );
//       PLOG(debug) << pmessage->message(
//         "tool segment: v21 = " + tmp_ls[0]->printString() + " -> "
//         + "v22 = " + tmp_ls[1]->printString()
//       );
//       // PLOG(debug) << pmessage->message("tol = " + std::to_string(tol));
//       // PLOG(debug) << pmessage->message("number of vertices of the curve = " + std::to_string(vertices.size()));

//       bool update = false;

//       // If the intersection is within the tool segment
//       PLOG(debug) << pmessage->message("u1 = " + std::to_string(u1));
//       PLOG(debug) << pmessage->message("tol = " + std::to_string(tol));
//       if (fabs(tmp_t_u) <= tol || (tmp_t_u > 0 && tmp_t_u < 1) || fabs(1 - tmp_t_u) <= tol) {

//         // If want the intersection closer to the beginning
//         if (end == 0) {

//           // If the intersection is before the first vertex
//           if (ls_i == 0 && tmp_c_u < 0 && tmp_c_u > u1) {
//             update = true;
//           }
//           else if (fabs(tmp_c_u) <= tol || (tmp_c_u > 0 && tmp_c_u < 1) || fabs(1 - tmp_c_u) <= tol) {
//             if (ls_i > ls_i_prev) {
//               update = true;
//             }
//             else if (ls_i == ls_i_prev && tmp_c_u > u1) {
//               update = true;
//             }
//           }

//           // if (
//           //   (ls_i == 0 && tmp_c_u < 0 && tmp_c_u > u1)  // before the first vertex
//           //   || ((fabs(tmp_c_u) <= tol || (tmp_c_u > 0 && tmp_c_u < 1) || fabs(1 - tmp_c_u) <= tol) && ls_i > ls_i_prev)  // inner line segment
//           //   || ((fabs(tmp_c_u) <= tol || (tmp_c_u > 0 && tmp_c_u < 1) || fabs(1 - tmp_c_u) <= tol) && ls_i == ls_i_prev && tmp_c_u > u1) // same line segment but inner u
//           //   ) {

//           //   u1 = tmp_c_u;
//           //   u2 = tmp_t_u;
//           //   he = hei;
//           //   ls_i_prev = ls_i;

//           // }

//         }

//         // If want the intersection closer to the ending
//         else if (end == 1) {

//           if (ls_i == vertices.size() - 2 && tmp_c_u > 1 && tmp_c_u < u1) {
//             update = true;
//           }
//           else if (fabs(tmp_c_u) <= tol || (tmp_c_u > 0 && tmp_c_u < 1) || fabs(1 - tmp_c_u) <= tol) {
//             if (ls_i < ls_i_prev) {
//               update = true;
//             }
//             else if (ls_i == ls_i_prev && tmp_c_u < u1) {
//               update = true;
//             }
//           }

//           // if (
//           //   ((ls_i == vertices.size() - 2) && tmp_c_u > 1 && tmp_c_u < u1)  // after the last vertex
//           //   || ((fabs(tmp_c_u) <= tol || (tmp_c_u > 0 && tmp_c_u < 1) || fabs(1 - tmp_c_u) <= tol) && ls_i < ls_i_prev)  // inner line segment
//           //   || ((fabs(tmp_c_u) <= tol || (tmp_c_u > 0 && tmp_c_u < 1) || fabs(1 - tmp_c_u) <= tol) && ls_i == ls_i_prev && tmp_c_u < u1) // same line segment but inner u
//           //   ) {

//           //   u1 = tmp_c_u;
//           //   u2 = tmp_t_u;
//           //   he = hei;
//           //   ls_i_prev = ls_i;

//           // }

//         }

//       }

//       if (update) {

//         PLOG(debug) << pmessage->message("update intersection");

//         u1 = tmp_c_u;
//         u2 = tmp_t_u;
//         he = hei;
//         ls_i_prev = ls_i;

//         PLOG(debug) << pmessage->message("u1 = " + std::to_string(u1));
//         PLOG(debug) << pmessage->message("u2 = " + std::to_string(u2));
//         PLOG(debug) << pmessage->message("end = " + std::to_string(end));

//       }


//       ls_i = ls_i_prev;

//     }

//     hei = hei->next();

//   } while (hei != hel->incidentEdge());

//   pmessage->decreaseIndent();

//   return he;
// }




/**
 * @brief Finds the intersection point between two polylines that is closest to a certain parameter location on each polyline.
 * 
 * Find the intersection point between two polylines that is closest to a certain
 * parameter location on each polyline.
 *
 * @param[in] polyline_1 list of vertices of the first polyline
 * @param[in] polyline_2 list of vertices of the second polyline
 * @param[in] param_loc_1 the nondimensional parametric location on the first polyline the intersection point is closest to. The parametric location can be in the range [0, 1] or -1. 0 means the beginning of the polyline, 1 means the end of the polyline, and -1 means anywhere on the polyline.
 * @param[in] param_loc_2 the nondimensional parametric location on the second polyline the intersection point is closest to. The parametric location can be in the range [0, 1] or -1. 0 means the beginning of the polyline, 1 means the end of the polyline, and -1 means anywhere on the polyline.
 * @param[in] ex11 whether consider the intersection before the beginning point of the first polyline
 * @param[in] ex12 whether consider the intersection after the ending point of the first polyline
 * @param[in] ex21 whether consider the intersection before the beginning point of the second polyline
 * @param[in] ex22 whether consider the intersection after the ending point of the second polyline
 * @param[out] i1 the index of the line segment of the first polyline that contains the intersection point
 * @param[out] u1 the nondimensional parametric location of the intersection point on the line segment of the first polyline
 * @param[out] i2 the index of the line segment of the second polyline that contains the intersection point
 * @param[out] u2 the nondimensional parametric location of the intersection point on the line segment of the second polyline
 * @param[out] is_new_1 whether the intersection point is new to the first polyline
 * @param[out] is_new_2 whether the intersection point is new to the second polyline
 * @param[in] pmessage a message object for logging
 *
 * @return the intersection point, or nullptr if no intersection point is found
 */
PDCELVertex *get_polylines_intersection_close_to(
  std::vector<PDCELVertex *> &polyline_1, std::vector<PDCELVertex *> &polyline_2,
  const double &param_loc_1, const double &param_loc_2,
  const int &ex11, const int &ex12, const int &ex21, const int &ex22,
  int &i1, double &u1, int &i2, double &u2, bool &is_new_1, bool &is_new_2,
  Message *pmessage
) {
  pmessage->increaseIndent();

  PLOG(debug) << pmessage->message("in function: get_polylines_intersection_close_to");

  PDCELVertex *v_intersect = nullptr;

  // Find all intersections between the curves and the half-edge loop
  std::vector<int> is1, is2;  // curve indices, tool indices
  std::vector<double> us1, us2;  // curve parametric locations, tool parametric locations
  find_open_polylines_intersections(
    polyline_1, polyline_2, is1, is2, us1, us2,
    ex11, ex12, ex21, ex22,
    pmessage
  );

  if (is1.size() == 1) {
    // Only one intersection
    i1 = is1[0];
    i2 = is2[0];
    u1 = us1[0];
    u2 = us2[0];
  } else {
    // Find the intersection closest to the specified parametric locations of the curve
    int which_end;
    bool inner_only;
    int j;
    if (param_loc_1 != -1) {
      if (param_loc_1 == 0) {
        which_end = 0;
        if (ex11) {
          inner_only = false;
        } else {
          inner_only = true;
        }
      } else if (param_loc_1 == 1) {
        which_end = 1;
        if (ex12) {
          inner_only = false;
        } else {
          inner_only = true;
        }
      }
      j = get_intersection_closer_to(
        polyline_1, is1, us1, which_end, inner_only, pmessage
      );
    } else if (param_loc_2 != -1) {
      if (param_loc_2 == 0) {
        which_end = 0;
        if (ex21) {
          inner_only = false;
        } else {
          inner_only = true;
        }
      } else if (param_loc_2 == 1) {
        which_end = 1;
        if (ex22) {
          inner_only = false;
        } else {
          inner_only = true;
        }
      }
      j = get_intersection_closer_to(
        polyline_2, is2, us2, which_end, inner_only, pmessage
      );
    }

    // Get output and return values
    i1 = is1[j];
    u1 = us1[j];
    i2 = is2[j];
    u2 = us2[j];
  }

  // Get the intersection point
  PDCELVertex *v1, v2;
  is_new_1 = get_vertex_by_param_coord_of_two_vertices(
    polyline_1[i1], polyline_1[i1 + 1], u1, v1
  );
  is_new_2 = get_vertex_by_param_coord_of_two_vertices(
    polyline_2[i2], polyline_2[i2 + 1], u2, v2
  );

  if (is_new_1 && is_new_2) {
    v_intersect = v1;
  } else if (!is_new_1) {
    v_intersect = v1;
  } else if (!is_new_2) {
    v_intersect = v2;
  }

  pmessage->decreaseIndent();

  return v_intersect;
}




PDCELHalfEdge *find_curves_intersection(
  std::vector<PDCELVertex *> vertices, PDCELHalfEdgeLoop *hel,
  const int &end, const int &ex, int &ls_i, double &u1, double &u2,
  Message *pmessage
  ) {
  pmessage->increaseIndent();

  PLOG(debug) << pmessage->message("in function: find_curves_intersection");

  PDCELHalfEdge *he = nullptr;


  // Convert the half-edge loop to a list of vertices
  std::vector<PDCELVertex *> vertices_hel;
  PDCELHalfEdge *hei = hel->incidentEdge();
  vertices_hel.push_back(hei->source());
  do {
    vertices_hel.push_back(hei->target());
    hei = hei->next();
  } while (hei != hel->incidentEdge());

  double param_loc;
  if (end == 0) {
    param_loc = 0.0;
  } else if (end == 1) {
    param_loc = 1.0;
  }
  int i1, i2;
  double u1, u2;
  bool is_new_1, is_new_2;
  PDCELVertex v_intersect = get_polylines_intersection_close_to(
    vertices, vertices_hel, param_loc, ex, ex, 0, 0,
    i1, u1, i2, u2, is_new_1, is_new_2, pmessage
  );


  // Get the j-th half-edge from the half-edge loop
  hei = hel->incidentEdge();
  for (int k = 0; k < i2 - 1; k++) {
    hei = hei->next();
  }
  he = hei;

  pmessage->decreaseIndent();

  return he;
}










/**
 * @brief Finds the intersection of curves and adjusts the baseline accordingly.
 *
 * This function takes a baseline and a line segment, and finds the intersection
 * between them. It adjusts the baseline by adding new vertices at the intersection
 * points and returns the new baseline. The function also updates the indices of the
 * old and new vertices and the link lists.
 *
 * @param bl Pointer to the original baseline.
 * @param ls Pointer to the line segment to intersect with.
 * @param end Indicates the end of the baseline to adjust (0 for start, 1 for end).
 * @param u1 Reference to a double to store the parametric coordinate of the intersection on the baseline.
 * @param u2 Reference to a double to store the parametric coordinate of the intersection on the line segment.
 * @param iold Reference to an integer to store the index of the first kept vertex in the old baseline.
 * @param inew Reference to an integer to store the index of the first kept vertex in the new baseline.
 * @param link_to_list Reference to a vector of integers representing the link list of the original baseline.
 * @param base_offset_indices_links Reference to a vector of integers representing the base offset indices links.
 * @return Pointer to the new baseline with the adjusted vertices. Returns nullptr if the intersection is not found.
 */
// Baseline *findCurvesIntersection(
//   Baseline *bl, PGeoLineSegment *ls, int end,
//   double &u1, double &u2, int &iold, int &inew,
//   std::vector<int> &link_to_list, std::vector<int> &base_offset_indices_links
//   ) {
//   // After adjusting the curve
//   // the first kept vertex will be at index iold in the old curve
//   // and at the index inew in the new curve

//   // std::cout << "\n[debug] findCurvesIntersection" << std::endl;

//   // std::cout << "        curve bl:" << std::endl;
//   // for (auto v : bl->vertices()) {
//   //   std::cout << "        " << v << std::endl;
//   // }

//   iold = 0;
//   inew = 0;

//   Baseline *bl_new = new Baseline();
//   std::size_t n = bl->vertices().size();

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

//     not_parallel = calcLineIntersection2D(lsi, ls, u1, u2, TOLERANCE);

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









/**
 * @brief Finds all intersections between line segments defined by two sets of vertices.
 *
 * This function takes two sets of vertices representing line segments and finds all intersections
 * between these segments. The indices of the intersecting segments and the non-dimensional
 * locations of the intersections are stored in the provided vectors.
 *
 * @param c1 A vector of pointers to PDCELVertex objects representing the first set of vertices.
 * @param c2 A vector of pointers to PDCELVertex objects representing the second set of vertices.
 * @param i1s A vector to store the indices of the intersecting segments from the first set.
 * @param i2s A vector to store the indices of the intersecting segments from the second set.
 * @param u1s A vector to store the non-dimensional locations of the intersections on the segments from the first set.
 * @param u2s A vector to store the non-dimensional locations of the intersections on the segments from the second set.
 * @return An integer indicating the success of the function (always returns 0).
 */
// int findAllIntersections(
//   const std::vector<PDCELVertex *> &c1, const std::vector<PDCELVertex *> &c2,
//   std::vector<int> &i1s, std::vector<int> &i2s,
//   std::vector<double> &u1s, std::vector<double> &u2s
// ) {

//   bool found = false;

//   if (c1.empty() || c2.empty()) {
//     return 0;
//   }

//   // Find all intersections

//   PDCELVertex *v11, *v12, *v21, *v22;

//   for (int i = 0; i < c1.size() - 1; ++i) {

//     v11 = c1[i];
//     v12 = c1[i + 1];

//     for (int j = 0; j < c2.size() - 1; ++j) {

//       v21 = c2[j];
//       v22 = c2[j + 1];

//       // Check intersection
//       bool not_parallel;
//       double u1, u2;

//       if (v11 == nullptr || v12 == nullptr || v21 == nullptr || v22 == nullptr) {
//         throw std::runtime_error("null pointer reference in findAllIntersections");
//       }

//       // std::cout << "        find intersection:" << std::endl;
//       // std::cout << "        v11 = " << v11 << ", v12 = " << v12 <<
//       // std::endl; std::cout << "        v21 = " << v21 << ", v22 = " << v22
//       // << std::endl;

//       not_parallel = calcLineIntersection2D(v11, v12, v21, v22, u1, u2, TOLERANCE);

//       // std::cout << "line_" << line_i << "_seg_" << i+1 << " and ";
//       // std::cout << "line_" << line_i+1 << "_seg_" << j+1 << ": ";
//       // std::cout << "u1=" << u1 << " and u2=" << u2 << std::endl;
//       // std::cout << "        not_parallel = " << not_parallel << ", u1 = "
//       // << u1 << ", u2 = " << u2 << std::endl;

//       if (not_parallel) {
//         if (
//           // 9 cases
//           (u1 >= 0 && u1 <= 1 && u2 >= 0 && u2 <= 1)  // inner x inner
//           || (i == 0 && u1 < 0 && j == 0 && u2 < 0) // before head x before head
//           || (i == 0 && u1 < 0 && u2 >= 0 && u2 <= 1)  // before head x inner
//           || (i == 0 && u1 < 0 && j == c2.size() - 2 && u2 > 1) // before head x after tail
//           || (i == c1.size() - 2 && u1 > 1 && j == 0 && u2 < 0) // after tail x before head
//           || (i == c1.size() - 2 && u1 > 1 && u2 >= 0 && u2 <= 1)  // after tail x inner
//           || (i == c1.size() - 2 && u1 > 1 && j == c2.size() - 2 && u2 > 1) // after tail x after tail
//           || (u1 >= 0 && u1 <= 1 && j == 0 && u2 < 0)  // inner x before head
//           || (u1 >= 0 && u1 <= 1 && j == c2.size() - 2 && u2 > 1)  // inner x after tail
//           ) {

//           i1s.push_back(i);
//           i2s.push_back(j);
//           u1s.push_back(u1);
//           u2s.push_back(u2);

//           // found = true;
//           // std::cout << "        found intersection:" << std::endl;
//           // std::cout << "        v11 = " << v11 << ", v12 = " << v12 << std::endl;
//           // std::cout << "        v21 = " << v21 << ", v22 = " << v22 << std::endl;
//           // std::cout << "        not_parallel = " << not_parallel
//           //           << ", u1 = " << u1 << ", u2 = " << u2 << std::endl;

//         }

//       }

//     }

//   }

//   // for (auto i = 0; i < i1s.size(); i++) {
//   //   std::cout << "intersection " << i << std::endl;
//   //   std::cout << "  i1 = " << i1s[i] << ", v11 = " << c1[i1s[i]] << ", v12 = " << c1[i1s[i]+1] << ", u1 = " << u1s[i] << std::endl;
//   //   std::cout << "  i2 = " << i2s[i] << ", v21 = " << c2[i2s[i]] << ", v22 = " << c2[i2s[i]+1] << ", u2 = " << u2s[i] << std::endl;
//   // }

//   return 0;

// }




int find_open_polylines_intersections(
  const std::vector<PDCELVertex *> &c1,
  const std::vector<PDCELVertex *> &c2,
  std::vector<int> &i1s, std::vector<int> &i2s,
  std::vector<double> &u1s, std::vector<double> &u2s,
  const int &ex11, const int &ex12, const int &ex21, const int &ex22,
  Message *pmessage
) {
  pmessage->increaseIndent();

  PLOG(debug) << pmessage->message("find_open_polylines_intersections");

  // std::stringstream ss;
  bool found = false;

  if (c1.empty() || c2.empty()) {
    return 0;
  }

  // Find all intersections

  PDCELVertex *v11, *v12, *v21, *v22;
  auto nls1 = c1.size() - 1;  // number of line segments
  auto nls2 = c2.size() - 1;

  int _ex11, _ex12, _ex21, _ex22;  // flags to consider extensions of segments

  for (int i = 0; i < nls1; ++i) {
    // Iterate through all line segments of the first polyline

    v11 = c1[i];
    v12 = c1[i + 1];

    if (i == 0) {
      // For the first line segment
      _ex11 = ex11;  // use the same flag for the starting point
      _ex12 = 0;  // no extension after the ending point
    } else if (i == nls1 - 1) {
      // For the last line segment
      _ex11 = 0;  // no extension before the starting point
      _ex12 = ex12;  // use the same flag for the ending point
    } else {
      // For all other line segments, use no extension
      _ex11 = 0;
      _ex12 = 0;
    }

    for (int j = 0; j < nls2; ++j) {
      // Iterate through all line segments of the second polyline

      v21 = c2[j];
      v22 = c2[j + 1];

      if (j == 0) {
        // For the first line segment
        _ex21 = ex21;  // use the same flag for the starting point
        _ex22 = 0;  // no extension after the ending point
      } else if (j == nls2 - 1) {
        // For the last line segment
        _ex21 = 0;  // no extension before the starting point
        _ex22 = ex22;  // use the same flag for the ending point
      } else {
        // For all other line segments, use no extension
        _ex21 = 0;
        _ex22 = 0;
      }

      // Check intersection
      // bool not_parallel;
      
      if (v11 == nullptr || v12 == nullptr || v21 == nullptr || v22 == nullptr) {
        throw std::runtime_error("null pointer reference in findAllIntersections");
      }
      
      // ss.str("");
      // ss << "  find intersection:\n";
      // ss << "  polyline 1 segment " << i + 1 << " v11 = " << v11 << ", v12 = " << v12 << "\n";
      // ss << "  polyline 2 segment " << j + 1 << " v21 = " << v21 << ", v22 = " << v22 << std::endl;
      // PLOG(debug) << pmessage->message(ss.str());
      
      double u1, u2;
      PDCELVertex *ip;
      bool not_parallel = calc_line_intersection_2d(
        v11, v12, v21, v22, ip, u1, u2, _ex11, _ex12, _ex21, _ex22);

      if (not_parallel) {
        PLOG(debug) << "  u1 = " << u1 << ", u2 = " << u2 << std::endl;

        i1s.push_back(i);
        i2s.push_back(j);
        u1s.push_back(u1);
        u2s.push_back(u2);

      } else {
        PLOG(debug) << "  parallel" << std::endl;
      }

    }

  }

  pmessage->decreaseIndent();

  return 0;
}









/**
 * @brief Finds the intersection location that is the closest to the expected end.
 *
 * @param c A vector of pointers to PDCELVertex objects.
 * @param ii A vector of integers representing indices.
 * @param uu A vector of doubles representing intersection parameters.
 * @param which_end An integer indicating which end to consider (0 for beginning, 1 for ending).
 * @param inner_only An integer flag indicating whether to consider only inner intersections (1 for true, 0 for false).
 * @param ls_i An integer reference to store the index of the closest intersection.
 * @param j An integer reference to store the index of the closest intersection in the uu vector.
 * @param pmessage A pointer to a Message object for logging purposes.
 * @return The intersection parameter (u) of the closest intersection.
 */
// double getIntersectionLocation(
//   std::vector<PDCELVertex *> &c,
//   const std::vector<int> &ii, std::vector<double> &uu,
//   const int &which_end, const int &inner_only,
//   int &ls_i, int &j, Message *pmessage
// ) {
//   // Find the intersection location that is the closest to the expected end
//   pmessage->increaseIndent();

//   // PLOG(debug) << pmessage->message("in function: getIntersectionLocation");

//   ls_i = ii[0];
//   double u = uu[0];
//   j = 0;

//   for (auto k = 1; k < ii.size(); k++) {

//     if ((inner_only && uu[k] >= 0 && uu[k] <= 1) || !inner_only) {
//       if (which_end == 0) {
//         // Closer to the beginning side
//         if (ii[k] < ls_i) {
//           // If the current segment index is smaller
//           ls_i = ii[k];
//           u = uu[k];
//           j = k;
//         }
//         else if (ii[k] == ls_i) {
//           if (uu[k] < u) {
//             u = uu[k];
//             j = k;
//           }

//         }

//       }
//       else if (which_end == 1) {
//         // Closer to the ending side
//         if (ii[k] > ls_i) {
//           ls_i = ii[k];
//           u = uu[k];
//           j = k;
//         }
//         else if (ii[k] == ls_i) {
//           if (uu[k] > u) {
//             u = uu[k];
//             j = k;
//           }

//         }

//       }
//     }

//   }


//   // PLOG(debug) << pmessage->message(
//   //   "ls_i = " + std::to_string(ls_i)
//   //   + ", u = " + std::to_string(u)
//   //   + ", v1 = " + c[ls_i]->printString()
//   //   + ", v2 = " + c[ls_i+1]->printString()
//   // );

//   pmessage->decreaseIndent();

//   return u;
// }




/// @brief Get the intersection location that is the closest to a given parametric coordinate.
/// @param c The curve.
/// @param ii The indices of the line segments of the intersections.
/// @param uu The non-dimensional locations of the intersections on the line segments.
/// @param param_coord The parametric coordinate. The beginning of the curve is 0, the end is 1.
/// @param ex1 The extension flag for the starting point of the line segment.
/// @param ex2 The extension flag for the ending point of the line segment.
/// @param side The side from which to consider the distance to the parametric coordinate (-1: from the beginning, 1: from the ending, 0: either side).
/// @param pmessage The message.
/// @return The index of the intersection.
int get_intersection_close_to_param_coord(
  const std::vector<PDCELVertex *> &c,
  const std::vector<int> &ii, const std::vector<double> &uu,
  const double &param_coord, const int &ex1, const int &ex2, const int &side,
  Message *pmessage
) {

  pmessage->increaseIndent();

  PLOG(debug) << pmessage->message("in function: get_intersection_close_to_param_coord");

  int j = 0;

  auto nls = c.size() - 1;
  double length = calcPolylineLength(c);

  double d = INF;

  for (auto k = 1; k < ii.size(); k++) {

    int _i = ii[k];
    double _u = uu[k];

    // Calculate the parametric coordinate
    double _l;
    if (_i == 0 && _u < 0) {
      // The intersection is before the beginning of the curve
      _l = _u * std::sqrt(calcDistanceSquared(c[_i], c[_i+1]));
    } else if (_i == nls - 1 && _u > 1) {
      // The intersection is after the end of the curve
      _l = length + _u * std::sqrt(calcDistanceSquared(c[_i], c[_i+1]));
    } else {
      _l = calc_curve_length_of_segment_param_coord_from_start(c, _i, _u);
    }

    double _l_nd = _l / length;

    double _d = fabs(_l_nd - param_coord);
    if (_d < d) {
      if (_l_nd < param_coord && (side == -1 || side == 0)) {
        // The intersection is before the parametric coordinate
        d = _d;
        j = k;
      } else if (_l_nd > param_coord && (side == 1 || side == 0)) {
        // The intersection is after the parametric coordinate
        d = _d;
        j = k;
      }
    }

  }

  pmessage->decreaseIndent();

  return j;

}




int get_intersection_closer_to(
  const std::vector<PDCELVertex *> &c,
  const std::vector<int> &ii, const std::vector<double> &uu,
  const int &which_end, const bool &inner_only,
  Message *pmessage
) {
  // Find the intersection location that is the closest to the expected end
  pmessage->increaseIndent();

  PLOG(debug) << pmessage->message("in function: get_intersection_closer_to");

  int j = 0;

  int ls_i = ii[0];
  double u = uu[0];

  for (auto k = 1; k < ii.size(); k++) {

    if ((inner_only && uu[k] >= 0 && uu[k] <= 1) || !inner_only) {
      if (which_end == 0) {
        // Closer to the beginning side
        if (ii[k] < ls_i) {
          // If the current segment index is smaller
          ls_i = ii[k];
          u = uu[k];
          j = k;
        }
        else if (ii[k] == ls_i) {
          // If the current segment index is the same
          if (uu[k] < u) {
            // If the current parametric coordinate is smaller
            u = uu[k];
            j = k;
          }

        }

      }
      else if (which_end == 1) {
        // Closer to the ending side
        if (ii[k] > ls_i) {
          // If the current segment index is larger
          ls_i = ii[k];
          u = uu[k];
          j = k;
        }
        else if (ii[k] == ls_i) {
          // If the current segment index is the same
          if (uu[k] > u) {
            // If the current parametric coordinate is larger
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

  return j;
}







/**
 * @brief Get the intersection vertex from two curves.
 *
 * This function calculates the intersection vertex between two curves and 
 * inserts it into the appropriate position in the curves.
 *
 * @param c1 The first curve represented as a vector of PDCELVertex pointers.
 * @param c2 The second curve represented as a vector of PDCELVertex pointers.
 * @param i1 Index of the line segment in the first curve where the intersection occurs.
 * @param i2 Index of the line segment in the second curve where the intersection occurs.
 * @param u1 Non-dimensional location of the intersection on the line segment of the first curve.
 * @param u2 Non-dimensional location of the intersection on the line segment of the second curve.
 * @param which_end_1 Choose the intersection closer to the beginning (0) or ending (1) of the first curve.
 * @param which_end_2 Choose the intersection closer to the beginning (0) or ending (1) of the second curve.
 * @param inner_only_1 Whether to consider intersections between two ends (1) or not (0) for the first curve.
 * @param inner_only_2 Whether to consider intersections between two ends (1) or not (0) for the second curve.
 * @param is_new_1 Output parameter indicating if a new vertex was created for the first curve.
 * @param is_new_2 Output parameter indicating if a new vertex was created for the second curve.
 * @param tol Tolerance value for determining if the intersection is at the beginning or end of a segment.
 * @return PDCELVertex* Pointer to the intersection vertex.
 */
// PDCELVertex *getIntersectionVertex(
//   std::vector<PDCELVertex *> &c1, std::vector<PDCELVertex *> &c2,
//   int &i1, int &i2, const double &u1, const double &u2,
//   const int &which_end_1, const int &which_end_2,
//   const int &inner_only_1, const int &inner_only_2,
//   int &is_new_1, int &is_new_2,
//   const double &tol
// ) {
//   // Get wanted intersection vertex from all options

//   // i1s, i2s: indices of line segments having intersection
//   // u1s, u2s: non-dimensional location of the intersection on the line segment
//   // which_end_1, which_end_2: choose the intersection closer to beginning (0) or ending (1)
//   // inner_only_1, inner_only_2: whether consider intersections between two ends (1) or not (0)

//   // std::cout << "\n[debug] function: getIntersectionVertex\n";

//   PDCELVertex *ip = nullptr; // The intersection vertex
//   // PDCELVertex *ip2 = nullptr;

//   // Create/Get the intersection vertex
//   PDCELVertex *v11 = c1[i1];
//   PDCELVertex *v12 = c1[i1 + 1];
//   PDCELVertex *v21 = c2[i2];
//   PDCELVertex *v22 = c2[i2 + 1];

//   // std::cout << "v11 = " << v11 << ", v12 = " << v12 << std::endl;
//   // std::cout << "v21 = " << v21 << ", v22 = " << v22 << std::endl;

//   bool insert1, insert2;
//   // int ii1, ii2;

//   // The intersecting point is an existing point of either curve
//   if (fabs(u1) <= tol || fabs(1 - u1) <= tol ||
//       fabs(u2) <= tol || fabs(1 - u2) <= tol) {

//     // For curve c1
//     if (fabs(u1) <= tol) {
//       // Intersecting point is the beginning point (v11) of the line
//       // segment i (v11-v12) of c1
//       insert1 = false;
//       is_new_1 = 0;
//       // i1 = i1;
//       ip = v11;
//     }
//     else if (fabs(1 - u1) <= tol) {
//       // Intersecting point is the ending point (v12) of the line
//       // segment i (v11-v12) of c1
//       insert1 = false;
//       is_new_1 = 0;
//       i1 += 1;
//       ip = v12;
//     }
//     else {
//       // Intersecting point is in the middle of the line segment i
//       // of c1, insertion is needed
//       insert1 = true;
//       is_new_1 = 1;
//       i1 += 1;
//     }

//     if (fabs(u2) <= tol) {
//       // Intersecting point is the beginning point (v21) of the line
//       // segment j (v21-v22) of c2
//       insert2 = false;
//       is_new_2 = 0;
//       // ii2 = i2;
//       ip = v21;
//     }
//     else if (fabs(1 - u2) <= tol) {
//       // Intersecting point is the beginning point (v22) of the line
//       // segment j (v21-v22) of c2
//       insert2 = false;
//       is_new_2 = 0;
//       i2 += 1;
//       ip = v22;
//     }
//     else {
//       // Intersecting point is in the middle of the line segment i
//       // of c2, insertion is needed
//       insert2 = true;
//       is_new_2 = 1;
//       i2 += 1;
//     }
//   }

//   else {
//     // The last case, the intersection is at the middle of two sub-lines
//     // Calculate the new point
//     insert1 = true;
//     insert2 = true;
//     is_new_1 = 1;
//     is_new_2 = 1;
//     i1 += 1;
//     i2 += 1;

//     ip = new PDCELVertex(
//         getParametricPoint(v11->point(), v12->point(), u1));
//     // ip2 = new PDCELVertex(
//     //     getParametricPoint(v21->point(), v22->point(), u2));

//     // std::cout << "        new vertex v0 = " << v0 << std::endl;
//   }

//   // std::cout << "intersecting point ip1: " << ip << std::endl;
//   // std::cout << "intersecting point ip2: " << ip2 << std::endl;

//   // Insert the intersection vertex (if needed)
//   if (insert1) {
//     c1.insert(c1.begin()+i1, ip);
//   }
//   else {
//     c1[i1] = ip;
//   }

//   if (insert2) {
//     c2.insert(c2.begin()+i2, ip);
//   } else {
//     c2[i2] = ip;
//   }

//   // std::cout << "updated curve 1\n";
//   // for (auto v : c1) {
//   //   std::cout << v;
//   //   if (v == ip) {
//   //     std::cout << " intersect i = " << i1;
//   //   }
//   //   std::cout << std::endl;
//   // }
//   // std::cout << "updated curve 2\n";
//   // for (auto v : c2) {
//   //   std::cout << v;
//   //   if (v == ip) {
//   //     std::cout << " intersect i = " << i2;
//   //   }
//   //   std::cout << std::endl;
//   // }

//   return ip;

// }









PDCELVertex *get_intersection_vertex(
  std::vector<PDCELVertex *> &c1, std::vector<PDCELVertex *> &c2,
  int &i1, int &i2, const double &u1, const double &u2,
  Message *pmessage
) {
  // Get wanted intersection vertex from all options

  // i1s, i2s: indices of line segments having intersection
  // u1s, u2s: non-dimensional location of the intersection on the line segment

  pmessage->increaseIndent();

  PLOG(debug) << pmessage->message("in function: get_intersection_vertex");

  std::stringstream ss;

  PDCELVertex *ip = nullptr; // The intersection vertex

  // Check if the vectors are not empty
  if (c1.empty() || c2.empty()) {
    pmessage->decreaseIndent();
    return ip;
  }

  // Create/Get the intersection vertex
  PDCELVertex *v11 = c1[i1];
  PDCELVertex *v12 = c1[i1 + 1];
  PDCELVertex *v21 = c2[i2];
  PDCELVertex *v22 = c2[i2 + 1];

  PLOG(debug) << "  segment 1: " << v11 << " - " << v12 << "\n";
  PLOG(debug) << "  segment 2: " << v21 << " - " << v22 << "\n";

  if (!v11 || !v12 || !v21 || !v22) {
    PLOG(error) << pmessage->message("Null pointer reference");
    throw std::runtime_error("Null pointer reference");
  }

  bool is_new_1, is_new_2;

  // The intersecting point is an existing point of either curve

  // For curve c1
  if (is_close(u1, 0.0)) {
    // Intersecting point is the beginning point (v11) of the line
    // segment i (v11-v12) of c1
    is_new_1 = false;
    ip = v11;
    PLOG(debug) << pmessage->message("  intersection is close to the beginning point of segment 1");
  }
  else if (is_close(u1, 1.0)) {
    // Intersecting point is the ending point (v12) of the line
    // segment i (v11-v12) of c1
    is_new_1 = false;
    i1 += 1;
    ip = v12;
    PLOG(debug) << pmessage->message("  intersection is close to the ending point of segment 1");
  }
  else {
    // Intersecting point is in the middle of the line segment i
    // of c1, insertion is needed
    is_new_1 = true;
    i1 += 1;
    PLOG(debug) << pmessage->message("  intersection is in the middle of segment 1");
  }
  
  // For curve c2
  if (is_close(u2, 0.0)) {
    // Intersecting point is the beginning point (v21) of the line
    // segment j (v21-v22) of c2
    is_new_2 = false;
    ip = v21;
    PLOG(debug) << pmessage->message("  intersection is close to the beginning point of segment 2");
  }
  else if (is_close(u2, 1.0)) {
    // Intersecting point is the beginning point (v22) of the line
    // segment j (v21-v22) of c2
    is_new_2 = false;
    i2 += 1;
    ip = v22;
    PLOG(debug) << pmessage->message("  intersection is close to the ending point of segment 2");
  }
  else {
    // Intersecting point is in the middle of the line segment i
    // of c2, insertion is needed
    is_new_2 = true;
    i2 += 1;
    PLOG(debug) << pmessage->message("  intersection is in the middle of segment 2");
  }

  PLOG(debug) << "  is_new_1 = " << is_new_1 << ", i1 = " << i1 << "\n";
  PLOG(debug) << "  is_new_2 = " << is_new_2 << ", i2 = " << i2 << "\n";

  if (is_new_1 && is_new_2) {
    // The last case, the intersection is at the middle of two sub-lines
    // Calculate the new point

    if (!ip) {
      ip = new PDCELVertex(
          getParametricPoint(v11->point(), v12->point(), u1));
    }

    PLOG(debug) << "  new vertex ip = " << ip << "\n";
  }

  // Insert the intersection vertex (if needed)
  if (is_new_1) {
    c1.insert(c1.begin()+i1, ip);
  }
  else {
    c1[i1] = ip;
  }

  if (is_new_2) {
    c2.insert(c2.begin()+i2, ip);
  } else {
    c2[i2] = ip;
  }

  pmessage->decreaseIndent();

  return ip;

}




