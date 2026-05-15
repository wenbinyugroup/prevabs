#pragma once

// CurveFrameLookup
//
// Nearest-segment / tangent query for a 2-D polyline reference curve.
//
// Phase A of plan-20260514-decouple-local-frame-from-map.md.
//
// The class stores a polyline as a sequence of SPoint2 vertices in the
// cross-section plane (PreVABS convention: SPoint2(a, b) corresponds to
// 3-D coordinates (0, a, b)).  For any query point q it returns:
//   - the index of the closest segment
//   - the parameter u in [0, 1] of the foot of perpendicular on that
//     segment
//   - the unit tangent at that foot
// The unit tangent is returned as an SVector3 with x = 0, matching the
// 3-D tangent convention used by `PGeoLineSegment::toVector()`.

#include "geo_types.hpp"

#include <vector>

class CurveFrameLookup {
 public:
  struct Hit {
    int    seg_index;
    double u;
    double dist;
  };

  // Construct from a polyline.  When `closed` is true an extra implicit
  // segment from vertices.back() to vertices.front() is treated as part
  // of the curve.  Caller must supply at least two vertices.
  CurveFrameLookup(const std::vector<SPoint2>& vertices, bool closed);

  // Nearest segment / foot parameter / distance to query point q.
  Hit nearest(const SPoint2& q) const;

  // Unit tangent at parameter u on segment seg_index, returned as
  // SVector3(0, dy, dz) where (dy, dz) is the 2-D segment direction.
  SVector3 tangentAt(int seg_index, double u) const;

  // Convenience: returns the unit tangent at the foot of perpendicular
  // from q.  When the foot lies on a vertex shared by two segments, the
  // tangent of the segment selected by `nearest()` is returned.
  SVector3 yAxisAt(const SPoint2& q) const;

  // Number of segments stored (N-1 for open, N for closed).
  int segmentCount() const { return _seg_count; }

 private:
  std::vector<SPoint2> _v;
  bool                 _closed;
  int                  _seg_count;
};
