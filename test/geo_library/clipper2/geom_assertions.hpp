#pragma once

// Geometric inspection helpers used by test_clipper2_airfoil.cpp.
// Pure functions over Clipper2 PathD / PathsD. No PreVABS coupling.
//
// These are written to read like assertions, not to be optimised. The
// intent is to give actionable failure messages when Clipper2 output
// drifts from expectations.

#include "clipper2/clipper.h"

namespace clipper2_airfoil {

// Signed area of a polygon (positive when CCW). 0 for empty/degenerate.
double signedArea(const Clipper2Lib::PathD& path);

// Sum of |signed area| across all paths in a multi-polygon solution.
double totalAbsArea(const Clipper2Lib::PathsD& paths);

// Largest |signed area| across paths.
double largestAbsArea(const Clipper2Lib::PathsD& paths);

// Returns the number of segments in `path` that intersect any other
// non-adjacent segment of the same path. Brute-force O(N^2); only
// suitable for the airfoil scale (a few hundred vertices).
int countSelfIntersections(const Clipper2Lib::PathD& path);

// True if the polyline is closed-by-convention: front and back vertex
// coincide. Tolerance is absolute in user units.
bool isClosedByCoincidence(const Clipper2Lib::PathD& path, double tol = 1e-9);

// Bounding box convenience.
struct Bbox {
  double xmin, ymin, xmax, ymax;
  double width()  const { return xmax - xmin; }
  double height() const { return ymax - ymin; }
};
Bbox bboxOf(const Clipper2Lib::PathD& path);
Bbox bboxOf(const Clipper2Lib::PathsD& paths);

// Half-thickness probe along the airfoil chord. For a point at
// (x, y_chord), returns the minimum distance from that point to any
// segment of `contour`. Used to detect "local thickness < 2*delta"
// pre-emptively for offset sanity tests.
double minDistanceToContour(double x, double y,
                            const Clipper2Lib::PathD& contour);

}  // namespace clipper2_airfoil
