#pragma once

// ---------------------------------------------------------------------------
// Geometry diagnostic / check helpers.
//
// Home for geometry *checks* and *diagnostics* — functions that measure local
// geometry and emit warnings (informational or user-facing) but do NOT change
// the geometric result. New geometry checks/diagnostics belong here.
//
// First residents (extracted from offset.cpp, 2026-06-23): the offset
// distance-vs-edge robustness check, the local-thickness precheck, the M/N
// ratio diagnostic, and the shared `signedHalfThickness` measurement.
// ---------------------------------------------------------------------------

#include <cstddef>
#include <vector>

class PDCELVertex;

namespace prevabs {
namespace geo {

// Local half-thickness at base[i]: cast a ray from base[i] along the inward
// normal (per `side`, matching the offset cross-product convention) to the
// nearest non-adjacent base segment and return that distance. Returns +INF
// when the inward ray never re-enters the polygon (plenty of room locally).
// Closed inputs use all distinct vertices; open inputs return INF for the two
// endpoints (half-thickness undefined there).
double signedHalfThickness(const std::vector<PDCELVertex *> &base,
                           int i, int side, bool base_is_closed);

// Informational warning: |dist| large relative to the shortest base segment
// (offset construction may be numerically fragile). No-op for < 2 vertices.
void checkOffsetDistanceVsShortestEdge(
    const std::vector<PDCELVertex *> &base, double dist);

// Informational warning: base vertices whose local half-thickness is below
// |dist| (skin will be locally dropped) or in [|dist|, 2|dist|) (locally
// thin). Open inputs skip the two endpoints.
void warnLocalThinRegions(const std::vector<PDCELVertex *> &base,
                          int side, double dist, bool base_is_closed);

// User-facing warning: too few offset vertices relative to base (Clipper2
// merged corners during inset). Downstream gmsh recovery often fails below
// M/N = 0.7. `base_size` / `offset_size` are raw container sizes; closed
// inputs are corrected for the front==back duplicate internally.
void warnLowOffsetMNRatio(std::size_t base_size, std::size_t offset_size,
                          bool base_is_closed, double dist);

}  // namespace geo
}  // namespace prevabs
