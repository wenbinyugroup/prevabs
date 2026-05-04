#pragma once

#include <cmath>

/** Advance a segment index to the corresponding vertex index after an
 * intersection parameter has been resolved on that segment.
 */
inline int advanceIntersectionVertexIndex(int index, double u, double tol)
{
  if (std::fabs(u) < tol) {
    return index + 1;
  }
  if (std::fabs(1.0 - u) < tol) {
    return index + 2;
  }
  return index + 1;
}
