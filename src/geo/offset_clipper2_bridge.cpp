// Pure-logic Stage C reverse-match bridge for plan-20260514-
// clipper2-offset-backend.md §5.
//
// This translation unit deliberately has **no PDCELVertex / spdlog /
// geo.hpp dependency** so it can be linked into standalone unit
// tests (test_offset_clipper2) alongside Stage B's offset_clipper2.cpp.
// The PDCELVertex adapter that consumes a ReverseMatchPlan and
// materialises PDCELVertex* objects lives in
// `offset_clipper2_pdcel.cpp` and pulls in the heavier headers.

#include "offset_clipper2.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <string>
#include <utility>

namespace prevabs {
namespace geo {

namespace {

// ---------------------------------------------------------------------------
// Local staircase validator — duplicates the body of
// src/geo/offset.cpp's `validateBaseOffsetMap` but stays in this TU so
// the pure-logic layer does not have to include geo.hpp (which would
// drag in PDCELVertex, Material, etc.). The single source of truth
// for the invariant is still `include/geo_types.hpp:27`.
// ---------------------------------------------------------------------------

bool staircaseValid(const BaseOffsetMap& m, std::string* err) {
  for (std::size_t i = 0; i < m.size(); ++i) {
    if (m[i].base < 0 || m[i].offset < 0) {
      if (err) *err = "negative index at pair " + std::to_string(i);
      return false;
    }
    if (i == 0) continue;
    const int db = m[i].base   - m[i - 1].base;
    const int dd = m[i].offset - m[i - 1].offset;
    if (db < 0 || dd < 0 || db > 1 || dd > 1) {
      if (err) {
        *err = "staircase violation between pairs "
               + std::to_string(i - 1) + " and " + std::to_string(i);
      }
      return false;
    }
  }
  return true;
}

// ---------------------------------------------------------------------------
// §5.1  pickPrimary — choose the largest-absolute-area polygon.
// ---------------------------------------------------------------------------

double signedArea(const std::vector<SPoint2>& p) {
  if (p.size() < 3) return 0.0;
  double a = 0.0;
  const std::size_t n = p.size();
  for (std::size_t i = 0; i < n; ++i) {
    const auto& p0 = p[i];
    const auto& p1 = p[(i + 1) % n];
    a += p0.x() * p1.y() - p1.x() * p0.y();
  }
  return 0.5 * a;
}

SPoint2 polygonCentroid(const std::vector<SPoint2>& p) {
  if (p.empty()) return SPoint2(0.0, 0.0);
  double sx = 0.0;
  double sy = 0.0;
  for (const auto& q : p) {
    sx += q.x();
    sy += q.y();
  }
  return SPoint2(sx / static_cast<double>(p.size()),
                 sy / static_cast<double>(p.size()));
}

SPoint2 bboxCentroid(const std::vector<SPoint2>& p) {
  if (p.empty()) return SPoint2(0.0, 0.0);
  double xmin = p[0].x(), xmax = p[0].x();
  double ymin = p[0].y(), ymax = p[0].y();
  for (const auto& q : p) {
    if (q.x() < xmin) xmin = q.x();
    if (q.x() > xmax) xmax = q.x();
    if (q.y() < ymin) ymin = q.y();
    if (q.y() > ymax) ymax = q.y();
  }
  return SPoint2(0.5 * (xmin + xmax), 0.5 * (ymin + ymax));
}

// pickPrimaryOpen: choose the open OffsetPolygon whose attributed
// `base_seg` indices span the widest range (max - min). Area is not a
// meaningful selector for one-sided open polylines (they have ~zero
// signed area), but base_seg span directly measures "how much of the
// base this run actually covers" — exactly the property we want when
// the open Butt-wrap filter has produced multiple disconnected runs
// from pathological inputs (very high curvature, large dist/length).
//
// Returns -1 when no candidate has >= 2 points or no attributed source.
int pickPrimaryOpen(const std::vector<OffsetPolygon>& polys,
                    const std::vector<SPoint2>&       base) {
  (void)base;
  int leader = -1;
  int leader_span = -1;
  for (int i = 0; i < static_cast<int>(polys.size()); ++i) {
    if (polys[i].points.size() < 2) continue;
    int lo = std::numeric_limits<int>::max();
    int hi = std::numeric_limits<int>::min();
    for (const auto& s : polys[i].sources) {
      if (s.base_seg >= 0) {
        if (s.base_seg < lo) lo = s.base_seg;
        if (s.base_seg > hi) hi = s.base_seg;
      }
    }
    if (lo > hi) continue;
    const int span = hi - lo;
    if (span > leader_span) {
      leader_span = span;
      leader = i;
    }
  }
  return leader;
}

// Stage F pickPrimary: largest |signed area|; on near-ties (within
// 5% of the best), break the tie by polygon centroid distance to the
// base bbox centroid (plan-20260514 §5.1 priority 2).
int pickPrimary(const std::vector<OffsetPolygon>& polys,
                const std::vector<SPoint2>&       base) {
  // Pass 1: largest |signed area|.
  int    leader = -1;
  double leader_area = -1.0;
  for (int i = 0; i < static_cast<int>(polys.size()); ++i) {
    if (polys[i].points.size() < 3) continue;
    const double a = std::fabs(signedArea(polys[i].points));
    if (a > leader_area) {
      leader_area = a;
      leader = i;
    }
  }
  if (leader < 0) return -1;

  // Pass 2: tie-break candidates within 5% of leader.
  constexpr double kTieFactor = 0.95;
  const double tie_threshold = leader_area * kTieFactor;
  int    n_close = 0;
  for (int i = 0; i < static_cast<int>(polys.size()); ++i) {
    if (polys[i].points.size() < 3) continue;
    if (std::fabs(signedArea(polys[i].points)) >= tie_threshold) {
      ++n_close;
    }
  }
  if (n_close <= 1) return leader;

  // Multiple candidates: prefer the one whose vertex-mean centroid is
  // closest to the base bbox centroid.
  const SPoint2 base_c = bboxCentroid(base);
  int    best = leader;
  double best_d2 = INFINITY;
  for (int i = 0; i < static_cast<int>(polys.size()); ++i) {
    if (polys[i].points.size() < 3) continue;
    if (std::fabs(signedArea(polys[i].points)) < tie_threshold) continue;
    const SPoint2 c = polygonCentroid(polys[i].points);
    const double dx = c.x() - base_c.x();
    const double dy = c.y() - base_c.y();
    const double d2 = dx * dx + dy * dy;
    if (d2 < best_d2) {
      best_d2 = d2;
      best = i;
    }
  }
  return best;
}

// ---------------------------------------------------------------------------
// §5.2  Per-vertex base attribution.
// ---------------------------------------------------------------------------

int attributeOne(const OffsetVertexSource& src,
                 int n_base_distinct,
                 bool closed) {
  if (src.base_seg < 0) return -1;
  int b = src.base_seg + (src.base_u > 0.5 ? 1 : 0);
  if (closed) {
    if (n_base_distinct <= 0) return -1;
    b = ((b % n_base_distinct) + n_base_distinct) % n_base_distinct;
  } else {
    if (b < 0) b = 0;
    if (b > n_base_distinct - 1) b = n_base_distinct - 1;
  }
  return b;
}

// Two-pass fill of -1 entries — forward sweep carries the last valid
// value forward, then a backward sweep catches leading gaps.
void forwardFill(std::vector<int>& cand) {
  const int n = static_cast<int>(cand.size());
  if (n == 0) return;
  int seed_val = -1;
  for (int v : cand) if (v >= 0) { seed_val = v; break; }
  if (seed_val < 0) return;

  int last = -1;
  for (int i = 0; i < n; ++i) {
    if (cand[i] >= 0) last = cand[i];
    else if (last >= 0) cand[i] = last;
  }
  int next = -1;
  for (int i = n - 1; i >= 0; --i) {
    if (cand[i] >= 0) next = cand[i];
    else if (next >= 0) cand[i] = next;
  }
}

// ---------------------------------------------------------------------------
// §5.3.1  Rotation — for closed inputs, rotate so the offset vertex
// at position 0 sits at the start of the lowest-cand_base run.
// ---------------------------------------------------------------------------

int findRotationStart(const std::vector<int>& cand) {
  const int n = static_cast<int>(cand.size());
  if (n == 0) return 0;
  int min_val = cand[0];
  for (int v : cand) if (v < min_val) min_val = v;
  for (int i = 0; i < n; ++i) {
    if (cand[i] == min_val) {
      const int prev = cand[(i - 1 + n) % n];
      if (prev != min_val) return i;
    }
  }
  return 0;
}

template <typename T>
void rotateInPlace(std::vector<T>& v, int k_start) {
  if (k_start <= 0) return;
  std::rotate(v.begin(), v.begin() + k_start, v.end());
}

// ---------------------------------------------------------------------------
// §5.3.2 / §5.3.3  Unroll cand_base[] into non-decreasing walked_base[].
// ---------------------------------------------------------------------------

std::vector<int> unrollMonotone(const std::vector<int>& cand,
                                int n_base_distinct,
                                bool closed) {
  const int m = static_cast<int>(cand.size());
  std::vector<int> walked(m, 0);
  if (m == 0) return walked;
  walked[0] = cand[0];
  for (int k = 1; k < m; ++k) {
    const int prev = walked[k - 1];
    const int raw  = cand[k];
    if (closed) {
      const int prev_mod = ((prev % n_base_distinct) + n_base_distinct)
                           % n_base_distinct;
      const int step = ((raw - prev_mod) % n_base_distinct
                        + n_base_distinct) % n_base_distinct;
      walked[k] = prev + step;
    } else {
      walked[k] = std::max(prev, raw);
    }
  }
  return walked;
}

// ---------------------------------------------------------------------------
// §5.3 / §5.4  Build id_pairs from walked_base, record dropped ranges.
// ---------------------------------------------------------------------------

void emit(BaseOffsetMap& m, int b, int o) { m.emplace_back(b, o); }

void buildIdPairs(const std::vector<int>& walked,
                  int n_base_distinct,
                  bool closed,
                  BaseOffsetMap& id_pairs,
                  std::vector<int>& dropped_lo,
                  std::vector<int>& dropped_hi) {
  const int m = static_cast<int>(walked.size());
  if (m == 0) return;

  emit(id_pairs, walked[0], 0);
  for (int k = 1; k < m; ++k) {
    const int prev_b = walked[k - 1];
    const int cur_b  = walked[k];
    if (cur_b == prev_b) {
      emit(id_pairs, cur_b, k);
    } else if (cur_b == prev_b + 1) {
      emit(id_pairs, cur_b, k);
    } else {
      for (int b = prev_b + 1; b < cur_b; ++b) {
        emit(id_pairs, b, k - 1);
      }
      emit(id_pairs, cur_b, k);
      dropped_lo.push_back(prev_b + 1);
      dropped_hi.push_back(cur_b - 1);
    }
  }

  // Tail forward-fill: if `walked.back()` doesn't reach the last
  // distinct base index, extend the staircase by repeating offset
  // index m-1. Applies to BOTH closed and open inputs — open paths
  // need it when the filtered run anchor doesn't reach base[N-1]
  // (typically only on pathological inputs).
  {
    const int last_b = walked.back();
    for (int b = last_b + 1; b < n_base_distinct; ++b) {
      emit(id_pairs, b, m - 1);
    }
    if (last_b < n_base_distinct - 1) {
      dropped_lo.push_back(last_b + 1);
      dropped_hi.push_back(n_base_distinct - 1);
    }
  }

  if (closed) {
    // Wrap pair (N_distinct, M_off_raw) — closed-only. Lets the
    // PDCELVertex adapter wire the trailing-duplicate cleanly.
    emit(id_pairs, n_base_distinct, m);
  }
}

// Force id_pairs to start at base index 0. Prepends (0,0)..(b0-1,0)
// and records the prepended range as dropped. Applies to both closed
// and open inputs (the open filtered run anchor may not coincide with
// base[0] on pathological inputs).
void anchorAtZero(BaseOffsetMap& id_pairs,
                  std::vector<int>& dropped_lo,
                  std::vector<int>& dropped_hi) {
  if (id_pairs.empty()) return;
  const int b0 = id_pairs.front().base;
  if (b0 == 0) return;
  BaseOffsetMap prepend;
  prepend.reserve(b0);
  for (int b = 0; b < b0; ++b) prepend.emplace_back(b, 0);
  dropped_lo.push_back(0);
  dropped_hi.push_back(b0 - 1);
  id_pairs.insert(id_pairs.begin(), prepend.begin(), prepend.end());
}

}  // namespace


ReverseMatchPlan planReverseMatch(
    const std::vector<SPoint2>&        base,
    bool                               base_is_closed,
    int                                side,
    double                             dist,
    const std::vector<OffsetPolygon>&  polygons) {
  (void)side;
  (void)dist;

  ReverseMatchPlan out;
  out.closed = base_is_closed;

  if (base.size() < 2 || polygons.empty()) return out;

  // Primary picking dispatches on closed/open. Closed inputs select by
  // |signed area| (with centroid tie-break); open inputs select by
  // base_seg span (area ≈ 0 for one-sided open polylines).
  const int primary_idx = base_is_closed
                            ? pickPrimary(polygons, base)
                            : pickPrimaryOpen(polygons, base);
  if (primary_idx < 0) return out;
  const OffsetPolygon& primary = polygons[primary_idx];
  const std::size_t min_pts = base_is_closed ? 3u : 2u;
  if (primary.points.size() < min_pts) return out;

  // Stage F multi-branch diagnostics — closed only. For open inputs
  // the polygon area is ~0 so areas would carry no signal; the open
  // counterpart "how many runs were dropped" is implicitly visible via
  // `polygons.size() - 1` to the caller.
  if (base_is_closed) {
    out.primary_polygon_area = std::fabs(signedArea(primary.points));
    if (polygons.size() > 1) {
      std::vector<double> dropped;
      dropped.reserve(polygons.size() - 1);
      for (int i = 0; i < static_cast<int>(polygons.size()); ++i) {
        if (i == primary_idx) continue;
        dropped.push_back(std::fabs(signedArea(polygons[i].points)));
      }
      std::sort(dropped.begin(), dropped.end(), std::greater<double>());
      out.dropped_polygon_areas = std::move(dropped);
    }
  }

  const int n_base_distinct = static_cast<int>(base.size());

  std::vector<int> cand(primary.points.size(), -1);
  for (std::size_t k = 0; k < primary.points.size(); ++k) {
    cand[k] = attributeOne(primary.sources[k], n_base_distinct,
                           base_is_closed);
  }
  forwardFill(cand);

  bool any_valid = false;
  for (int v : cand) if (v >= 0) { any_valid = true; break; }
  if (!any_valid) return out;

  std::vector<SPoint2> points_rot = primary.points;
  // Mirror the points array with the per-vertex origin tag. Length-
  // pad with `false` if upstream forgot to populate it (defensive —
  // a missing tag is conservatively reported as "raw Clipper2").
  std::vector<bool> resampled_rot = primary.resampled;
  if (resampled_rot.size() != points_rot.size()) {
    resampled_rot.assign(points_rot.size(), false);
  }
  if (base_is_closed) {
    // Rotation only makes sense on a cyclic walk. For open inputs the
    // Phase 1 extractor already orients the run from P_0 toward
    // P_{N-1} (base_seg non-decreasing).
    const int k_start = findRotationStart(cand);
    rotateInPlace(cand,          k_start);
    rotateInPlace(points_rot,    k_start);
    rotateInPlace(resampled_rot, k_start);
  }

  const std::vector<int> walked =
      unrollMonotone(cand, n_base_distinct, base_is_closed);

  buildIdPairs(walked, n_base_distinct, base_is_closed,
               out.id_pairs,
               out.dropped_base_ranges_lo,
               out.dropped_base_ranges_hi);

  // Anchor the staircase at base index 0 if the first attributed index
  // wasn't already 0 (prepends forward-fill pairs). Applies to both
  // closed and open inputs.
  anchorAtZero(out.id_pairs, out.dropped_base_ranges_lo,
               out.dropped_base_ranges_hi);

  out.offset_points    = std::move(points_rot);
  out.offset_resampled = std::move(resampled_rot);
  // Debug overlay: carry the pre-resample raw vertex snapshot through.
  // No rotation — it is rendered as an unordered scatter.
  out.pre_resample_raw_points = primary.pre_resample_points;

  std::string err;
  if (!staircaseValid(out.id_pairs, &err)) return out;

  out.ok = true;
  return out;
}

// ===========================================================================
// planReverseMatchByNearest — alternative point-to-point pairing.
// See header for the algorithm description.
// ===========================================================================

namespace {

inline double distSq(const SPoint2& a, const SPoint2& b) {
  const double dx = a.x() - b.x();
  const double dy = a.y() - b.y();
  return dx * dx + dy * dy;
}

// Foot-of-perpendicular distance from query point `q` to the base
// polyline. Mirrors `attributeSource`'s acceptance gate (1.5 * dist)
// without depending on the offset_clipper2.cpp TU.
double footDistToBase(const SPoint2& q,
                      const std::vector<SPoint2>& base,
                      bool closed) {
  const int n_v = static_cast<int>(base.size());
  if (n_v < 2) return std::numeric_limits<double>::infinity();
  const int n_s = closed ? n_v : (n_v - 1);
  double best = std::numeric_limits<double>::infinity();
  for (int i = 0; i < n_s; ++i) {
    const int j  = (closed && i == n_v - 1) ? 0 : i + 1;
    const auto& a = base[i];
    const auto& b = base[j];
    const double dx = b.x() - a.x();
    const double dy = b.y() - a.y();
    const double len2 = dx * dx + dy * dy;
    double u;
    if (len2 == 0.0) {
      u = 0.0;
    } else {
      u = ((q.x() - a.x()) * dx + (q.y() - a.y()) * dy) / len2;
      if (u < 0.0) u = 0.0;
      if (u > 1.0) u = 1.0;
    }
    const double fx  = a.x() + u * dx;
    const double fy  = a.y() + u * dy;
    const double rdx = q.x() - fx;
    const double rdy = q.y() - fy;
    const double d   = std::sqrt(rdx * rdx + rdy * rdy);
    if (d < best) best = d;
  }
  return best;
}

}  // namespace

ReverseMatchPlan planReverseMatchByNearest(
    const std::vector<SPoint2>&        base,
    bool                               base_is_closed,
    int                                side,
    double                             dist,
    const std::vector<OffsetPolygon>&  polygons) {
  (void)side;

  ReverseMatchPlan out;
  out.closed = base_is_closed;

  if (base.size() < 2 || polygons.empty() || !(dist > 0.0)) return out;

  const int primary_idx = base_is_closed
                            ? pickPrimary(polygons, base)
                            : pickPrimaryOpen(polygons, base);
  if (primary_idx < 0) return out;
  const OffsetPolygon& primary = polygons[primary_idx];

  // Prefer the pre-resample raw vertex sequence when available — that
  // is the whole point of this variant. Falls back to `points` for
  // closed inputs (which never resample) and for open runs that
  // didn't trigger resample.
  const std::vector<SPoint2>& src_seq =
      primary.pre_resample_points.empty() ? primary.points
                                          : primary.pre_resample_points;

  const int M = static_cast<int>(src_seq.size());
  const int min_pts = base_is_closed ? 3 : 2;
  if (M < min_pts) return out;

  // Stage F multi-branch diagnostics — closed only, mirrors planReverseMatch.
  if (base_is_closed) {
    out.primary_polygon_area = std::fabs(signedArea(primary.points));
    if (polygons.size() > 1) {
      std::vector<double> dropped;
      dropped.reserve(polygons.size() - 1);
      for (int i = 0; i < static_cast<int>(polygons.size()); ++i) {
        if (i == primary_idx) continue;
        dropped.push_back(std::fabs(signedArea(polygons[i].points)));
      }
      std::sort(dropped.begin(), dropped.end(), std::greater<double>());
      out.dropped_polygon_areas = std::move(dropped);
    }
  }

  // ---------------------------------------------------------------------
  // Step 1: per-offset validity (foot-of-perpendicular gate).
  // ---------------------------------------------------------------------
  const double radius = 1.5 * dist;
  std::vector<bool> src_valid(M, true);
  int n_valid = 0;
  for (int m = 0; m < M; ++m) {
    if (footDistToBase(src_seq[m], base, base_is_closed) > radius) {
      src_valid[m] = false;
    } else {
      ++n_valid;
    }
  }
  if (n_valid == 0) return out;

  // ---------------------------------------------------------------------
  // Step 2: seed. `j_seed` is the offset index (in src_seq's original
  // numbering) whose point is closest to base[0].
  // ---------------------------------------------------------------------
  int    j_seed = -1;
  double d_seed = std::numeric_limits<double>::infinity();
  for (int m = 0; m < M; ++m) {
    if (!src_valid[m]) continue;
    const double d = distSq(base[0], src_seq[m]);
    if (d < d_seed) { d_seed = d; j_seed = m; }
  }
  if (j_seed < 0) return out;

  // ---------------------------------------------------------------------
  // Step 3: rotate for closed inputs so off[0] == src_seq[j_seed].
  // For open inputs leave the polyline as-is; the seed is in src_seq's
  // native frame.
  // ---------------------------------------------------------------------
  std::vector<SPoint2> off(M);
  std::vector<bool>    off_valid(M);
  if (base_is_closed) {
    for (int m = 0; m < M; ++m) {
      const int src_m = (j_seed + m) % M;
      off[m]       = src_seq[src_m];
      off_valid[m] = src_valid[src_m];
    }
  } else {
    off       = src_seq;
    off_valid = src_valid;
  }
  const int j_seed_rot = base_is_closed ? 0 : j_seed;

  // ---------------------------------------------------------------------
  // Step 4: forward pass. For each base[i], walk j forward (unrolled
  // for closed) starting from the previous pairing. Stop when the
  // point-to-point distance starts increasing. Pair base[i] with
  // the j of minimum distance.
  // ---------------------------------------------------------------------
  const int N = static_cast<int>(base.size());
  std::vector<int> pair_for_base(N, 0);  // unrolled offset index per base
  pair_for_base[0] = j_seed_rot;

  for (int i = 1; i < N; ++i) {
    const int j_last = pair_for_base[i - 1];
    int    j_min = j_last;
    double d_min = distSq(base[i], off[j_last % M]);

    // For closed we may walk up to a full revolution; for open we
    // stop at the polyline end.
    const int j_limit = base_is_closed ? (j_last + M) : (M - 1);
    for (int j = j_last + 1; j <= j_limit; ++j) {
      const int jm = base_is_closed ? (j % M) : j;
      if (!off_valid[jm]) continue;  // join-point: don't stop, just skip
      const double d = distSq(base[i], off[jm]);
      if (d < d_min) {
        d_min = d;
        j_min = j;
      } else if (d > d_min) {
        break;  // distance has crossed the local min — stop walking
      }
      // d == d_min: tied; keep walking. Rare in practice.
    }
    pair_for_base[i] = j_min;
  }

  // ---------------------------------------------------------------------
  // Step 5: build walked_base[m] for m in [0, M-1], filling gaps via
  // the reverse pass and tail / pre-seed anchors.
  // ---------------------------------------------------------------------
  std::vector<int> walked(M, -1);

  // (a) Direct assignments from the forward pass.
  for (int i = 0; i < N; ++i) {
    const int jm = pair_for_base[i] % M;
    walked[jm] = i;  // last write wins if two base i's hit the same jm
                     // (only possible at the closed wrap, which we
                     // overwrite intentionally — the wrap pair (N, M)
                     // is added by buildIdPairs at the end).
  }

  // (b) Reverse pass: gaps between consecutive forward-pass slots.
  // For each skipped offset, pair with whichever of {i-1, i} is
  // geometrically nearer. Once we switch from i-1 to i within a single
  // gap, we don't switch back.
  for (int i = 1; i < N; ++i) {
    const int j_prev = pair_for_base[i - 1];
    const int j_curr = pair_for_base[i];
    if (j_curr <= j_prev + 1) continue;  // no gap

    bool switched = false;
    for (int j = j_prev + 1; j < j_curr; ++j) {
      const int jm = base_is_closed ? (j % M) : j;
      if (walked[jm] >= 0) continue;  // already assigned (shouldn't happen here)
      if (!off_valid[jm]) {
        // Join-point: inherit the more-recent assignment.
        walked[jm] = switched ? i : (i - 1);
        continue;
      }
      const double d_prev = distSq(base[i - 1], off[jm]);
      const double d_curr = distSq(base[i],     off[jm]);
      if (!switched && d_prev <= d_curr) {
        walked[jm] = i - 1;
      } else {
        walked[jm] = i;
        switched = true;
      }
    }
  }

  // (c) Pre-seed anchor (open only). For closed, j_seed_rot == 0 so
  // there are no pre-seed slots.
  if (!base_is_closed) {
    for (int m = 0; m < j_seed_rot; ++m) {
      if (walked[m] < 0) walked[m] = 0;
    }
  }

  // (d) Tail fill.
  if (base_is_closed) {
    // Closed: any unassigned slot is in the wrap region (between
    // pair_for_base[N-1] and the cyclic return to seed). They all
    // pair with base[N-1] in the walked array; buildIdPairs appends
    // the wrap pair (N, M) at the end on top of this.
    for (int m = 0; m < M; ++m) {
      if (walked[m] < 0) walked[m] = N - 1;
    }
  } else {
    // Open: slots after pair_for_base[N-1] pair with base[N-1]
    // (tail forward-fill of the staircase on the base side).
    const int j_tail = pair_for_base[N - 1];
    for (int m = j_tail + 1; m < M; ++m) {
      if (walked[m] < 0) walked[m] = N - 1;
    }
  }

  // Sanity: every slot must be assigned now (defensive — should hold
  // by construction).
  for (int m = 0; m < M; ++m) {
    if (walked[m] < 0) walked[m] = 0;
  }

  // Defensive monotonization: a degenerate offset polyline can in
  // principle produce a non-monotone walked sequence (e.g. when a
  // join-point lies "out of order"). Force non-decreasing so
  // buildIdPairs sees a clean staircase input.
  for (int m = 1; m < M; ++m) {
    if (walked[m] < walked[m - 1]) walked[m] = walked[m - 1];
  }

  // ---------------------------------------------------------------------
  // Step 6: write the offset polyline + delegate id_pairs construction
  // to the same helpers as planReverseMatch.
  // ---------------------------------------------------------------------
  out.offset_points    = std::move(off);
  out.offset_resampled.assign(out.offset_points.size(), false);
  // Carry pre-resample raw points through for debug overlays. Since
  // the source sequence already IS the raw vertices, the snapshot is
  // an empty vector here — debug rendering can drop the overlay
  // for this variant.
  out.pre_resample_raw_points.clear();

  buildIdPairs(walked, N, base_is_closed,
               out.id_pairs,
               out.dropped_base_ranges_lo,
               out.dropped_base_ranges_hi);

  anchorAtZero(out.id_pairs,
               out.dropped_base_ranges_lo,
               out.dropped_base_ranges_hi);

  std::string err;
  if (!staircaseValid(out.id_pairs, &err)) return out;

  out.ok = true;
  return out;
}

// ===========================================================================
// rebuildBaseOffsetMapFromGeometry — derive staircase from current geometry.
// See header for full contract. Implementation strategy: synthesize a
// single-polygon `OffsetPolygon` from the offset polyline (computing
// per-vertex `OffsetVertexSource` via foot-of-perpendicular projection
// onto the base, with the same 1.5·dist acceptance gate as
// `attributeSource` in offset_clipper2.cpp), then hand it to
// `planReverseMatchByNearest`. This reuses the existing primary-pick
// and id_pairs building helpers and gives parity-by-construction with
// the original Clipper2 → reverse-match path when the offset polyline
// is unmodified.
// ===========================================================================

namespace {

OffsetVertexSource attributeFootToBase(const SPoint2& q,
                                       const std::vector<SPoint2>& base,
                                       bool base_is_closed,
                                       double source_radius) {
  const int n_v = static_cast<int>(base.size());
  if (n_v < 2) return {-1, 0.0};
  const int n_s = base_is_closed ? n_v : (n_v - 1);
  OffsetVertexSource best{-1, 0.0};
  double best_d = std::numeric_limits<double>::infinity();
  for (int i = 0; i < n_s; ++i) {
    const int j  = (base_is_closed && i == n_v - 1) ? 0 : i + 1;
    const auto& a = base[i];
    const auto& b = base[j];
    const double dx = b.x() - a.x();
    const double dy = b.y() - a.y();
    const double len2 = dx * dx + dy * dy;
    double u;
    if (len2 == 0.0) {
      u = 0.0;
    } else {
      u = ((q.x() - a.x()) * dx + (q.y() - a.y()) * dy) / len2;
      if (u < 0.0) u = 0.0;
      if (u > 1.0) u = 1.0;
    }
    const double fx  = a.x() + u * dx;
    const double fy  = a.y() + u * dy;
    const double rdx = q.x() - fx;
    const double rdy = q.y() - fy;
    const double d   = std::sqrt(rdx * rdx + rdy * rdy);
    if (d < best_d) {
      best_d = d;
      best   = {i, u};
    }
  }
  if (best_d > source_radius) return {-1, 0.0};
  return best;
}

}  // namespace

ReverseMatchPlan rebuildBaseOffsetMapFromGeometry(
    const std::vector<SPoint2>&  base,
    const std::vector<SPoint2>&  offset,
    bool                         base_is_closed,
    int                          side,
    double                       dist,
    bool                         use_nearest_pairing) {
  ReverseMatchPlan empty;
  empty.closed = base_is_closed;

  if (base.size() < 2 || offset.size() < 2 || !(dist > 0.0)) return empty;

  // Drop a trailing-duplicate offset vertex on closed inputs (the PreVABS
  // Baseline convention is to repeat the first vertex at the end). For
  // open inputs the front/back are expected to be distinct end-points
  // already; leave the sequence untouched.
  std::vector<SPoint2> off = offset;
  if (base_is_closed && off.size() >= 2) {
    const auto& f = off.front();
    const auto& b = off.back();
    if (f.x() == b.x() && f.y() == b.y()) {
      off.pop_back();
    }
  }
  if (off.size() < 2) return empty;

  // Synthesize a single-polygon OffsetPolygon from the offset polyline.
  // `points` holds the raw vertices; `sources` is reconstructed by
  // projecting each offset vertex onto the base polyline with the same
  // 1.5·dist acceptance gate `attributeSource` uses. `resampled` is
  // all-false (we have no resample provenance for an externally-supplied
  // polyline) and `pre_resample_points` is empty so
  // planReverseMatchByNearest walks `points` directly.
  OffsetPolygon poly;
  poly.points    = off;
  poly.is_closed = base_is_closed;
  poly.resampled.assign(off.size(), false);
  poly.sources.reserve(off.size());
  const double radius = 1.5 * dist;
  for (const auto& p : off) {
    poly.sources.push_back(
        attributeFootToBase(p, base, base_is_closed, radius));
  }

  const std::vector<OffsetPolygon> polygons = {std::move(poly)};
  return use_nearest_pairing
      ? planReverseMatchByNearest(base, base_is_closed, side, dist, polygons)
      : planReverseMatch(base, base_is_closed, side, dist, polygons);
}

}  // namespace geo
}  // namespace prevabs
