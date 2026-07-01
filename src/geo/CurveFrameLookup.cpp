#include "CurveFrameLookup.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>

namespace {

// Endpoints of segment seg_index on the stored polyline.
// Honors the closed flag: when `closed`, the implicit last segment runs
// from v.back() to v.front().
void segmentEndpoints(const std::vector<SPoint2>& v, bool closed,
                      int seg_index, SPoint2& a, SPoint2& b) {
  const int n = static_cast<int>(v.size());
  const int j = (closed && seg_index == n - 1) ? 0 : seg_index + 1;
  a = v[seg_index];
  b = v[j];
}

}  // namespace

CurveFrameLookup::CurveFrameLookup(const std::vector<SPoint2>& vertices,
                                   bool closed)
    : _v(vertices), _closed(closed) {
  assert(vertices.size() >= 2 && "CurveFrameLookup needs >= 2 vertices");
  const int n = static_cast<int>(vertices.size());
  // Open polyline: n-1 segments. Closed polyline: n segments.
  _seg_count = closed ? n : n - 1;
}

CurveFrameLookup::Hit CurveFrameLookup::nearest(const SPoint2& q) const {
  Hit best{-1, 0.0, std::numeric_limits<double>::infinity()};

  for (int i = 0; i < _seg_count; ++i) {
    SPoint2 a, b;
    segmentEndpoints(_v, _closed, i, a, b);

    const double dx = b.x() - a.x();
    const double dy = b.y() - a.y();
    const double len2 = dx * dx + dy * dy;
    if (len2 == 0.0) {
      // Degenerate zero-length segment — treat as a point at `a`.
      const double qx = q.x() - a.x();
      const double qy = q.y() - a.y();
      const double d = std::sqrt(qx * qx + qy * qy);
      if (d < best.dist) {
        best = {i, 0.0, d};
      }
      continue;
    }

    const double qx = q.x() - a.x();
    const double qy = q.y() - a.y();
    double u = (qx * dx + qy * dy) / len2;
    if (u < 0.0) u = 0.0;
    if (u > 1.0) u = 1.0;

    const double fx = a.x() + u * dx;
    const double fy = a.y() + u * dy;
    const double rx = q.x() - fx;
    const double ry = q.y() - fy;
    const double d = std::sqrt(rx * rx + ry * ry);

    if (d < best.dist) {
      best = {i, u, d};
    }
  }

  return best;
}

SVector3 CurveFrameLookup::tangentAt(int seg_index, double /*u*/) const {
  assert(seg_index >= 0 && seg_index < _seg_count);
  SPoint2 a, b;
  segmentEndpoints(_v, _closed, seg_index, a, b);

  // Polyline tangent is constant along a segment.
  const double dy = b.x() - a.x();
  const double dz = b.y() - a.y();
  const double n  = std::sqrt(dy * dy + dz * dz);
  if (n == 0.0) {
    return SVector3(0.0, 0.0, 0.0);
  }
  return SVector3(0.0, dy / n, dz / n);
}

SVector3 CurveFrameLookup::yAxisAt(const SPoint2& q) const {
  const Hit h = nearest(q);
  return tangentAt(h.seg_index, h.u);
}
