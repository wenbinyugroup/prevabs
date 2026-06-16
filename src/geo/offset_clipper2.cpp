#include "offset_clipper2.hpp"

#include "clipper2/clipper.h"

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

SPoint2 openOffsetNormal(const SPoint2& a, const SPoint2& b,
                         int side, double dist) {
  const double dx = b.x() - a.x();
  const double dy = b.y() - a.y();
  const double len = std::sqrt(dx * dx + dy * dy);
  if (len == 0.0) return SPoint2(0.0, 0.0);
  const double s = side >= 0 ? 1.0 : -1.0;
  return SPoint2(s * -dy / len * dist, s * dx / len * dist);
}

bool lineIntersection(const SPoint2& p, const SPoint2& r,
                      const SPoint2& q, const SPoint2& s,
                      SPoint2* out) {
  const double det = r.x() * s.y() - r.y() * s.x();
  if (std::fabs(det) < 1e-14) return false;
  const double qpx = q.x() - p.x();
  const double qpy = q.y() - p.y();
  const double t = (qpx * s.y() - qpy * s.x()) / det;
  *out = SPoint2(p.x() + t * r.x(), p.y() + t * r.y());
  return true;
}

std::vector<SPoint2> buildOpenMiterOffset(
    const std::vector<SPoint2>& base, int side, double dist) {
  std::vector<SPoint2> out;
  const int N = static_cast<int>(base.size());
  if (N < 2 || !(dist > 0.0)) return out;

  std::vector<SPoint2> normals;
  normals.reserve(N - 1);
  for (int i = 0; i + 1 < N; ++i) {
    normals.push_back(openOffsetNormal(base[i], base[i + 1], side, dist));
  }

  out.reserve(N);
  out.push_back(SPoint2(base[0].x() + normals[0].x(),
                        base[0].y() + normals[0].y()));
  for (int i = 1; i + 1 < N; ++i) {
    const SPoint2 p0(base[i].x() + normals[i - 1].x(),
                     base[i].y() + normals[i - 1].y());
    const SPoint2 d0(base[i].x() - base[i - 1].x(),
                     base[i].y() - base[i - 1].y());
    const SPoint2 p1(base[i].x() + normals[i].x(),
                     base[i].y() + normals[i].y());
    const SPoint2 d1(base[i + 1].x() - base[i].x(),
                     base[i + 1].y() - base[i].y());
    SPoint2 q;
    if (lineIntersection(p0, d0, p1, d1, &q)) {
      out.push_back(q);
    } else {
      out.push_back(SPoint2(base[i].x() + 0.5 * (normals[i - 1].x()
                                                 + normals[i].x()),
                            base[i].y() + 0.5 * (normals[i - 1].y()
                                                 + normals[i].y())));
    }
  }
  out.push_back(SPoint2(base[N - 1].x() + normals[N - 2].x(),
                        base[N - 1].y() + normals[N - 2].y()));
  return out;
}

