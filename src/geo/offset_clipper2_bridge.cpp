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

  if (closed) {
    int last_b = walked.back();
    for (int b = last_b + 1; b < n_base_distinct; ++b) {
      emit(id_pairs, b, m - 1);
    }
    if (last_b < n_base_distinct - 1) {
      dropped_lo.push_back(last_b + 1);
      dropped_hi.push_back(n_base_distinct - 1);
    }
    emit(id_pairs, n_base_distinct, m);
  }
}

// Force a closed loop's id_pairs to start at base index 0. Prepends
// (0,0)..(b0-1,0) and records the prepended range as dropped.
void anchorClosed(BaseOffsetMap& id_pairs,
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

  const int primary_idx = pickPrimary(polygons, base);
  if (primary_idx < 0) return out;
  const OffsetPolygon& primary = polygons[primary_idx];
  if (primary.points.size() < 3) return out;

  // Stage F multi-branch diagnostics: record areas (largest-first
  // semantics; we already know primary_idx is the chosen one). The
  // PDCELVertex adapter consumes this for the user-facing warning.
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
  if (base_is_closed) {
    const int k_start = findRotationStart(cand);
    rotateInPlace(cand,       k_start);
    rotateInPlace(points_rot, k_start);
  }

  const std::vector<int> walked =
      unrollMonotone(cand, n_base_distinct, base_is_closed);

  buildIdPairs(walked, n_base_distinct, base_is_closed,
               out.id_pairs,
               out.dropped_base_ranges_lo,
               out.dropped_base_ranges_hi);

  if (base_is_closed) {
    anchorClosed(out.id_pairs, out.dropped_base_ranges_lo,
                 out.dropped_base_ranges_hi);
  }

  out.offset_points = std::move(points_rot);

  std::string err;
  if (!staircaseValid(out.id_pairs, &err)) return out;

  out.ok = true;
  return out;
}

}  // namespace geo
}  // namespace prevabs
