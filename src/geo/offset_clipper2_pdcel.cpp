// PDCELVertex adapter for the Stage C reverse-match bridge.
//
// Per plan-20260515 §8.4 this translation unit is the planned
// deletion site when the end-state nested-offset orchestrator
// replaces BaseOffsetMap. Until then it bridges the pure-logic
// `planReverseMatch` (in offset_clipper2_bridge.cpp) to the
// PreVABS-native PDCELVertex* + BaseOffsetMap contract that
// `src/cs/PSegment.cpp` consumes.

#include "offset_clipper2.hpp"

#include "PDCELVertex.hpp"
#include "plog.hpp"

#include <sstream>
#include <utility>

namespace prevabs {
namespace geo {

ReverseMatchResult buildBaseOffsetMapFromOffsetPolygons(
    const std::vector<PDCELVertex*>&   base,
    bool                               base_is_closed,
    int                                side,
    double                             dist,
    const std::vector<OffsetPolygon>&  polygons) {
  ReverseMatchResult out;

  if (base.size() < 2) return out;

  // Drop trailing duplicate vertex on closed Baseline (front == back
  // pointer identity is the PreVABS convention). Open inputs are
  // passed through unchanged — front and back are expected to be
  // distinct end-points.
  std::vector<PDCELVertex*> base_distinct = base;
  const bool input_has_trailing_dup =
      base_is_closed && base.size() >= 2 && base.front() == base.back();
  if (input_has_trailing_dup) {
    base_distinct.pop_back();
  }

  std::vector<SPoint2> base_pts;
  base_pts.reserve(base_distinct.size());
  for (auto* v : base_distinct) {
    base_pts.emplace_back(v->point2()[0], v->point2()[1]);
  }

  const ReverseMatchPlan plan = planReverseMatch(
      base_pts, base_is_closed, side, dist, polygons);

  if (!plan.ok) {
    PLOG(error) << "offset clipper2 bridge: failed to build a valid "
                   "BaseOffsetMap from Clipper2 output";
    return out;
  }

  // Stage F multi-branch diagnostic: detail per-polygon areas so the
  // user can see which pieces were dropped and how big they were
  // (plan-20260514 §5.1). The bridge already picked the primary by
  // largest |area| + base-bbox-centroid tie-break.
  if (!plan.dropped_polygon_areas.empty()) {
    std::ostringstream oss;
    oss.precision(6);
    oss << "offset clipper2 bridge: " << (1 + plan.dropped_polygon_areas.size())
        << " disconnected polygons; kept primary (area="
        << plan.primary_polygon_area << "), dropped smaller pieces (areas=";
    for (std::size_t i = 0; i < plan.dropped_polygon_areas.size(); ++i) {
      if (i > 0) oss << ", ";
      oss << plan.dropped_polygon_areas[i];
    }
    oss << ")";
    PLOG(warning) << oss.str();
  }

  // Reserve +1 to leave room for the closed-only trailing-duplicate
  // pointer below; for open inputs the extra slot is unused.
  out.offset_vertices.reserve(plan.offset_points.size() + 1);
  out.offset_resampled.reserve(plan.offset_points.size() + 1);
  // Defensive: if upstream skipped populating the tag, treat every
  // vertex as raw rather than crashing on a size mismatch.
  const bool tag_aligned =
      plan.offset_resampled.size() == plan.offset_points.size();
  for (std::size_t k = 0; k < plan.offset_points.size(); ++k) {
    const auto& p = plan.offset_points[k];
    out.offset_vertices.push_back(new PDCELVertex(0.0, p.x(), p.y()));
    out.offset_resampled.push_back(tag_aligned ? plan.offset_resampled[k]
                                               : false);
  }
  if (base_is_closed && !out.offset_vertices.empty()) {
    // §5.6 closed convention: front and back are the same pointer.
    // Open inputs skip this — `offset_vertices` stays end-to-end
    // distinct so `.front()` and `.back()` align 1:1 with
    // `base.front()` and `base.back()` (matching `PSegment::build`'s
    // expectation for an open offset curve).
    out.offset_vertices.push_back(out.offset_vertices.front());
    out.offset_resampled.push_back(out.offset_resampled.front());
  }

  out.id_pairs                  = plan.id_pairs;
  out.dropped_base_ranges_lo    = plan.dropped_base_ranges_lo;
  out.dropped_base_ranges_hi    = plan.dropped_base_ranges_hi;
  out.pre_resample_raw_points   = plan.pre_resample_raw_points;
  out.ok                        = true;
  return out;
}

}  // namespace geo
}  // namespace prevabs
