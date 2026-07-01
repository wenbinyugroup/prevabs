#pragma once

// A0 of Clipper2 plan: load UIUC-style airfoil .dat files into a
// Clipper2 PathD. **Zero PreVABS dependency** — this header pulls in
// only the C++ stdlib and clipper2/clipper.h.
//
// Format expected (UIUC):
//   line 1: free-form header (e.g. "AH 79-K-143/18"), ignored
//   lines 2..: "x y" pairs in [0,1] x [-something, +something], two
//   doubles separated by whitespace. Comments / blank lines tolerated.
//
// Order is conventionally TE -> upper surface -> LE -> lower surface
// -> TE. The TE may close (first == last) or be open (different y
// values at x=1), corresponding to either a sharp or finite TE
// thickness. Both cases are accepted.

#include "clipper2/clipper.h"

#include <string>
#include <vector>

namespace clipper2_airfoil {

struct AirfoilProfile {
  std::string         name;         // header line from the .dat file
  Clipper2Lib::PathD  points;       // 2-D contour vertices in chord units
  bool                te_closed;    // first == last vertex exactly
  double              te_gap;       // |first - last|, 0 when te_closed
};

// Load airfoil from a .dat file given by absolute or repo-relative
// path. Throws std::runtime_error on parse failure.
AirfoilProfile loadFromFile(const std::string& path);

// Load airfoil by short name (e.g. "ah79k143"). Resolves to
// CLIPPER2_AIRFOIL_DIR/<name>.dat. CLIPPER2_AIRFOIL_DIR is a compile
// definition set by CMakeLists.txt.
AirfoilProfile loadByName(const std::string& short_name);

// Generate a synthetic NACA 4-digit airfoil polyline with the
// standard cosine-spaced chord distribution. n_chord is the number of
// stations from LE to TE on each surface (so the resulting contour
// has roughly 2*n_chord - 1 vertices). Returns a *closed* sharp-TE
// profile.
AirfoilProfile makeNaca4(const std::string& four_digit,
                         int                n_chord = 60,
                         double             chord   = 1.0);

}  // namespace clipper2_airfoil
