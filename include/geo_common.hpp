#pragma once

#include <cmath>

enum class PolylineAxis {
  X2,
  X3
};

enum class CurveEnd {
  Begin,
  End
};

enum class AnglePlane {
  YZ,
  ZX,
  XY
};

template <typename P>
P calcPointFromParam(const P &p1, const P &p2, const double &u, bool &is_new,
                     const double &tol) {
  if (std::fabs(u) < tol) {
    is_new = false;
    return P(p1);
  } else if (std::fabs(u - 1) < tol) {
    is_new = false;
    return P(p2);
  } else {
    is_new = true;
    return P(p1 + u * (p2 - p1));
  }
}
