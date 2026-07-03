#pragma once

#include <limits>

namespace dcel {

// DCEL-local numeric constants — replace the former dependency on the global
// PI / INF from globalConstants.hpp. Same values, so behavior is unchanged.
constexpr double kPi  = 3.141592653589793;
constexpr double kInf = std::numeric_limits<double>::infinity();

// Injected per-PDCEL configuration, replacing the DCEL core's former direct
// reads of the domain globals GEO_TOL (== TOLERANCE) and config.debug_level.
// The owner (e.g. PModel) fills this in before geometry is built.
struct Config {
  // Coincidence / on-segment length tolerance used by addVertex,
  // findCoincidentVertex and the sweep-line intersection endpoint tests.
  // Was the global GEO_TOL (== TOLERANCE); default matches its 1e-9 default.
  double geo_tol = 1e-9;
  // Gate for the verbose geometry dump in print_dcel (was
  // config.debug_level >= DebugLevel::geo). Default off.
  bool verbose_dump = false;
};

}  // namespace dcel
