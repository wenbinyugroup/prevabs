#include "globalConstants.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

// C: dimensionless relative ladder (fundamental, user-configurable layer).
// Ascending rungs; chosen so the B floors below govern at test-suite scales.
double REL_PREDICATE = 1e-3;
double REL_MERGE_FINE = 2e-3;
double REL_EXPORT_MERGE = 4e-3;
double REL_RESAMPLE = 2e-2;
double REL_MERGE_COARSE = 1e-1;

// C (parameter-space): direct thresholds on a normalized u/t parameter.
double PARAM_ENDPOINT_SLACK = 1e-12;
double PARAM_CORNER_EPS = 1e-9;

// D: machine-precision guard (not a geometric tolerance; never scaled).
double GEO_EPS_MACHINE = 1e-14;

// B floors: absolute FP / Clipper2-grid noise protection. The precision-8 grid
// (~1e-8) and independent-interpolation gaps (~1e-6) are absolute artifacts that
// do NOT scale with L_char, so the B ladder is floored by these values.
static const double kCoincidenceFloor = 1e-6;
static const double kExportMergeFloor = 2e-6;
static const double kResampleFloor    = 1e-5;

// A: length tolerances derived from the C ladder. setGeometryTolerance receives
// GEO_TOL = L_char * REL_PREDICATE and derives GEO_MERGE_TOL from the rel ratio.
double GEO_TOL = 1e-9;
double GEO_MERGE_TOL = GEO_TOL * (REL_MERGE_COARSE / REL_PREDICATE);
double& TOLERANCE = GEO_TOL;
double& ABS_TOL = GEO_TOL;
double& REL_TOL = GEO_TOL;

// B: max(absolute floor, L_char * rel) — see setGeometryTolerance. Initialised
// to the floors; the floor governs at test-suite scales, the relative term
// scales the ladder up for large-feature models.
double GEO_COINCIDENCE_TOL = kCoincidenceFloor;
double GEO_EXPORT_MERGE_TOL = kExportMergeFloor;
double GEO_RESAMPLE_MIN_SEP = kResampleFloor;
double GEO_NESTING_SLACK = kCoincidenceFloor;

void setGeometryTolerance(double tolerance) {
  if (!(tolerance > 0.0) || !std::isfinite(tolerance)) {
    throw std::invalid_argument(
        "geometry tolerance must be positive and finite");
  }
  // `tolerance` is L_char * REL_PREDICATE, so L_char = tolerance / REL_PREDICATE.
  GEO_TOL = tolerance;
  GEO_MERGE_TOL = tolerance * (REL_MERGE_COARSE / REL_PREDICATE);

  // B ladder: relative scaling above the absolute noise floor.
  const double l_char = tolerance / REL_PREDICATE;
  GEO_COINCIDENCE_TOL  = std::max(kCoincidenceFloor, l_char * REL_MERGE_FINE);
  GEO_EXPORT_MERGE_TOL = std::max(kExportMergeFloor, l_char * REL_EXPORT_MERGE);
  GEO_RESAMPLE_MIN_SEP = std::max(kResampleFloor,    l_char * REL_RESAMPLE);
  GEO_NESTING_SLACK    = std::max(kCoincidenceFloor, l_char * REL_MERGE_FINE);
}
