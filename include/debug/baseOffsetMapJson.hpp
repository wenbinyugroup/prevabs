#pragma once

// baseOffsetMapJson.hpp
// Debug helper paired with `baseOffsetMapSvg.hpp`: write the same
// (base, offset, staircase) snapshot to a tiny machine-readable JSON
// file. Intended for Python prototypes that need the actual y-z
// polyline coordinates (not the SVG-projected pixel coords).
//
// plan-20260522-staircase-dp-matcher.md Phase A uses this hook to
// feed real mh104 / mh104_te_only / strip cases into the Python DP
// matcher prototype.

#include "geo_types.hpp"

#include <string>
#include <vector>

class PDCELVertex;
namespace prevabs {
namespace geo {
struct LinearAdaptiveThicknessPlan;
}
}

// JSON schema:
//   {
//     "title": "<segment name>",
//     "closed": true|false,
//     "side":   +1|-1,
//     "dist":   <layup total thickness, or 0 if unknown>,
//     "base":   [[y, z], ...],            // base vertex coordinates
//     "offset": [[y, z], ...],            // offset vertex coordinates
//     "pairs":  [[b_idx, o_idx], ...],    // BaseOffsetMap entries
//     "dropped_base_ranges_lo": [int...], // optional
//     "dropped_base_ranges_hi": [int...]  // optional
//   }
//
// Coordinate convention matches PreVABS y-z plane (same as SVG dump).
void dumpBaseOffsetMapJson(
    const std::string& path,
    const std::string& title,
    const std::vector<PDCELVertex*>& base,
    const std::vector<PDCELVertex*>& offset,
    const BaseOffsetMap& pairs,
    bool   closed,
    int    side,
    double dist,
    const std::vector<int>* dropped_base_ranges_lo = nullptr,
    const std::vector<int>* dropped_base_ranges_hi = nullptr,
    const prevabs::geo::LinearAdaptiveThicknessPlan* adaptive_plan = nullptr);
