#include "offset_clipper2.hpp"

#include "clipper2/clipper.h"
#include "globalConstants.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace prevabs {
namespace geo {

namespace {

// Forward declarations — extractOpenRuns calls attributeSource which is
// defined later in this TU.
struct Projection;
int segmentCount(int n_vertices, bool closed);
OffsetVertexSource attributeSource(double qx, double qy,
                                   const std::vector<SPoint2>& base,
                                   bool base_is_closed,
                                   double source_radius);

// Foot-of-perpendicular projection of (qx, qy) onto segment [a, b].
// Returns u clamped to [0, 1] and the foot-to-query distance.
struct Projection {
  double u;
  double dist;
};

Projection projectOnSegment(double qx, double qy,
                            double ax, double ay,
                            double bx, double by) {
  const double dx = bx - ax;
  const double dy = by - ay;
  const double len2 = dx * dx + dy * dy;
  if (len2 == 0.0) {
    const double rx = qx - ax;
    const double ry = qy - ay;
    return {0.0, std::sqrt(rx * rx + ry * ry)};
  }
  double u = ((qx - ax) * dx + (qy - ay) * dy) / len2;
  if (u < 0.0) u = 0.0;
  if (u > 1.0) u = 1.0;
  const double fx = ax + u * dx;
  const double fy = ay + u * dy;
  const double rx = qx - fx;
  const double ry = qy - fy;
  return {u, std::sqrt(rx * rx + ry * ry)};
}

// Find the closest open-base segment for a query point (no source_radius
// gate). Returns (-1, 0) only if base has fewer than 2 vertices. Used by
// side-classification on the Butt-wrap polygon, where every wrap vertex
// is geometrically near *some* base segment but a few may sit beyond the
// 1.5*dist attribution radius (miter join points).
OffsetVertexSource closestOpenSegment(double qx, double qy,
                                      const std::vector<SPoint2>& base) {
  const int n_s = static_cast<int>(base.size()) - 1;
  if (n_s <= 0) return {-1, 0.0};
  OffsetVertexSource best{0, 0.0};
  double best_d = std::numeric_limits<double>::infinity();
  for (int i = 0; i < n_s; ++i) {
    const auto& a = base[i];
    const auto& b = base[i + 1];
    const Projection p =
        projectOnSegment(qx, qy, a.x(), a.y(), b.x(), b.y());
    if (p.dist < best_d) {
      best_d = p.dist;
      best = {i, p.u};
    }
  }
  return best;
}

bool unitOpenOffsetNormal(const SPoint2& a, const SPoint2& b,
                          int side, SPoint2* normal) {
  const double dx = b.x() - a.x();
  const double dy = b.y() - a.y();
  const double len = std::sqrt(dx * dx + dy * dy);
  if (len == 0.0) return false;
  const double s = side >= 0 ? 1.0 : -1.0;
  *normal = SPoint2(s * -dy / len, s * dx / len);
  return true;
}

// Direction used to project one interior base vertex onto the raw offset run:
// the normalized sum of the two adjacent segment normals on the requested
// offset side. For a straight base the normals coincide, so the direction is
// the ordinary segment normal.
bool openOffsetBisector(const std::vector<SPoint2>& base, int i, int side,
                        SPoint2* direction) {
  const int N = static_cast<int>(base.size());
  if (N < 3 || i <= 0 || i >= N - 1) return false;

  SPoint2 n0;
  SPoint2 n1;
  if (!unitOpenOffsetNormal(base[i - 1], base[i], side, &n0)
      || !unitOpenOffsetNormal(base[i], base[i + 1], side, &n1)) {
    return false;
  }
  const double bx = n0.x() + n1.x();
  const double by = n0.y() + n1.y();
  const double len = std::sqrt(bx * bx + by * by);
  if (len <= 1e-14) return false;
  *direction = SPoint2(bx / len, by / len);
  return true;
}

// Intersect the positive ray origin + t*direction with segment [a,b].
// Returns the nearest point on the ray only when t >= 0 and segment parameter
// u lies in [0,1].
bool raySegmentIntersection(const SPoint2& origin,
                            const SPoint2& direction,
                            const SPoint2& a, const SPoint2& b,
                            double* ray_t, double* segment_u,
                            SPoint2* point) {
  const double sx = b.x() - a.x();
  const double sy = b.y() - a.y();
  const double det = direction.x() * sy - direction.y() * sx;
  if (std::fabs(det) < 1e-14) return false;

  const double qx = a.x() - origin.x();
  const double qy = a.y() - origin.y();
  double t = (qx * sy - qy * sx) / det;
  double u = (qx * direction.y() - qy * direction.x()) / det;
  const double tol = 1e-12;
  if (t < -tol || u < -tol || u > 1.0 + tol) return false;

  if (t < 0.0) t = 0.0;
  if (u < 0.0) u = 0.0;
  if (u > 1.0) u = 1.0;
  *ray_t = t;
  *segment_u = u;
  *point = SPoint2(origin.x() + t * direction.x(),
                   origin.y() + t * direction.y());
  return true;
}

bool projectAlongOpenBisector(const SPoint2& base_point,
                              const SPoint2& direction,
                              const std::vector<SPoint2>& run,
                              int start_seg, int* hit_seg,
                              double* hit_u,
                              SPoint2* projected) {
  const int n_seg = static_cast<int>(run.size()) - 1;
  double best_t = std::numeric_limits<double>::infinity();
  int best_seg = -1;
  double best_u = 0.0;
  SPoint2 best_point;
  for (int s = start_seg; s < n_seg; ++s) {
    double t = 0.0;
    double u = 0.0;
    SPoint2 q;
    if (raySegmentIntersection(base_point, direction, run[s], run[s + 1],
                               &t, &u, &q)
        && t < best_t) {
      best_t = t;
      best_seg = s;
      best_u = u;
      best_point = q;
    }
  }
  if (best_seg < 0) return false;
  *hit_seg = best_seg;
  *hit_u = best_u;
  *projected = best_point;
  return true;
}

bool samePoint(const SPoint2& a, const SPoint2& b) {
  return std::fabs(a.x() - b.x()) <= GEO_TOL
      && std::fabs(a.y() - b.y()) <= GEO_TOL;
}

// Sign of (t × d) where t is the local base tangent at the foot of
// perpendicular and d is the displacement from foot to query point.
// +1 → left of base, -1 → right, 0 → degenerate.
//
// Corner handling: when the foot lands at a shared base vertex (u ≈ 0
// or u ≈ 1 with an adjacent segment present), the picked segment's
// tangent is ambiguous — picking the wrong one can flip the sign for
// query points on the outer side of an acute corner (e.g. miter-limit
// bevel vertices in a V-shape). We average the two adjacent segment
// tangents at the corner, which mirrors the legacy n × t formula in
// `signedHalfThickness`.
int sideSignOfBase(double qx, double qy,
                   const std::vector<SPoint2>& base) {
  if (base.size() < 2) return 0;
  const OffsetVertexSource src = closestOpenSegment(qx, qy, base);
  const int n_seg = static_cast<int>(base.size()) - 1;
  const double u_eps = 1e-9;

  double tx, ty, fx, fy;
  if (src.base_u < u_eps && src.base_seg > 0) {
    // Foot at base[src.base_seg]: shared with prev segment. Average.
    const auto& prev_a = base[src.base_seg - 1];
    const auto& prev_b = base[src.base_seg];
    const auto& cur_b  = base[src.base_seg + 1];
    tx = (prev_b.x() - prev_a.x()) + (cur_b.x() - prev_b.x());
    ty = (prev_b.y() - prev_a.y()) + (cur_b.y() - prev_b.y());
    fx = prev_b.x();
    fy = prev_b.y();
  } else if (src.base_u > 1.0 - u_eps && src.base_seg + 1 < n_seg) {
    // Foot at base[src.base_seg+1]: shared with next segment. Average.
    const auto& cur_a  = base[src.base_seg];
    const auto& cur_b  = base[src.base_seg + 1];
    const auto& next_b = base[src.base_seg + 2];
    tx = (cur_b.x() - cur_a.x()) + (next_b.x() - cur_b.x());
    ty = (cur_b.y() - cur_a.y()) + (next_b.y() - cur_b.y());
    fx = cur_b.x();
    fy = cur_b.y();
  } else {
    const auto& a = base[src.base_seg];
    const auto& b = base[src.base_seg + 1];
    tx = b.x() - a.x();
    ty = b.y() - a.y();
    fx = a.x() + src.base_u * tx;
    fy = a.y() + src.base_u * ty;
  }

  const double dx = qx - fx;
  const double dy = qy - fy;
  const double cross = tx * dy - ty * dx;
  if (cross > 0.0) return +1;
  if (cross < 0.0) return -1;
  return 0;
}

// Extract one or more single-sided open polylines from the CCW closed
// "Butt wrap" polygon Clipper2 returns for an open base.
//
// Geometry of the wrap (Clipper2 EndType::Butt + open path):
//   ┌─ left side    (vertices with t × d > 0; base_seg ascending)
//   │  left end-cap vertex at P_{N-1}        (t × d > 0)
//   │  right end-cap vertex at P_{N-1}       (t × d < 0)
//   ├─ right side   (vertices with t × d < 0; base_seg descending)
//   │  right end-cap vertex at P_0           (t × d < 0)
//   │  left end-cap vertex at P_0            (t × d > 0)
//   └─ back to start
//
// Side tags partition the cycle into exactly two contiguous runs (one
// per side); end-cap vertices fall naturally into their respective side
// because the Butt cap is a single flat edge whose two endpoints sit on
// opposite sides of the base tangent at the end point.
//
// For requested side `side`:
//   - tag each wrap vertex by sign(t × d) using closestOpenSegment
//     (the source_radius gate is bypassed here — Clipper2 may insert
//     miter-join vertices slightly outside 1.5*dist that still belong
//     to a well-defined side).
//   - walk the cycle and collect maximal runs of vertices whose tag
//     equals `side`.
//   - within each run, reverse if base_seg sequence is strictly
//     descending, so all runs walk in base direction (P_0 → P_{N-1}).
//
// For a non-degenerate open base + reasonable dist this returns exactly
// one run with N or N+1 vertices (one per base vertex plus an extra at
// each Butt cap). Pathological inputs (very high curvature, large
// dist/length ratio) may yield multiple runs; callers can keep the run
// with the widest base_seg span or surface a warning.
std::vector<OffsetPolygon> extractOpenRuns(
    const Clipper2Lib::PathD&       path,
    const std::vector<SPoint2>&     base,
    int                             side,
    double                          source_radius,
    double                          dist,
    bool                            do_resample,
    bool                            do_miter_resample,
    OpenResampleMode                resample_mode) {
  std::vector<OffsetPolygon> runs;
  const int M = static_cast<int>(path.size());
  if (M < 2 || base.size() < 2) return runs;

  // 1. Per-vertex side tag.
  std::vector<int> tag(M, 0);
  for (int k = 0; k < M; ++k) {
    tag[k] = sideSignOfBase(path[k].x, path[k].y, base);
  }

  // 2. Find a cycle boundary (an index k where tag[k] != side).
  int start = -1;
  for (int k = 0; k < M; ++k) {
    if (tag[k] != side) { start = k; break; }
  }

  // 3a. All vertices match `side` — pathological; treat the whole wrap
  // as one run (caller's bridge will report it).
  if (start < 0) {
    OffsetPolygon op;
    op.is_closed = false;
    op.points.reserve(M);
    op.sources.reserve(M);
    op.resampled.reserve(M);
    for (int k = 0; k < M; ++k) {
      op.points.emplace_back(path[k].x, path[k].y);
      op.sources.push_back(
          attributeSource(path[k].x, path[k].y, base,
                          /*closed*/ false, source_radius));
      op.resampled.push_back(false);
    }
    runs.push_back(std::move(op));
    return runs;
  }

  // 3b. Walk one full cycle starting just past `start`, accumulating
  // contiguous runs of tag == side.
  OffsetPolygon cur;
  cur.is_closed = false;
  bool in_run = false;
  for (int step = 1; step <= M; ++step) {
    const int k = (start + step) % M;
    if (tag[k] == side) {
      cur.points.emplace_back(path[k].x, path[k].y);
      cur.sources.push_back(
          attributeSource(path[k].x, path[k].y, base,
                          /*closed*/ false, source_radius));
      cur.resampled.push_back(false);
      in_run = true;
    } else if (in_run) {
      runs.push_back(std::move(cur));
      cur = OffsetPolygon();
      cur.is_closed = false;
      in_run = false;
    }
  }
  if (in_run && !cur.points.empty()) {
    runs.push_back(std::move(cur));
  }

  // 4. Orient each run so its base_seg sequence is non-decreasing
  // (i.e. walks from P_0 toward P_{N-1} of the base).
  for (auto& r : runs) {
    int first_seg = -1;
    int last_seg  = -1;
    for (const auto& s : r.sources) {
      if (s.base_seg >= 0) {
        if (first_seg < 0) first_seg = s.base_seg;
        last_seg = s.base_seg;
      }
    }
    if (first_seg >= 0 && last_seg >= 0 && first_seg > last_seg) {
      std::reverse(r.points.begin(), r.points.end());
      std::reverse(r.sources.begin(), r.sources.end());
      std::reverse(r.resampled.begin(), r.resampled.end());
    }
  }

  // 5. Resample at base-vertex resolution (open-only). Each interior base
  //    vertex angle bisector is intersected with the raw run. Insert mode
  //    augments the raw run; Replace mode rebuilds it from the intersections.
  //    Dropped base vertices stay out and Stage C records them. The logic lives in
  //    `resampleOpenRuns` so the offset / build-base-offset-map split can drive
  //    it from the map step instead of from the geometry core.
  if (do_resample) {
    resampleOpenRuns(runs, base, side, dist, do_miter_resample,
                     resample_mode);
  }

  return runs;
}

// Iterate over the input segments. For an open polyline of N vertices
// there are N - 1 segments; for a closed polyline of N vertices the
// implicit closing edge brings the count to N.
int segmentCount(int n_vertices, bool closed) {
  if (n_vertices < 2) return 0;
  return closed ? n_vertices : n_vertices - 1;
}

// Find the closest segment and its u-parameter for query point q.
// Linear scan (O(N)); N is typically a few hundred for airfoil-class
// inputs, so this is < 10 μs per query — under the perf budget set
// by the airfoil benchmark in CurveFrameLookup.
OffsetVertexSource attributeSource(double qx, double qy,
                                   const std::vector<SPoint2>& base,
                                   bool base_is_closed,
                                   double source_radius) {
  const int n_v = static_cast<int>(base.size());
  const int n_s = segmentCount(n_v, base_is_closed);
  OffsetVertexSource best{-1, 0.0};
  double best_d = std::numeric_limits<double>::infinity();
  for (int i = 0; i < n_s; ++i) {
    const int j = (base_is_closed && i == n_v - 1) ? 0 : i + 1;
    const auto& a = base[i];
    const auto& b = base[j];
    const Projection p =
        projectOnSegment(qx, qy, a.x(), a.y(), b.x(), b.y());
    if (p.dist < best_d) {
      best_d = p.dist;
      best = {i, p.u};
    }
  }
  // Outside the "physical offset shell": likely a join-point vertex
  // Clipper2 inserted that has no analog on the input polyline.
  if (best_d > source_radius) return {-1, 0.0};
  return best;
}

}  // namespace

OpenResampleMode openResampleModeFromString(const std::string& value) {
  return value == "replace"
      ? OpenResampleMode::Replace : OpenResampleMode::Insert;
}

// Add base-vertex-resolution points to one open offset run. Insert mode keeps
// every raw Clipper2 point and inserts the calculated points in run order;
// replace mode rebuilds the run from calculated points for diagnostics and
// comparison with the previous behavior.
// Dropped base vertices stay out and the Stage C bridge's forward-fill records
// them as `dropped_base_ranges`. Each covered base vertex is projected along
// the adjacent-segment angle bisector on `side`, so outer miter apexes are
// retained instead of being replaced by a perpendicular foot on one offset
// edge. No-op unless `runs` holds exactly one open run with >= 2 points;
// mutates `runs[0]` in place (points/sources/resampled, and stores the
// pre-resample raw run for debug overlays in Replace mode).
//
// Lives at namespace scope (declared in the header) so the offset /
// build-base-offset-map split can invoke it from the map step; `extractOpenRuns`
// also calls it (open-input geometry path) when its `do_resample` flag is set.
void resampleOpenRuns(std::vector<OffsetPolygon>& runs,
                      const std::vector<SPoint2>& base,
                      int side, double dist, bool do_miter_resample,
                      OpenResampleMode mode) {
  if (!(runs.size() == 1 && !runs[0].is_closed
        && runs[0].points.size() >= 2)) {
    return;
  }
  auto& r = runs[0];
  const int N = static_cast<int>(base.size());
  if (N < 2) return;
  (void)dist;
  (void)do_miter_resample;  // Temporarily bypass the synthetic miter branch.

  struct CalculatedPoint {
    SPoint2 point;
    OffsetVertexSource source;
    int raw_seg;
    double raw_u;
  };

  // Production angle-bisector resample: one point per covered base vertex,
  // intersected with the raw run polyline along the requested offset side.
  std::vector<bool> base_covered(N, false);
  for (const auto& s : r.sources) {
    if (s.base_seg < 0) continue;
    if (s.base_seg     >= 0 && s.base_seg     < N) base_covered[s.base_seg]     = true;
    if (s.base_seg + 1 >= 0 && s.base_seg + 1 < N) base_covered[s.base_seg + 1] = true;
  }

  std::vector<CalculatedPoint> calculated;
  calculated.reserve(N);

  int cur_seg = 0;
  for (int i = 0; i < N; ++i) {
    if (!base_covered[i]) continue;

    int best_seg = cur_seg;
    double raw_u = 0.0;
    SPoint2 projected;
    if (i == 0) {
      projected = r.points.front();
      best_seg = 0;
    } else if (i == N - 1) {
      projected = r.points.back();
      best_seg = static_cast<int>(r.points.size()) - 2;
      raw_u = 1.0;
    } else {
      SPoint2 direction;
      if (!openOffsetBisector(base, i, side, &direction)
          || !projectAlongOpenBisector(base[i], direction, r.points, cur_seg,
                                       &best_seg, &raw_u, &projected)) {
        continue;
      }
    }

    OffsetVertexSource src;
    src.base_seg = (i == 0) ? 0 : (i - 1);
    src.base_u   = (i == 0) ? 0.0 : 1.0;
    calculated.push_back({projected, src, best_seg, raw_u});

    cur_seg = best_seg;
  }

  if (calculated.size() < 2) return;

  if (mode == OpenResampleMode::Replace) {
    std::vector<SPoint2>            compact_pts;
    std::vector<OffsetVertexSource> compact_srcs;
    std::vector<bool>               compact_resampled;
    compact_pts.reserve(calculated.size());
    compact_srcs.reserve(calculated.size());
    compact_resampled.reserve(calculated.size());
    for (const auto& p : calculated) {
      if (!compact_pts.empty() && samePoint(compact_pts.back(), p.point)) {
        continue;
      }
      compact_pts.push_back(p.point);
      compact_srcs.push_back(p.source);
      compact_resampled.push_back(true);
    }
    r.pre_resample_points = r.points;
    r.points    = std::move(compact_pts);
    r.sources   = std::move(compact_srcs);
    r.resampled = std::move(compact_resampled);
    return;
  }

  // Raw-priority insertion: a calculated point coincident with any raw point
  // is discarded, so the original coordinate, source, and provenance survive.
  const std::size_t raw_count = r.points.size();
  std::vector<std::vector<CalculatedPoint>> by_segment(raw_count - 1);
  std::vector<SPoint2> accepted_points;
  for (const auto& p : calculated) {
    bool duplicate = false;
    for (const auto& raw : r.points) {
      if (samePoint(raw, p.point)) {
        duplicate = true;
        break;
      }
    }
    if (!duplicate) {
      for (const auto& accepted : accepted_points) {
        if (samePoint(accepted, p.point)) {
          duplicate = true;
          break;
        }
      }
    }
    if (duplicate || p.raw_seg < 0
        || p.raw_seg >= static_cast<int>(by_segment.size())) {
      continue;
    }
    by_segment[p.raw_seg].push_back(p);
    accepted_points.push_back(p.point);
  }
  for (auto& bucket : by_segment) {
    std::sort(bucket.begin(), bucket.end(),
              [](const CalculatedPoint& a, const CalculatedPoint& b) {
                return a.raw_u < b.raw_u;
              });
  }

  std::vector<SPoint2>            augmented_pts;
  std::vector<OffsetVertexSource> augmented_srcs;
  std::vector<bool>               augmented_resampled;
  augmented_pts.reserve(raw_count + accepted_points.size());
  augmented_srcs.reserve(raw_count + accepted_points.size());
  augmented_resampled.reserve(raw_count + accepted_points.size());
  const bool sources_aligned = r.sources.size() == raw_count;
  const bool tags_aligned = r.resampled.size() == raw_count;
  for (std::size_t s = 0; s + 1 < raw_count; ++s) {
    augmented_pts.push_back(r.points[s]);
    augmented_srcs.push_back(sources_aligned
        ? r.sources[s] : OffsetVertexSource{-1, 0.0});
    augmented_resampled.push_back(tags_aligned ? r.resampled[s] : false);
    for (const auto& p : by_segment[s]) {
      augmented_pts.push_back(p.point);
      augmented_srcs.push_back(p.source);
      augmented_resampled.push_back(true);
    }
  }
  augmented_pts.push_back(r.points.back());
  augmented_srcs.push_back(sources_aligned
      ? r.sources.back() : OffsetVertexSource{-1, 0.0});
  augmented_resampled.push_back(tags_aligned ? r.resampled.back() : false);

  r.pre_resample_points.clear();
  r.points = std::move(augmented_pts);
  r.sources = std::move(augmented_srcs);
  r.resampled = std::move(augmented_resampled);
}

std::vector<OffsetPolygon> offsetWithClipper2(
    const std::vector<SPoint2>& base,
    bool                        base_is_closed,
    int                         side,
    double                      dist,
    JoinTypeChoice              join,
    double                      miter_limit,
    bool                        resample_open,
    bool                        experimental_open_miter_resample,
    OpenResampleMode            resample_mode) {

  std::vector<OffsetPolygon> result;
  if (base.size() < 2 || !(dist > 0.0)) return result;

  // Defensive: closed inputs sometimes carry a trailing duplicate
  // (UIUC convention, PreVABS Baseline convention). Drop it so
  // Clipper2 does not see a degenerate closing segment of length 0.
  std::vector<SPoint2> input = base;
  if (base_is_closed && input.size() > 2) {
    const auto& f = input.front();
    const auto& b = input.back();
    if (f.x() == b.x() && f.y() == b.y()) {
      input.pop_back();
    }
  }
  if (input.size() < 2) return result;

  // Build Clipper2 PathD in the yz plane (PreVABS convention: SPoint2
  // x/y *are* yz; the cross-section problem is in the x = 0 plane).
  Clipper2Lib::PathD subj;
  subj.reserve(input.size());
  for (const auto& v : input) {
    subj.push_back({v.x(), v.y()});
  }
  Clipper2Lib::PathsD subjs;
  subjs.push_back(std::move(subj));

  // PreVABS side convention at this API layer (Clipper2-native):
  //   +1 = left  of base (CCW outward, sign(t × d) > 0)
  //   -1 = right of base (CCW inward,  sign(t × d) < 0)
  // For closed inputs Clipper2 InflatePaths consumes `side` directly via
  // signed delta (positive delta = outward expansion).
  // For open inputs Clipper2 requires positive delta and returns a
  // *closed* "Butt-wrap" polygon that surrounds the polyline on BOTH
  // sides. We pass +|dist|, then post-filter the wrap into one or more
  // single-sided open OffsetPolygons via `extractOpenRuns`.
  const double delta = base_is_closed
                         ? (side >= 0 ? +1.0 : -1.0) * dist
                         : dist;
  Clipper2Lib::JoinType jt = Clipper2Lib::JoinType::Miter;
  switch (join) {
    case JoinTypeChoice::Miter:  jt = Clipper2Lib::JoinType::Miter;  break;
    case JoinTypeChoice::Square: jt = Clipper2Lib::JoinType::Square; break;
    case JoinTypeChoice::Bevel:  jt = Clipper2Lib::JoinType::Bevel;  break;
    case JoinTypeChoice::Round:  jt = Clipper2Lib::JoinType::Round;  break;
  }
  const Clipper2Lib::EndType  et = base_is_closed
                                     ? Clipper2Lib::EndType::Polygon
                                     : Clipper2Lib::EndType::Butt;

  // precision = 8 is the Clipper2 1.4.0 hard ceiling — see
  // local/issue-20260515-clipper2-airfoil-a0.md §3.1.
  // miter_limit defaults to 2.0 (legacy kOffsetMiterLimitFactor); only
  // honoured by jt == Miter, ignored by other join types.
  constexpr int kClipperPrecision = 8;
  const Clipper2Lib::PathsD sol = Clipper2Lib::InflatePaths(
      subjs, delta, jt, et, miter_limit, kClipperPrecision);

  const double source_radius = 1.5 * dist;

  if (base_is_closed) {
    // Closed branch: each Clipper2 path is the offset polygon itself.
    // Every vertex is raw (closed inputs don't go through resample).
    result.reserve(sol.size());
    for (const auto& path : sol) {
      OffsetPolygon op;
      op.is_closed = true;
      op.points.reserve(path.size());
      op.sources.reserve(path.size());
      op.resampled.assign(path.size(), false);
      for (const auto& p : path) {
        op.points.emplace_back(p.x, p.y);
        op.sources.push_back(
            attributeSource(p.x, p.y, input, /*closed*/ true, source_radius));
      }
      result.push_back(std::move(op));
    }
    return result;
  }

  // Open branch: extract side-filtered open polylines from each Butt
  // wrap path. Most well-formed inputs produce exactly one wrap path,
  // and exactly one run on the requested side.
  for (const auto& path : sol) {
    std::vector<OffsetPolygon> runs =
        extractOpenRuns(path, input, side, source_radius, dist, resample_open,
                        experimental_open_miter_resample
                            && join == JoinTypeChoice::Miter,
                        resample_mode);
    for (auto& r : runs) {
      result.push_back(std::move(r));
    }
  }
  return result;
}

}  // namespace geo
}  // namespace prevabs