bool samePoint(const SPoint2& a, const SPoint2& b) {
  return std::fabs(a.x() - b.x()) <= 1e-14
      && std::fabs(a.y() - b.y()) <= 1e-14;
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
    bool                            do_miter_resample) {
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

  // 5. Resample at base-vertex resolution (covered subset only).
  //
  // Clipper2 collapses collinear interior vertices: a 3-vertex straight
  // base produces a 2-vertex run, leaving the middle base vertex with
  // no offset correspondent. PreVABS downstream (`PSegment::build` +
  // layup intersection insertion) relies on 1:1 base ↔ offset alignment
  // and breaks on M < N output. We resample the run polyline at every
  // base vertex covered by the raw run; dropped base vertices stay
  // out and the Stage C bridge's forward-fill handles them as
  // `dropped_base_ranges`.
  //
  // Attribution for resampled base[i] uses (seg = max(0, i-1),
  // u = i==0 ? 0 : 1). `attributeOne` open-branch maps these to base
  // index i ⇒ Stage C sees a clean staircase over the covered subset.
  //
  // Control group: `do_resample = false` skips this entire block — the
  // returned `points` is then the raw side-filtered run (M = raw
  // Clipper2 vertex count, sources already attributed via
  // `attributeSource` foot-of-perpendicular). Used by the standalone
  // experimentation harness; production keeps `do_resample = true`.
  if (do_resample && runs.size() == 1 && runs[0].points.size() >= 2) {
    auto& r = runs[0];
    const int N = static_cast<int>(base.size());

    if (do_miter_resample
        && static_cast<int>(2 * r.points.size()) >= N) {
      std::vector<SPoint2> miter_pts =
          buildOpenMiterOffset(base, side, dist);
      if (miter_pts.size() >= 2) {
        r.pre_resample_points = r.points;
        r.points = std::move(miter_pts);
        r.sources.clear();
        r.resampled.assign(r.points.size(), true);
        r.sources.reserve(r.points.size());
        for (int i = 0; i < static_cast<int>(r.points.size()); ++i) {
          OffsetVertexSource src;
          src.base_seg = (i == 0) ? 0 : (i - 1);
          src.base_u   = (i == 0) ? 0.0 : 1.0;
          r.sources.push_back(src);
        }
      }
      return runs;
    }

    std::vector<bool> base_covered(N, false);
    for (const auto& s : r.sources) {
      if (s.base_seg < 0) continue;
      if (s.base_seg     >= 0 && s.base_seg     < N) base_covered[s.base_seg]     = true;
      if (s.base_seg + 1 >= 0 && s.base_seg + 1 < N) base_covered[s.base_seg + 1] = true;
    }

    std::vector<SPoint2>            new_pts;
    std::vector<OffsetVertexSource> new_srcs;
    std::vector<bool>               new_resampled;
    new_pts.reserve(N);
    new_srcs.reserve(N);
    new_resampled.reserve(N);

    int cur_seg = 0;
    const int n_run_seg = static_cast<int>(r.points.size()) - 1;
    for (int i = 0; i < N; ++i) {
      if (!base_covered[i]) continue;

      int    best_seg = cur_seg;
      double best_u   = 0.0;
      double best_d   = std::numeric_limits<double>::infinity();
      for (int s = cur_seg; s < n_run_seg; ++s) {
        const Projection p = projectOnSegment(
            base[i].x(), base[i].y(),
            r.points[s].x(),     r.points[s].y(),
            r.points[s + 1].x(), r.points[s + 1].y());
        if (p.dist < best_d) {
          best_d   = p.dist;
          best_seg = s;
          best_u   = p.u;
        }
      }

      const auto& a = r.points[best_seg];
      const auto& b = r.points[best_seg + 1];
      new_pts.emplace_back(
          a.x() + best_u * (b.x() - a.x()),
          a.y() + best_u * (b.y() - a.y()));

      OffsetVertexSource src;
      src.base_seg = (i == 0) ? 0 : (i - 1);
      src.base_u   = (i == 0) ? 0.0 : 1.0;
      new_srcs.push_back(src);
      new_resampled.push_back(true);  // synthetic foot-of-perpendicular

      cur_seg = best_seg;
    }

    if (new_pts.size() >= 2) {
      std::vector<SPoint2>            compact_pts;
      std::vector<OffsetVertexSource> compact_srcs;
      std::vector<bool>               compact_resampled;
      compact_pts.reserve(new_pts.size());
      compact_srcs.reserve(new_srcs.size());
      compact_resampled.reserve(new_resampled.size());
      for (std::size_t k = 0; k < new_pts.size(); ++k) {
        if (!compact_pts.empty() && samePoint(compact_pts.back(), new_pts[k])) {
          continue;
        }
        compact_pts.push_back(new_pts[k]);
        compact_srcs.push_back(new_srcs[k]);
        compact_resampled.push_back(new_resampled[k]);
      }

      // Snapshot the raw run polyline before any positions are replaced
      // by the foot-of-perpendicular interpolations above. Debug overlays
      // use this to plot where Clipper2 originally placed vertices — those
      // positions are otherwise lost for slots tagged `resampled=true`.
      r.pre_resample_points = r.points;
      r.points    = std::move(compact_pts);
      r.sources   = std::move(compact_srcs);
      r.resampled = std::move(compact_resampled);
    }
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

std::vector<OffsetPolygon> offsetWithClipper2(
    const std::vector<SPoint2>& base,
    bool                        base_is_closed,
    int                         side,
    double                      dist,
    JoinTypeChoice              join,
    double                      miter_limit,
    bool                        resample_open,
    bool                        experimental_open_miter_resample) {

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
                            && join == JoinTypeChoice::Miter);
    for (auto& r : runs) {
      result.push_back(std::move(r));
    }
  }
  return result;
}

}  // namespace geo
}  // namespace prevabs
