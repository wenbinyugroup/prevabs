#include "globalConstants.hpp"

#include <cmath>
#include <stdexcept>

double GEO_TOL = 1e-9;
double GEO_MERGE_TOL = GEO_TOL * 100.0;
double& TOLERANCE = GEO_TOL;
double& ABS_TOL = GEO_TOL;
double& REL_TOL = GEO_TOL;

void setGeometryTolerance(double tolerance) {
  if (!(tolerance > 0.0) || !std::isfinite(tolerance)) {
    throw std::invalid_argument(
        "geometry tolerance must be positive and finite");
  }
  GEO_TOL = tolerance;
  GEO_MERGE_TOL = tolerance * 100.0;
}
