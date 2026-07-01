#pragma once

// Minimal hand-rolled SVG writer for failure diagnostics. We deliberately
// avoid Clipper2's clipper.svg utilities (CLIPPER2_UTILS=OFF in
// CMakeLists.txt) so this test target needs only the core Clipper2
// library.

#include "clipper2/clipper.h"

#include <string>

namespace clipper2_airfoil {

// Write an SVG with the base contour (blue, thin) and the offset
// solution (red, filled with low alpha) for visual diagnosis. The
// output is auto-scaled to fit a 800x600 canvas with a 20-unit margin.
// Returns the full path of the file that was written.
std::string dumpOffsetSvg(const std::string& tag,
                          const Clipper2Lib::PathD&  base,
                          const Clipper2Lib::PathsD& solution);

}  // namespace clipper2_airfoil
