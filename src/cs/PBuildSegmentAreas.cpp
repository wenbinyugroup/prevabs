#include "PSegment.hpp"

#include "CurveFrameLookup.hpp"
#include "Material.hpp"
#include "PDCELFace.hpp"
#include "PDCELHalfEdgeLoop.hpp"
#include "PDCELUtils.hpp"
#include "PDCELVertex.hpp"
#include "PModel.hpp"
#include "PModelIO.hpp"
#include "debug/baseOffsetMapJson.hpp"
#include "debug/baseOffsetMapSvg.hpp"
#include "debug/segmentBuildDump.hpp"
#include "geo.hpp"
#include "globalConstants.hpp"
#include "globalVariables.hpp"
#include "offset_clipper2.hpp"
#include "overloadOperator.hpp"
#include "utilities.hpp"
#include "plog.hpp"

#include "geo_types.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <list>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace dcel;  // phase 0: DCEL types moved to namespace dcel

// Whether the layered per-layer-offset build path is active. Primary knob is
// the XML config `config.layered_offset` (<general>/<layered_offset>, default
// ON). The env var PREVABS_LAYERED_OFFSET still overrides if set (quick CLI
// toggle / tests). Global so offsetCurveBase (PSegment.cpp) can decide whether
// to keep the total-thickness shell raw while per-layer curves are resampled;
// declared in globalVariables.hpp.
bool useLayeredOffset() {
  const char* raw = std::getenv("PREVABS_LAYERED_OFFSET");
  if (raw && *raw) {
    std::string s(raw);
    for (auto& c : s) c = static_cast<char>(std::tolower(c));
    return s == "1" || s == "true" || s == "on" || s == "yes";
  }
  return config.layered_offset;
}

// Whether to skip area construction over Clipper2 "dropped" base ranges.
// Primary knob is the XML config `config.skip_dropped_areas`
// (<general>/<skip_dropped_areas>, default OFF). The env var
// PREVABS_SKIP_DROPPED_AREAS overrides if set (quick CLI toggle / tests).
bool useSkipDroppedAreas() {
  const char* raw = std::getenv("PREVABS_SKIP_DROPPED_AREAS");
  if (raw && *raw) {
    std::string s(raw);
    for (auto& c : s) c = static_cast<char>(std::tolower(c));
    return s == "1" || s == "true" || s == "on" || s == "yes";
  }
  return config.skip_dropped_areas;
}

// Per-layer offset boundary generation method ("dir" or "seq"). Primary knob is
// the XML config `config.layer_offset_method`
// (<general>/<layer_offset_method>, default "dir"). The env var
// PREVABS_LAYER_OFFSET_METHOD overrides if set (quick CLI toggle / tests).
// Unknown values fall through to the config value. Global so the layered build
// loop can pick base-cumulative (dir) vs previous-incremental (seq) offsetting.
std::string layerOffsetMethod() {
  const char* raw = std::getenv("PREVABS_LAYER_OFFSET_METHOD");
  if (raw && *raw) {
    std::string s(raw);
    for (auto& c : s) c = static_cast<char>(std::tolower(c));
    if (s == "dir" || s == "seq") return s;
  }
  return config.layer_offset_method;
}

namespace {

// Foot-of-perpendicular distance from q to a polyline (open or closed).
double footDistToPolylineSeg(const SPoint2& q,
                             const std::vector<SPoint2>& poly, bool closed) {
  const int n = static_cast<int>(poly.size());
  if (n < 2) return std::numeric_limits<double>::infinity();
  const int ns = closed ? n : n - 1;
  double best = std::numeric_limits<double>::infinity();
  for (int i = 0; i < ns; ++i) {
    const SPoint2& a = poly[i];
    const SPoint2& b = poly[(i + 1) % n];
    const double dx = b.x() - a.x(), dy = b.y() - a.y();
    const double len2 = dx * dx + dy * dy;
    double u = 0.0;
    if (len2 > 0.0) {
      u = ((q.x() - a.x()) * dx + (q.y() - a.y()) * dy) / len2;
      u = std::max(0.0, std::min(1.0, u));
    }
    const double fx = a.x() + u * dx, fy = a.y() + u * dy;
    best = std::min(best, std::hypot(q.x() - fx, q.y() - fy));
  }
  return best;
}

// Route-i offset curve of `base` at `dist` — the primary run. Closed →
// largest |signed area| polygon; open → most-vertices side run. Mirrors the
// Phase-0 prototype (test/integration/t0_offset_clipper2/phase0_layered_v.cpp).
std::vector<SPoint2> offsetCurveSeg(const std::vector<SPoint2>& base,
                                    bool closed, int clipper_side,
                                    double dist) {
  auto polys =
      prevabs::geo::offsetWithClipper2(base, closed, clipper_side, dist);
  const prevabs::geo::OffsetPolygon* best = nullptr;
  double best_metric = -1.0;
  for (const auto& p : polys) {
    if (p.points.size() < 2) continue;
    double metric = static_cast<double>(p.points.size());
    if (closed) {
      double a = 0.0;
      const std::size_t m = p.points.size();
      for (std::size_t i = 0; i < m; ++i) {
        const auto& p0 = p.points[i];
        const auto& p1 = p.points[(i + 1) % m];
        a += p0.x() * p1.y() - p1.x() * p0.y();
      }
      metric = std::fabs(0.5 * a);
    }
    if (metric > best_metric) { best_metric = metric; best = &p; }
  }
  return best ? best->points : std::vector<SPoint2>{};
}

struct LayeredCurve {
  std::vector<PDCELVertex *> vertices;
  std::vector<SPoint2> points;
  bool owns_vertices = false;
};

void deleteUnregisteredLayeredCurve(LayeredCurve& curve) {
  if (!curve.owns_vertices) return;
  for (auto* v : curve.vertices) {
    if (v != nullptr && !v->isRegistered()) {
      delete v;
    }
  }
  curve.vertices.clear();
  curve.points.clear();
}

double signedArea2D(const std::vector<SPoint2>& pts) {
  if (pts.size() < 3) return 0.0;
  double a = 0.0;
  for (std::size_t i = 0; i < pts.size(); ++i) {
    const auto& p = pts[i];
    const auto& q = pts[(i + 1) % pts.size()];
    a += p.x() * q.y() - q.x() * p.y();
  }
  return 0.5 * a;
}

// Align a CLOSED layer ring to a reference ring so their seams correspond.
// Clipper2 does not preserve either the winding or the starting (seam) vertex
// of an offset polygon, but the closed staircase (rebuildBaseOffsetMapFrom
// Geometry) pins index 0 of both curves as the seam and walks in step — so a
// misaligned seam or opposite winding yields a garbage staircase. Match the
// reference winding (by signed-area sign) then rotate the ring so its vertex 0
// is the one nearest the reference seam. Aligning every ring to the base seam
// keeps consecutive rings mutually aligned.
void alignClosedRingToReference(LayeredCurve& c,
                                const std::vector<SPoint2>& ref) {
  if (c.points.size() < 3 || ref.size() < 3) return;
  if (signedArea2D(c.points) * signedArea2D(ref) < 0.0) {
    std::reverse(c.points.begin(), c.points.end());
    std::reverse(c.vertices.begin(), c.vertices.end());
  }
  const SPoint2& seam = ref.front();
  std::size_t best = 0;
  double best_d2 = std::numeric_limits<double>::infinity();
  for (std::size_t i = 0; i < c.points.size(); ++i) {
    const double dx = c.points[i].x() - seam.x();
    const double dy = c.points[i].y() - seam.y();
    const double d2 = dx * dx + dy * dy;
    if (d2 < best_d2) { best_d2 = d2; best = i; }
  }
  if (best == 0) return;
  std::rotate(c.points.begin(), c.points.begin() + best, c.points.end());
  std::rotate(c.vertices.begin(), c.vertices.begin() + best, c.vertices.end());
}

// Remove consecutive near-coincident vertices from a CLOSED layer ring,
// including the wrap-around pair (last vs first). Clipper2 works on an integer
// grid (~1e-8 resolution) and, offsetting a sharp corner inward, can emit two
// vertices spaced only ~1e-8 apart — a zero-length ring edge. The closed
// staircase (rebuildBaseOffsetMapFromGeometry) then pairs a connector onto each
// of those coincident vertices, so consecutive connectors are geometrically
// identical and splitFaceByPolyline produces a zero-area sliver → the degenerate
// -split self-check aborts the whole closed tile (issue-20260702). The open path
// never hits this because makeArcResampledOuterCurve re-spaces the final outer
// curve with a strict min separation; closed intermediate rings had no such
// pass. Dropping vertices below GEO_COINCIDENCE_TOL only removes edges that are
// already degenerate, so it changes no real geometry. If the ring owns its
// vertices, unregistered dropped ones are deleted (still pre-DCEL-embedding).
void dedupClosedRing(LayeredCurve& c) {
  const std::size_t n = c.points.size();
  if (n < 2) return;
  std::vector<PDCELVertex*> kept_v;
  std::vector<SPoint2> kept_p;
  kept_v.reserve(n);
  kept_p.reserve(n);
  std::vector<PDCELVertex*> dropped;
  for (std::size_t i = 0; i < n; ++i) {
    // Compare against the previously kept point; also fold the last vertex into
    // the first (wrap) so the seam edge cannot be zero-length either.
    const bool coincident_prev =
        !kept_p.empty()
        && c.points[i].distance(kept_p.back()) < GEO_COINCIDENCE_TOL;
    const bool coincident_wrap =
        (i == n - 1) && !kept_p.empty()
        && c.points[i].distance(kept_p.front()) < GEO_COINCIDENCE_TOL;
    if (coincident_prev || coincident_wrap) {
      dropped.push_back(c.vertices[i]);
      continue;
    }
    kept_v.push_back(c.vertices[i]);
    kept_p.push_back(c.points[i]);
  }
  if (kept_v.size() == c.vertices.size()) return;  // nothing coincident
  if (c.owns_vertices) {
    for (auto* v : dropped) {
      if (v != nullptr && !v->isRegistered()) delete v;
    }
  }
  c.vertices = std::move(kept_v);
  c.points = std::move(kept_p);
}

const prevabs::geo::OffsetPolygon* pickLayeredPrimary(
    const std::vector<prevabs::geo::OffsetPolygon>& polys, bool closed) {
  const prevabs::geo::OffsetPolygon* best = nullptr;
  double best_metric = -1.0;
  for (const auto& p : polys) {
    if (p.points.size() < 2) continue;
    double metric = static_cast<double>(p.points.size());
    if (closed) {
      metric = std::fabs(signedArea2D(p.points));
    } else {
      int lo = std::numeric_limits<int>::max();
      int hi = std::numeric_limits<int>::min();
      for (const auto& s : p.sources) {
        if (s.base_seg < 0) continue;
        lo = std::min(lo, s.base_seg);
        hi = std::max(hi, s.base_seg);
      }
      if (lo <= hi) {
        metric = static_cast<double>(hi - lo + 1);
      }
    }
    if (metric > best_metric) {
      best_metric = metric;
      best = &p;
    }
  }
  return best;
}

LayeredCurve makeExistingCurve(
    const std::vector<PDCELVertex *>& source, bool closed) {
  LayeredCurve curve;
  curve.owns_vertices = false;
  curve.vertices = source;
  if (closed && curve.vertices.size() >= 2
      && curve.vertices.front() == curve.vertices.back()) {
    curve.vertices.pop_back();
  }
  curve.points.reserve(curve.vertices.size());
  for (auto* v : curve.vertices) {
    curve.points.emplace_back(v->point2()[0], v->point2()[1]);
  }
  return curve;
}

LayeredCurve makeLayerOffsetCurve(
    const std::vector<SPoint2>& base, bool closed, int clipper_side,
    double dist) {
  prevabs::debug::SegmentTraceScope _trace_scope(
      "makeLayerOffsetCurve (dist=" + std::to_string(dist) + ")");
  LayeredCurve curve;
  auto polys = prevabs::geo::offsetWithClipper2(
      base, closed, clipper_side, dist,
      prevabs::geo::JoinTypeChoice::Miter, 2.0,
      /*resample_open*/ true,
      /*experimental_open_miter_resample*/ false,
      prevabs::geo::openResampleModeFromString(
          config.offset_resample_mode));
  const auto* primary = pickLayeredPrimary(polys, closed);
  if (primary == nullptr) return curve;

  curve.points = primary->points;
  curve.vertices.reserve(curve.points.size());
  for (const auto& p : curve.points) {
    curve.vertices.push_back(new PDCELVertex(0.0, p.x(), p.y()));
  }
  curve.owns_vertices = true;
  return curve;
}

// Cumulative arc length of an open polyline (cum[0]==0, cum.back()==total).
std::vector<double> cumulativeArcLength(const std::vector<SPoint2>& p) {
  std::vector<double> cum(p.size(), 0.0);
  for (std::size_t i = 1; i < p.size(); ++i) {
    cum[i] = cum[i - 1] + p[i - 1].distance(p[i]);
  }
  return cum;
}

// Point on open polyline `poly` (with precomputed cumulative arc `cum`) at
// cumulative arc length `target` (clamped to the polyline ends).
SPoint2 pointAtArcLength(const std::vector<SPoint2>& poly,
                         const std::vector<double>& cum, double target) {
  if (target <= 0.0) return poly.front();
  if (target >= cum.back()) return poly.back();
  std::size_t i = 1;
  while (i < cum.size() && cum[i] < target) ++i;     // first cum[i] >= target
  const std::size_t a = i - 1;
  const double seg = cum[i] - cum[a];
  const double t = (seg > 0.0) ? (target - cum[a]) / seg : 0.0;
  return SPoint2(poly[a].x() + t * (poly[i].x() - poly[a].x()),
                 poly[a].y() + t * (poly[i].y() - poly[a].y()));
}

// Cumulative-arc position of the foot of perpendicular from `q` onto open
// polyline `poly` (with cumulative arc `cum`) — i.e. the arc length at the
// closest point on the polyline. Used to resample the shell by projecting each
// inner vertex onto it (connectors then follow the layer-thickness direction).
double projectArcOntoPolyline(const SPoint2& q,
                              const std::vector<SPoint2>& poly,
                              const std::vector<double>& cum) {
  double best_d2 = std::numeric_limits<double>::infinity();
  double best_arc = 0.0;
  for (std::size_t i = 0; i + 1 < poly.size(); ++i) {
    const double dx = poly[i + 1].x() - poly[i].x();
    const double dy = poly[i + 1].y() - poly[i].y();
    const double len2 = dx * dx + dy * dy;
    double u = 0.0;
    if (len2 > 0.0) {
      u = ((q.x() - poly[i].x()) * dx + (q.y() - poly[i].y()) * dy) / len2;
      u = std::max(0.0, std::min(1.0, u));
    }
    const double fx = poly[i].x() + u * dx, fy = poly[i].y() + u * dy;
    const double d2 = (q.x() - fx) * (q.x() - fx) + (q.y() - fy) * (q.y() - fy);
    if (d2 < best_d2) {
      best_d2 = d2;
      best_arc = cum[i] + u * std::sqrt(len2);
    }
  }
  return best_arc;
}

// note-build-laminate-segment.md §2.1 / issue-20260628 §8.3: the FINAL layer's
// outer boundary is the segment offset (shell) itself — it is NOT re-offset.
// Its vertex distribution was fixed for the whole segment, independent of this
// layer's inner curve (curves[last]); mapping the two raw point sets makes the
// sparser side (the shell, near a convex head) STALL, fanning several
// connectors off one shell vertex into zero-area sliver cells that crash the
// split. Fix: resample the shell so it carries exactly ONE point per inner
// vertex, placed at the matching cumulative-arc-length fraction along the
// shell. This yields a strictly 1:1, monotone, outer-stall-free correspondence
// (every cell a quad). Endpoints REUSE the existing shell corner vertices (the
// band's begin/end caps); interior points are NEW vertices lying ON the shell
// polyline — splitFaceByPolyline subdivides the shell edges to embed them, so
// the exact shell geometry is preserved (no re-offset, no offset error). The
// caller pairs these 1:1 via a trivial {i,i} BaseOffsetMap.
LayeredCurve makeArcResampledOuterCurve(
    const LayeredCurve& inner, const LayeredCurve& shell) {
  LayeredCurve out;
  const std::size_t n = inner.points.size();
  if (n < 2 || shell.points.size() < 2) return out;  // empty -> caller falls back
  const std::vector<double> cum_in = cumulativeArcLength(inner.points);
  const std::vector<double> cum_sh = cumulativeArcLength(shell.points);
  const double L_in = cum_in.back(), L_sh = cum_sh.back();
  if (!(L_in > 0.0) || !(L_sh > 0.0)) return out;
  // Strictly-increasing minimum spacing between consecutive outer arc
  // positions: keeps the staircase 1:1 (no two samples collapse) and the cells
  // from inverting. Tiny vs the layer/shell scale, yet kept clearly above the
  // export-time vertex-merge tolerance (PModelBuildGmsh EXPORT_MERGE_TOL ~2e-6)
  // so an intentional short resample edge is never mistaken for a coincident
  // pair and merged away.
  const double min_sep = std::max(GEO_RESAMPLE_MIN_SEP, L_sh * 1e-5);
  out.points.reserve(n);
  out.vertices.reserve(n);
  out.owns_vertices = true;
  double prev_arc = 0.0;  // arc position of the previously placed outer vertex
  for (std::size_t i = 0; i < n; ++i) {
    if (i == 0) {
      out.points.push_back(shell.points.front());
      out.vertices.push_back(shell.vertices.front());   // existing band corner
      prev_arc = 0.0;
    } else if (i == n - 1) {
      out.points.push_back(shell.points.back());
      out.vertices.push_back(shell.vertices.back());    // existing band corner
    } else {
      // Project inner[i] onto the shell (perpendicular foot), then force the
      // arc position strictly forward of the previous one so connectors stay
      // ordered (monotone) and never cross — projection alone can stall/retreat
      // where the shell is locally concave or coarsely sampled.
      double a = projectArcOntoPolyline(inner.points[i], shell.points, cum_sh);
      a = std::max(a, prev_arc + min_sep);
      // Leave room for the remaining interior points before the back cap.
      a = std::min(a, L_sh - static_cast<double>(n - 1 - i) * min_sep);
      a = std::max(a, prev_arc + min_sep);  // re-assert after the ceiling clamp
      const SPoint2 q = pointAtArcLength(shell.points, cum_sh, a);
      out.points.push_back(q);
      out.vertices.push_back(new PDCELVertex(0.0, q.x(), q.y()));
      prev_arc = a;
    }
  }
  return out;
}

// Signed side of point P relative to the directed cap line A->B (sign of the
// 2D cross product). Same sign = same side; 0 = on the line.
double capSide(const SPoint2& A, const SPoint2& B, const SPoint2& P) {
  return (B.x() - A.x()) * (P.y() - A.y())
       - (B.y() - A.y()) * (P.x() - A.x());
}

// note-build-laminate-segment.md §2.1.2: trim/extend an OPEN layer offset curve
// so its two endpoints land exactly on the segment's begin/end bound (cap)
// lines. At this stage the remaining face is bounded by `inner` (the previous
// layer curve) and `shell` (the total-thickness offset), so the begin cap line
// runs inner.head -> shell.head and the end cap line runs inner.tail ->
// shell.tail (caps are straight single edges, free or joined).
//
// The raw miter offset can overshoot a bound (and even fold back into a
// near-180° reversal cusp just outside it). Trim drops every vertex on the
// exterior side of the cap and replaces the end with the curve/cap
// intersection; an undershooting end is extended along its first/last segment
// to meet the cap. The two new cap endpoints lie on the cap edge, so the
// subsequent splitFaceByPolyline canonicalizes them onto it.
//
// The curve owns its vertices: dropped unregistered vertices are deleted and
// the two cap endpoints are freshly created. Throws on a parallel/degenerate
// intersection — bounds are fully resolved by now, so this should not happen
// (fail fast rather than mask a geometry bug).
void trimLayerCurveEndsToCaps(
    LayeredCurve& c, const LayeredCurve& inner, const LayeredCurve& shell,
    double dist, const std::string& segment_name) {
  const std::size_t m = c.points.size();
  if (m < 2 || inner.points.size() < 2 || shell.points.size() < 2) {
    throw std::runtime_error(
        "layered offset[" + segment_name +
        "]: cannot trim degenerate layer curve to segment bounds");
  }
  // Merge tolerance for spurious near-coincident vertices. The raw miter offset
  // and the cap intersection emit FP-noise-level duplicates (~1e-9..1e-8) at
  // sharp corners / open-end caps; left in, they become zero-length connectors
  // that splitFaceByPolyline rejects. Scale to the layer thickness so the
  // threshold sits far below real feature size yet well above FP noise.
  const double merge_tol = std::max(GEO_MERGE_TOL, std::fabs(dist) * 1e-4);

  const SPoint2 beginA = inner.points.front();
  const SPoint2 beginB = shell.points.front();
  const SPoint2 endA = inner.points.back();
  const SPoint2 endB = shell.points.back();

  // Returns the cap-line landing point; `u1_out` reports the parameter of that
  // landing ALONG the curve segment [s, s+1] (can be <0 or >1: line∩line, not
  // segment∩segment), which the reversal-spike guard below uses to decide
  // whether the boundary vertex sits behind or ahead of the landing.
  auto crossCap = [&](std::size_t s, const SPoint2& A, const SPoint2& B,
                      const char* which, double& u1_out) -> SPoint2 {
    double u1, u2;
    if (!calcLineIntersection2D(
            c.points[s], c.points[s + 1], A, B, u1, u2)) {
      throw std::runtime_error(
          "layered offset[" + segment_name + "]: layer curve "
          + which + " segment parallel to its bound");
    }
    (void)u2;
    u1_out = u1;
    return SPoint2(
        c.points[s].x() + u1 * (c.points[s + 1].x() - c.points[s].x()),
        c.points[s].y() + u1 * (c.points[s + 1].y() - c.points[s].y()));
  };

  // Signed perpendicular distance of a point to a cap line (positive on the
  // interior reference side). A vertex is "strictly exterior" when it sits
  // beyond the cap by more than merge_tol; those are the overshoot/reversal
  // vertices to drop. On-cap (|perp| < merge_tol) and interior vertices stay —
  // a straight strip whose endpoints lie exactly on the caps must be kept
  // verbatim.
  const double caplen_b = beginA.distance(beginB);
  const double caplen_e = endA.distance(endB);
  if (caplen_b <= GEO_TOL || caplen_e <= GEO_TOL) {
    throw std::runtime_error(
        "layered offset[" + segment_name + "]: degenerate segment bound");
  }
  auto beginPerp = [&](const SPoint2& p) {
    return capSide(beginA, beginB, p) / caplen_b;
  };
  auto endPerp = [&](const SPoint2& p) {
    return capSide(endA, endB, p) / caplen_e;
  };
  // A merge-tolerance-near endpoint can still be outside PDCEL's stricter
  // GEO_TOL boundary predicate. Snap it exactly to the cap line before split.
  auto snapToCap = [](const SPoint2& p, const SPoint2& A,
                      const SPoint2& B) {
    const double dx = B.x() - A.x();
    const double dy = B.y() - A.y();
    const double len2 = dx * dx + dy * dy;
    const double u = ((p.x() - A.x()) * dx + (p.y() - A.y()) * dy)
                     / len2;
    return SPoint2(A.x() + u * dx, A.y() + u * dy);
  };

  // Land the begin endpoint where the curve first COMMITS to the interior of
  // the begin cap, not on the first non-exterior vertex. A deep open-end offset
  // can leave a fold at the bound — leading vertices that are exterior OR merely
  // on the cap line (perp ≈ 0) but collinear-beyond the inner corner (a butt-cap
  // remnant; see the circle_param_layup_range 0.6-offset head). Snapping the
  // endpoint to that first fold vertex lands it OUTSIDE the cap segment and
  // breaks splitFaceByPolyline. Instead skip the whole non-interior prefix and
  // cross the segment that enters the interior; the fold vertices are dropped.
  const double begin_sign = (beginPerp(c.points.back()) >= 0.0) ? 1.0 : -1.0;
  std::size_t head_in = 0;
  while (head_in < m
         && beginPerp(c.points[head_in]) * begin_sign <= merge_tol) {
    ++head_in;
  }
  std::size_t keep_lo = 0;
  SPoint2 head_pt;
  if (head_in >= m) {
    // No strictly-interior vertex: the curve runs along/against the cap, where
    // an intersection is ill-conditioned. Snap the first non-exterior vertex.
    std::size_t head_keep = 0;
    while (head_keep < m
           && beginPerp(c.points[head_keep]) * begin_sign < -merge_tol) {
      ++head_keep;
    }
    if (head_keep >= m) {
      throw std::runtime_error(
          "layered offset[" + segment_name +
          "]: entire layer curve lies outside the begin bound");
    }
    head_pt = snapToCap(c.points[head_keep], beginA, beginB);
    keep_lo = head_keep;
  } else if (head_in == 0) {
    // Curve starts already interior: extend its first segment back to the cap.
    // Reversal-spike guard (note-build-laminate-segment.md §2.1.2): if the cap
    // landing sits AHEAD of vertex 0 along that segment, keeping vertex 0 would
    // double the rebuilt head back into a near-collinear spike — drop it.
    double head_u1 = 0.0;
    head_pt = crossCap(0, beginA, beginB, "head", head_u1);
    keep_lo =
        (head_u1 * c.points[0].distance(c.points[1]) > merge_tol) ? 1 : 0;
  } else {
    // Cross the segment entering the interior; drop the leading fold prefix.
    double head_u1 = 0.0;
    head_pt = crossCap(head_in - 1, beginA, beginB, "head", head_u1);
    keep_lo = head_in;
  }

  // Tail: symmetric — land where the curve LAST leaves the interior.
  const double end_sign = (endPerp(c.points.front()) >= 0.0) ? 1.0 : -1.0;
  std::size_t tail_in = m;
  while (tail_in > 0
         && endPerp(c.points[tail_in - 1]) * end_sign <= merge_tol) {
    --tail_in;
  }
  std::size_t keep_hi = m - 1;
  SPoint2 tail_pt;
  if (tail_in == 0) {
    std::size_t tail_keep = m;
    while (tail_keep > 0
           && endPerp(c.points[tail_keep - 1]) * end_sign < -merge_tol) {
      --tail_keep;
    }
    if (tail_keep == 0) {
      throw std::runtime_error(
          "layered offset[" + segment_name +
          "]: entire layer curve lies outside the end bound");
    }
    tail_pt = snapToCap(c.points[tail_keep - 1], endA, endB);
    keep_hi = tail_keep - 1;
  } else if (tail_in == m) {
    double tail_u1 = 0.0;
    tail_pt = crossCap(m - 2, endA, endB, "tail", tail_u1);
    keep_hi = ((1.0 - tail_u1) * c.points[m - 2].distance(c.points[m - 1])
                   > merge_tol)
                  ? m - 2
                  : m - 1;
  } else {
    double tail_u1 = 0.0;
    tail_pt = crossCap(tail_in - 1, endA, endB, "tail", tail_u1);
    keep_hi = tail_in - 1;
  }

  // Keep the cap endpoints authoritative and drop interior vertices that are
  // within merge_tol of a cap endpoint or of the previous kept vertex — these
  // spurious near-duplicates (sharp corner / open-end cap) would otherwise
  // become zero-length connectors that splitFaceByPolyline rejects.
  auto near2 = [&](const SPoint2& a, const SPoint2& b) {
    return a.distance(b) < merge_tol;
  };
  std::vector<bool> kept(m, false);
  std::vector<int> keep_idx;
  for (int i = static_cast<int>(keep_lo); i <= static_cast<int>(keep_hi);
       ++i) {
    if (near2(head_pt, c.points[i]) || near2(tail_pt, c.points[i])) continue;
    if (!keep_idx.empty() && near2(c.points[keep_idx.back()], c.points[i]))
      continue;
    keep_idx.push_back(i);
    kept[i] = true;
  }

  // Rebuild: [head_pt] + kept interior + [tail_pt].
  std::vector<PDCELVertex*> new_v;
  std::vector<SPoint2> new_p;
  new_v.push_back(new PDCELVertex(0.0, head_pt.x(), head_pt.y()));
  new_p.push_back(head_pt);
  for (int i : keep_idx) {
    new_v.push_back(c.vertices[i]);
    new_p.push_back(c.points[i]);
  }
  new_v.push_back(new PDCELVertex(0.0, tail_pt.x(), tail_pt.y()));
  new_p.push_back(tail_pt);

  if (c.owns_vertices) {
    for (int i = 0; i < static_cast<int>(m); ++i) {
      if (!kept[i] && c.vertices[i] != nullptr
          && !c.vertices[i]->isRegistered()) {
        delete c.vertices[i];
      }
    }
  }
  c.vertices = std::move(new_v);
  c.points = std::move(new_p);
  c.owns_vertices = true;
}

SVector3 averageLayupVector(
    const LayeredCurve& inner, const LayeredCurve& outer,
    std::size_t i0, std::size_t i1) {
  SVector3 y2(0, 1, 0);
  if (inner.vertices.empty() || outer.vertices.empty()) return y2;
  const SVector3 a(inner.vertices[i0]->point(), outer.vertices[i0]->point());
  const SVector3 b(inner.vertices[i1]->point(), outer.vertices[i1]->point());
  y2 = a + b;
  if (y2.norm() > 0.0) {
    y2.normalize();
  }
  return y2;
}

bool computeFaceCentroid2DForLayered(PDCELFace *face, SPoint2 &out) {
  if (face == nullptr || face->outer() == nullptr) return false;
  double sy = 0.0, sz = 0.0;
  int n = 0;
  PDCELHalfEdge *start = face->outer();
  walkLoopWithLimit(start, [&](PDCELHalfEdge *he) {
    PDCELVertex *v = he->source();
    if (v != nullptr) {
      sy += v->y();
      sz += v->z();
      ++n;
    }
  });
  if (n == 0) return false;
  out = SPoint2(sy / n, sz / n);
  return true;
}

// Number of half-edges on the face outer loop (0 if no outer loop).
int faceOuterEdgeCount(PDCELFace *face) {
  if (face == nullptr || face->outer() == nullptr) return 0;
  int n = 0;
  walkLoopWithLimit(face->outer(), [&](PDCELHalfEdge *) { ++n; });
  return n;
}

// True if the face's outer boundary carries a vertex coincident (within tol)
// with point p (p is in (y,z) like LayeredCurve points).
bool faceBoundaryHasPoint(PDCELFace *face, const SPoint2& p, double tol) {
  if (face == nullptr || face->outer() == nullptr) return false;
  bool found = false;
  walkLoopWithLimit(face->outer(), [&](PDCELHalfEdge *he) {
    PDCELVertex *v = he->source();
    if (v != nullptr && std::fabs(v->y() - p.x()) < tol
        && std::fabs(v->z() - p.y()) < tol) {
      found = true;
    }
  });
  return found;
}

double faceCentroidDistanceToCurveForLayered(
    PDCELFace *face, const LayeredCurve& curve, bool closed) {
  SPoint2 centroid;
  if (!computeFaceCentroid2DForLayered(face, centroid)) {
    return std::numeric_limits<double>::infinity();
  }
  return footDistToPolylineSeg(centroid, curve.points, closed);
}

void applyLayeredFaceFrame(
    PDCELFace *face, Baseline *base_curve, bool closed,
    const std::string& e1, const std::string& e2,
    const SVector3& layup_y2, const BuilderConfig& bcfg) {
  if (face == nullptr) return;

  if (e2 == "layup") {
    face->setLocaly2(layup_y2);
    if (bcfg.tool == AnalysisTool::VABS) {
      face->setTheta1(face->calcTheta1Fromy2(layup_y2));
    }
  }

  const bool e1_baseline = (e1 == "baseline");
  const bool e2_baseline = (e2 == "baseline");
  if (!e1_baseline && !e2_baseline) return;
  if (base_curve == nullptr || base_curve->vertices().size() < 2) return;

  std::vector<SPoint2> poly;
  poly.reserve(base_curve->vertices().size());
  for (auto* v : base_curve->vertices()) {
    poly.emplace_back(v->y(), v->z());
  }
  if (closed && poly.size() > 1
      && poly.front().x() == poly.back().x()
      && poly.front().y() == poly.back().y()) {
    poly.pop_back();
  }
  if (poly.size() < 2) return;

  SPoint2 centroid;
  if (!computeFaceCentroid2DForLayered(face, centroid)) return;
  CurveFrameLookup lookup(poly, closed);
  const SVector3 tangent = lookup.yAxisAt(centroid);
  if (e1_baseline) {
    face->setLocaly1(tangent);
  }
  if (e2_baseline) {
    face->setLocaly2(tangent);
    if (bcfg.tool == AnalysisTool::VABS) {
      face->setTheta1(face->calcTheta1Fromy2(tangent));
    }
  }
}

bool assignLayeredFaceProperties(
    PDCELFace *face, const std::string& name, Layer layer,
    const LayeredCurve& inner, const LayeredCurve& outer,
    Baseline *base_curve, bool closed,
    const std::string& e1, const std::string& e2,
    const BuilderConfig& bcfg) {
  if (face == nullptr || layer.getLamina() == nullptr) return false;
  if (inner.vertices.empty() || outer.vertices.empty()) return false;

  bcfg.model->setFaceName(face, name);
  face->setMaterial(layer.getLamina()->getMaterial());
  face->setTheta3(layer.getAngle());
  face->setLayerType(layer.getLayerType());

  const std::size_t last =
      std::min(inner.vertices.size(), outer.vertices.size()) - 1;
  const SVector3 layup_y2 = averageLayupVector(inner, outer, 0, last);
  applyLayeredFaceFrame(
      face, base_curve, closed, e1, e2, layup_y2, bcfg);
  return true;
}

bool layeredFaceContainsVertex(
    PDCEL *dcel, PDCELFace *face, PDCELVertex *vertex) {
  if (dcel == nullptr || face == nullptr || vertex == nullptr) return false;
  return dcel->findHalfEdgeInFace(vertex, face) != nullptr;
}

// Once the route-ii layered build has destructively split the shared band
// face, the DCEL can no longer be handed to the legacy fallback: the legacy
// area build walks the now-fragmented topology (and connector vertices shared
// with neighbouring segments) and dereferences stale half-edges, which on
// Windows manifests as heap corruption / access violation. So a failure that
// occurs *after* the first mutation aborts the build cleanly instead of
// returning false into that unsafe fallback. Failures *before* any mutation
// still return false and fall back to legacy safely.
[[noreturn]] void failLayeredAfterMutation(
    const std::string& segment_name, const std::string& reason) {
  throw std::runtime_error(
      "layered offset[" + segment_name + "]: " + reason
      + " after the shared DCEL was already mutated; aborting (a legacy "
        "fallback here would corrupt the cross-section mesh)");
}

// A closed (annular) segment has NO legacy fallback: the legacy area/layer
// builder relies on the slit (removed in this plan) to turn the annulus into a
// disk, so `return false` here would hand it an un-sliceable ring and crash.
// Every pre-mutation bail-out that would `return false` for an OPEN segment must
// therefore fail fast for a CLOSED one. This mirrors failLayeredAfterMutation's
// hard abort, but for the pre-mutation closed case — the DCEL is still pristine,
// so the reason is the missing legacy path, not a corrupted mesh.
[[noreturn]] void failClosedLayered(
    const std::string& segment_name, const std::string& reason) {
  throw std::runtime_error(
      "layered offset[" + segment_name + "]: " + reason
      + "; a closed segment has no legacy area/layer fallback (it needs the "
        "removed slit), so this is a hard failure");
}

std::vector<PDCELFace *> splitLayerBandIntoCells(
    PDCELFace *band_face, const LayeredCurve& inner,
    const LayeredCurve& outer, bool is_closed, int side, double dist,
    PDCEL *dcel, const std::string& segment_name,
    const BaseOffsetMap* precomputed_map = nullptr) {
  std::vector<PDCELFace *> cells;
  // note-build-laminate-segment.md §2.1.4-2.1.5: build the staircase vertex
  // mapping between this layer's inner (base) and outer (offset) curves, then
  // place ONE connector per interior staircase step. Reusing the same
  // base-offset map machinery the total-thickness path uses
  // (geo::rebuildBaseOffsetMapFromGeometry — the geometry-derived sibling of
  // buildBaseOffsetMap) is what lets the two curves carry DIFFERENT vertex
  // counts: a dense base offset by a relatively large distance legitimately
  // sheds vertices (Clipper2 merges them), exactly as the full-thickness
  // offsetCurveBase does. Where the staircase advances on only one side it
  // emits a triangle cell instead of a quad — the same tri/quad tiling the
  // legacy createIntermediateAreas walk produces. Hence NO 1:1 requirement.
  //
  // `precomputed_map`: when the caller has already established the inner/outer
  // correspondence (the final layer's arc-resampled, strictly 1:1 outer — see
  // makeArcResampledOuterCurve / issue-20260628 §8.3), use it verbatim and skip
  // the geometric pairing. That guarantees a deterministic, outer-stall-free
  // staircase (every step a quad), which the raw shell distribution cannot.
  BaseOffsetMap local_map;
  bool plan_ok = true;
  if (precomputed_map == nullptr) {
    const auto plan = prevabs::geo::rebuildBaseOffsetMapFromGeometry(
        inner.points, outer.points, is_closed, side, std::max(dist, 1e-12),
        prevabs::geo::readPairingAlgoEnv());
    local_map = plan.id_pairs;
    plan_ok = plan.ok;
  }
  const BaseOffsetMap& pairs =
      (precomputed_map != nullptr) ? *precomputed_map : local_map;
  prevabs::debug::SegmentTraceScope _trace_scope(
      "splitLayerBandIntoCells (pairs=" + std::to_string(pairs.size()) + ")");
  if (band_face == nullptr || !plan_ok || pairs.size() < 2) {
    PLOG(error) << "layered offset[" << segment_name
                << "]: splitLayerBandIntoCells early-out (band="
                << (band_face != nullptr) << ", plan_ok=" << plan_ok
                << ", pairs=" << pairs.size() << ", n_in="
                << inner.vertices.size() << ", n_out=" << outer.vertices.size()
                << ", closed=" << is_closed << ")";
    return cells;
  }

  const int n_in = static_cast<int>(inner.vertices.size());
  const int n_out = static_cast<int>(outer.vertices.size());
  PDCELFace *remaining = band_face;

  // A CLOSED band is an annulus, not a capped disk: its outer loop is the
  // `inner` curve (base side) and its hole is the `outer` curve (toward the
  // centre). There are no end caps — every staircase pair is a real radial
  // connector and the staircase wraps (pairs.front()/back() are the same seam).
  // The FIRST connector (pairs[0]) therefore lands one end on the outer loop
  // and one on the hole, so it must BRIDGE the two loops into a slit disk
  // (bridgeFaceLoops); the remaining connectors then split that disk exactly
  // like the open path, and the leftover after the last split is the wrap cell.
  if (is_closed) {
    const int b0 = pairs[0].base;
    const int o0 = pairs[0].offset;
    if (b0 < 0 || b0 >= n_in || o0 < 0 || o0 >= n_out) {
      PLOG(error) << "layered offset[" << segment_name
                  << "]: closed seam connector out of range (base=" << b0
                  << "/" << n_in << ", offset=" << o0 << "/" << n_out << ")";
      return {};
    }
    remaining = dcel->bridgeFaceLoops(
        band_face, inner.vertices[b0], outer.vertices[o0]);
    if (remaining == nullptr) {
      PLOG(error) << "layered offset[" << segment_name
                  << "]: failed to bridge closed layer band at the seam";
      prevabs::debug::segmentTracePush("closed band bridge FAILED @seam");
      return {};
    }
    // A two-entry closed staircase (seam + wrap) has a single distinct
    // connector: the bridge alone makes the whole annulus one cell.
    if (pairs.size() == 2) {
      cells.push_back(remaining);
      return cells;
    }
  } else {
    // Two staircase entries are just the head/tail end caps with no interior
    // connector between them: the whole band is a single cell.
    if (pairs.size() == 2) {
      cells.push_back(band_face);
      return cells;
    }
  }

  // Interior staircase steps. For the OPEN band these skip the first/last
  // entries (its two end caps, already edges of band_face); for the CLOSED band
  // pairs[0] was consumed by the bridge above and pairs.back() is the wrap, so
  // the same [1, size-2] range splits off every cell but the final wrap cell.
  for (std::size_t p = 1; p + 1 < pairs.size(); ++p) {
    const int bi = pairs[p].base;
    const int oj = pairs[p].offset;
    if (bi < 0 || bi >= n_in || oj < 0 || oj >= n_out) {
      PLOG(error) << "layered offset[" << segment_name
                  << "]: staircase index out of range at step " << p
                  << " (base=" << bi << "/" << n_in
                  << ", offset=" << oj << "/" << n_out << ")";
      prevabs::debug::segmentTracePush(
          "staircase index OOR @step " + std::to_string(p));
      return {};
    }
    std::vector<PDCELVertex *> connector = {
        inner.vertices[bi], outer.vertices[oj]};
    std::list<PDCELFace *> split_faces =
        dcel->splitFaceByPolyline(remaining, connector);
    if (split_faces.size() != 2) {
      PLOG(error) << "layered offset[" << segment_name
                  << "]: failed to split layer band cell at staircase step "
                  << p;
      prevabs::debug::segmentTracePush(
          "split failed @staircase step " + std::to_string(p));
      return {};
    }

    PDCELFace *f0 = split_faces.front();
    PDCELFace *f1 = split_faces.back();
    // Attribute the just-split cell vs the remaining (tail-side) band by
    // GEOMETRY, not DCEL vertex identity. The cell spans staircase steps
    // p-1..p, so its expected centroid is the average of those four corner
    // points (when the staircase advanced on only one side two corners
    // coincide and the cell is a triangle — the average is still interior).
    // Identity tests (does face X contain curve vertex Y) mis-fire because a
    // curve endpoint coincident with an existing DCEL vertex is replaced by
    // the canonical vertex during the split, leaving the curve's own vertex
    // pointer off the face — a spurious "ambiguous" that left the DCEL
    // half-split and crashed the legacy fallback at shared connectors.
    const int bi_prev = pairs[p - 1].base;
    const int oj_prev = pairs[p - 1].offset;
    const double cell_y =
        0.25 * (inner.vertices[bi_prev]->y() + inner.vertices[bi]->y()
              + outer.vertices[oj_prev]->y() + outer.vertices[oj]->y());
    const double cell_z =
        0.25 * (inner.vertices[bi_prev]->z() + inner.vertices[bi]->z()
              + outer.vertices[oj_prev]->z() + outer.vertices[oj]->z());
    SPoint2 c0, c1;
    if (!computeFaceCentroid2DForLayered(f0, c0)
        || !computeFaceCentroid2DForLayered(f1, c1)) {
      PLOG(error) << "layered offset[" << segment_name
                  << "]: cannot compute split-face centroid at staircase step "
                  << p;
      prevabs::debug::segmentTracePush(
          "centroid failure @staircase step " + std::to_string(p)
          + " of " + std::to_string(pairs.size() - 1));
      return {};
    }
    // (B) Post-split degeneracy self-check (issue-20260628 §8.3). A healthy
    // split yields two well-separated, non-degenerate faces. A near-zero-area
    // connector (e.g. the final-layer outer-stall fan, where consecutive
    // connectors share one shell vertex) makes splitFaceByPolyline return two
    // OVERLAPPING faces with coincident centroids; left undetected, that corrupt
    // face becomes `remaining` and the NEXT step crashes confusingly. Catch the
    // degeneracy here, at the step that produced it, with a clear diagnostic.
    const int e0 = faceOuterEdgeCount(f0);
    const int e1 = faceOuterEdgeCount(f1);
    if (e0 < 3 || e1 < 3 || c0.distance(c1) < GEO_COINCIDENCE_TOL) {
      PLOG(error) << "layered offset[" << segment_name
                  << "]: degenerate split at staircase step " << p << "/"
                  << (pairs.size() - 1) << " (edges f0=" << e0 << " f1=" << e1
                  << ", |c0-c1|=" << c0.distance(c1) << "); connector ("
                  << inner.vertices[bi]->y() << "," << inner.vertices[bi]->z()
                  << ")->(" << outer.vertices[oj]->y() << ","
                  << outer.vertices[oj]->z() << ")";
      prevabs::debug::segmentTracePush(
          "degenerate split @staircase step " + std::to_string(p)
          + " of " + std::to_string(pairs.size() - 1));
      return {};
    }
    const double d0 = (c0.x() - cell_y) * (c0.x() - cell_y)
                    + (c0.y() - cell_z) * (c0.y() - cell_z);
    const double d1 = (c1.x() - cell_y) * (c1.x() - cell_y)
                    + (c1.y() - cell_z) * (c1.y() - cell_z);
    const bool f0_is_cell = (d0 <= d1);
    PDCELFace *cell = f0_is_cell ? f0 : f1;
    remaining = f0_is_cell ? f1 : f0;
    cells.push_back(cell);
  }

  cells.push_back(remaining);
  return cells;
}

}  // namespace

namespace {

void logSkippingSegmentAreasAction(
    const char *caller, const std::string &segment_name,
    const std::string &reason) {
  if (config.debug_level >= DebugLevel::join) PLOG(debug) << "skipping " << caller << " for segment '"
              << segment_name << "': " << reason;
}

void deleteDetachedLineSegment(PGeoLineSegment *segment) {
  if (segment == nullptr) {
    return;
  }
  delete segment->v1();
  delete segment->v2();
  delete segment;
}


std::string faceLabel(PDCELFace *face, PModel *model) {
  if (face == nullptr) {
    return "nullptr";
  }

  (void)model;
  return face->displayLabel();
}




void logVertexEdgeRing(
    PDCELVertex *vertex, PModel *model, const std::string &prefix) {
  if (vertex == nullptr) {
    PLOG(error) << prefix << ": vertex=nullptr";
    return;
  }

  PLOG(error) << prefix << ": vertex=" << vertex->printString();
  if (vertex->edge() == nullptr) {
    PLOG(error) << prefix << ": edge ring is empty";
    return;
  }

  PDCELHalfEdge *start = vertex->edge();
  PDCELHalfEdge *he = start;
  int iter = 0;
  do {
    if (he == nullptr) {
      PLOG(error) << prefix << ": encountered nullptr half-edge in ring";
      return;
    }
    if (++iter > kDCELLoopHardCap) {
      PLOG(error) << prefix
                  << ": edge ring walk exceeded "
                  << kDCELLoopHardCap << " steps";
      return;
    }

    std::ostringstream line;
    line << prefix
         << ": outgoing[" << (iter - 1) << "] "
         << he->source()->printString()
         << " -> " << he->target()->printString()
         << " | face=" << faceLabel(he->face(), model)
         << " | loop=" << (he->loop() ? "set" : "nullptr");
    PLOG(error) << line.str();

    if (he->twin() == nullptr) {
      PLOG(error) << prefix << ": outgoing[" << (iter - 1)
                  << "] has nullptr twin";
      return;
    }
    he = he->twin()->next();
  } while (he != nullptr && he != start);

  if (he == nullptr) {
    PLOG(error) << prefix << ": edge ring terminated at nullptr next";
  }
}




void logMissingHalfEdgeInFace(
    const std::string &caller, const std::string &segment_name,
    PDCELVertex *vertex, PDCELFace *expected_face,
    const BuilderConfig &bcfg) {
  PLOG(error) << caller
              << ": findHalfEdgeInFace returned nullptr"
              << " for segment '" << segment_name << "'"
              << ", vertex=" << (vertex ? vertex->printString() : "nullptr")
              << ", expected_face="
              << faceLabel(expected_face, bcfg.model);
  logVertexEdgeRing(
      vertex, bcfg.model,
      caller + ": vertex edge ring for segment '" + segment_name + "'");
}




void logMissingHalfEdgeBetween(
    const std::string &caller, const std::string &segment_name,
    PDCELVertex *source, PDCELVertex *target, const BuilderConfig &bcfg) {
  PLOG(error) << caller
              << ": findHalfEdgeBetween returned nullptr"
              << " for segment '" << segment_name << "'"
              << ", source="
              << (source ? source->printString() : "nullptr")
              << ", target="
              << (target ? target->printString() : "nullptr");
  logVertexEdgeRing(
      source, bcfg.model,
      caller + ": source edge ring for segment '" + segment_name + "'");
}




// plan-20260522 §3.2: geometric replacement for the legacy
// `usesOffsetAsBaseAtPair(Δbase=0)` signal. The "base edge degenerate"
// condition is no longer derived from the staircase's discrete index
// history — it's read straight off the two base vertices that bound
// the area:
//
//   if |base[i] - base[i-1]| < GEO_TOL → use offset edge as "base"
//
// Under the legacy editor-splice path, Δbase=0 happens exactly when
// the two indexed base vertices are the same PDCELVertex* (so their
// distance is 0). Under the Phase 2 geometry-derived staircase, the
// rebuilt pairs may instead have Δbase=1 with two distinct PDCELVertex
// pointers that nonetheless sit on top of each other after join trim;
// the geometric check still fires.
bool usesOffsetAsBaseAt(
    const PDCELVertex *vb_prev, const PDCELVertex *vb_curr) {
  if (vb_prev == nullptr || vb_curr == nullptr) return false;
  return vb_prev->point().distance(vb_curr->point()) < GEO_TOL;
}




PGeoLineSegment *buildAreaBaseSegmentFromPair(
    Baseline *curve_base, Baseline *curve_offset,
    const BaseOffsetMap &pairs, std::size_t pair_index) {
  const int vbi = pairs[pair_index].base;
  const int voi = pairs[pair_index].offset;
  // vbi == 0 means the current pair sits on the very first base vertex
  // (the legacy Δbase=0 case at step 1, equivalent to "use offset edge").
  // There is no preceding base vertex to form a base edge with, so route
  // straight to the offset branch and skip the geometric distance check.
  const bool use_offset_as_base =
      (vbi <= 0)
          ? true
          : usesOffsetAsBaseAt(curve_base->vertices()[vbi - 1],
                               curve_base->vertices()[vbi]);

  if (!use_offset_as_base) {
    return new PGeoLineSegment(
        curve_base->vertices()[vbi - 1],
        curve_base->vertices()[vbi]);
  }

  return new PGeoLineSegment(
      curve_offset->vertices()[voi - 1],
      curve_offset->vertices()[voi]);
}




bool traversesPrevOnHeadBound(int layup_side) {
  // Open-bound traversal starts from the base vertex and then walks the
  // current face boundary until it meets the offset-side endpoint.
  // The head and tail see opposite boundary orientations, so the correct
  // traversal direction is intentionally inverted between the two ends.
  return layup_side > 0;
}




bool traversesPrevOnTailBound(int layup_side) {
  return layup_side < 0;
}

} // namespace




// Split the bound edge [vb, vo] parametrically by layup into layer vertices.
// Splits DCEL edges in place. Returns the new intermediate vertices.
std::vector<PDCELVertex *> Segment::splitBoundByLayup(
    PDCELVertex *vb, PDCELVertex *vo, const BuilderConfig &bcfg) {

  std::vector<PDCELVertex *> layer_vertices;
  PDCELVertex *v_layer, *v_layer_prev = nullptr;
  double cumu_thk = 0, norm_thk;

  for (int i = 0; i < _layup->getLayers().size() - 1; ++i) {
    cumu_thk += _layup->getLayers()[i].getLamina()->getThickness() *
                _layup->getLayers()[i].getStack();
    norm_thk = cumu_thk / _layup->getTotalThickness();
    v_layer = new PDCELVertex(
        getParametricPoint(vb->point(), vo->point(), norm_thk));

  PDCELVertex *v1_tmp = (i == 0) ? vb : v_layer_prev;
  PDCELHalfEdge *he_tmp = bcfg.dcel->findHalfEdgeBetween(v1_tmp, vo);
  if (he_tmp == nullptr) {
    logMissingHalfEdgeBetween(
        "splitBoundByLayup", _name, v1_tmp, vo, bcfg);
    break;
  }
  v_layer = bcfg.dcel->splitEdge(he_tmp, v_layer);

    layer_vertices.push_back(v_layer);
    v_layer_prev = v_layer;
  }

  return layer_vertices;
}




// Search face boundary edges from v_prev for the intersection with the
// infinite support line of ls_offset. Open-bound construction uses offset
// construction lines whose valid hit may lie beyond the finite ls_offset end
// points, so only the face-edge parameter is range-checked here.
// go_prev controls traversal direction (true = prev(), false = next()).
// May split an edge. Returns the intersection vertex, or nullptr if not found.
PDCELVertex *Segment::findLayerIntersectionOnFace(
    PDCELVertex *v_prev, PDCELFace *face,
    PGeoLineSegment *ls_offset, bool go_prev, PDCELVertex *stop_vertex,
    const BuilderConfig &bcfg) {

  PDCELVertex *v_layer = nullptr;
  double u1_tmp, u2_tmp;
  const double endpoint_tol =
      (bcfg.tol > TOLERANCE) ? bcfg.tol : TOLERANCE;

  PDCELHalfEdge *he_tmp = bcfg.dcel->findHalfEdgeInFace(v_prev, face);
  if (he_tmp == nullptr) {
    logMissingHalfEdgeInFace(
        "findLayerIntersectionOnFace", _name, v_prev, face, bcfg);
    return nullptr;
  }
  PDCELHalfEdge *he_base = he_tmp;
  if (go_prev) {
    he_tmp = he_tmp->prev();
  }

  int _iter = 0;
  do {
    if (++_iter > 65536) {
      throw std::runtime_error(
          "DCEL loop walk exceeded 65536 iterations"
          " in findLayerIntersectionOnFace at " +
          he_base->printString());
    }
        if (bcfg.debug_level >= DebugLevel::join) PLOG(debug) << "";
        if (bcfg.debug_level >= DebugLevel::join) PLOG(debug) << "he_tmp: " + he_tmp->printString();

    // Skip degenerate edges (source and target at the same position).
    // These can be created by splitEdge when the split vertex lands on an
    // existing endpoint, producing a zero-length half-edge.
    if (he_tmp->source()->point().distance(he_tmp->target()->point())
        < TOLERANCE) {
      PLOG(warning) << "degenerate edge, skipping";
    }
    else {
      bool not_parallel = calcLineIntersection2D(
          he_tmp->toLineSegment(), ls_offset, u1_tmp, u2_tmp);

            if (bcfg.debug_level >= DebugLevel::join) PLOG(debug) << "u1_tmp = " << u1_tmp;
            if (bcfg.debug_level >= DebugLevel::join) PLOG(debug) << "u2_tmp = " << u2_tmp;

      if (not_parallel) {
        if (fabs(u1_tmp) <= endpoint_tol) {
                    if (bcfg.debug_level >= DebugLevel::join) PLOG(debug) << "  case 1: intersect at source";
          v_layer = he_tmp->source();
          break;
        }
        else if (fabs(1 - u1_tmp) <= endpoint_tol) {
                    if (bcfg.debug_level >= DebugLevel::join) PLOG(debug) << "  case 2: intersect at target";
          v_layer = he_tmp->target();
          break;
        }
        else if (u1_tmp > 0 && u1_tmp < 1) {
                    if (bcfg.debug_level >= DebugLevel::join) PLOG(debug) << "  case 3: intersect current edge";
          v_layer = he_tmp->toLineSegment()->getParametricVertex1(u1_tmp);
                    if (bcfg.debug_level >= DebugLevel::join) PLOG(debug) << "  v_layer: " + v_layer->printString();
          v_layer = bcfg.dcel->splitEdge(he_tmp, v_layer);
          break;
        }
        // u1 out of range on this edge: fall through to advance
      }
      // parallel: fall through to advance
    }

    // Stop condition: reached the boundary of the offset curve
    if (go_prev && he_tmp->source() == stop_vertex) {
            if (bcfg.debug_level >= DebugLevel::join) PLOG(debug) << "reach the last edge";
      break;
    }
    if (!go_prev && he_tmp->target() == stop_vertex) {
            if (bcfg.debug_level >= DebugLevel::join) PLOG(debug) << "reach the last edge";
      break;
    }

    // Advance in traversal direction — always valid, no stale pointer
    he_tmp = go_prev ? he_tmp->prev() : he_tmp->next();

  } while (he_tmp != he_base);

  if (bcfg.debug_level >= DebugLevel::join) PLOG(debug) << "findLayerIntersectionOnFace: skipping remaining search"
              << " for segment '" << _name << "' because no face-boundary"
              << " intersection was found before reaching the stop vertex";
  return v_layer;
}




// Build layer vertices on an open bound by intersecting offset line segments
// with the face boundary. v_start is the seed vertex; go_prev controls
// traversal direction. Returns the new layer vertices.
std::vector<PDCELVertex *> Segment::buildOpenBoundLayerVertices(
    PDCELVertex *v_start, PGeoLineSegment *ls_base,
    bool go_prev, PDCELVertex *stop_vertex, const BuilderConfig &bcfg) {

  std::vector<PDCELVertex *> layer_vertices;
  PDCELVertex *v_layer_prev = v_start;
  double cumu_thk = 0;
  const int offset_dir = layupSide();

  for (int i = 0; i < _layup->getLayers().size() - 1; ++i) {
        if (bcfg.debug_level >= DebugLevel::join) PLOG(debug) << "layer " + std::to_string(i + 1);

    cumu_thk += _layup->getLayers()[i].getLamina()->getThickness() *
                _layup->getLayers()[i].getStack();

        if (bcfg.debug_level >= DebugLevel::join) PLOG(debug) << "cumu_thk = " << cumu_thk;

    PGeoLineSegment *ls_offset = offsetLineSegment(ls_base, offset_dir, cumu_thk);
        if (bcfg.debug_level >= DebugLevel::join) PLOG(debug) << "ls_offset: " + ls_offset->printString();

    PDCELVertex *v_layer = findLayerIntersectionOnFace(
        v_layer_prev, _face, ls_offset, go_prev, stop_vertex, bcfg);
    deleteDetachedLineSegment(ls_offset);

    if (v_layer == nullptr) {
      PLOG(error) << "cannot find intersection";
      break;
    }

        if (bcfg.debug_level >= DebugLevel::join) PLOG(debug) << "v_layer: " + v_layer->printString();
    layer_vertices.push_back(v_layer);
    v_layer_prev = v_layer;
  }

  return layer_vertices;
}




// Section 1: compute beginning-bound layer vertices.
// For closed segments, also fills first_bound_vertices.
std::vector<PDCELVertex *> Segment::buildBeginningBound(
    std::vector<PDCELVertex *> &first_bound_vertices,
    const BuilderConfig &bcfg) {

    if (bcfg.debug_level >= DebugLevel::join) PLOG(debug) << 
      "1. creating the beginning bound of the first area";

  std::vector<PDCELVertex *> prev_bound_vertices;

  if (closed()) {
        if (bcfg.debug_level >= DebugLevel::join) PLOG(debug) << "closed segment";
    prev_bound_vertices = splitBoundByLayup(
        _curve_base->vertices()[0], _curve_offset->vertices()[0], bcfg);
    first_bound_vertices = prev_bound_vertices;
  }
  else {
        if (bcfg.debug_level >= DebugLevel::join) PLOG(debug) << "open segment";

    PDCELVertex *vb = _curve_base->vertices()[0];
    PDCELVertex *vo = _curve_offset->vertices()[0];
        if (bcfg.debug_level >= DebugLevel::join) PLOG(debug) <<
        "first vertex of the base: " + vb->printString();
        if (bcfg.debug_level >= DebugLevel::join) PLOG(debug) <<
        "first vertex of the offset: " + vo->printString();

    // Head area's base edge runs base[0] -> base[1].
    const bool use_offset_as_base = usesOffsetAsBaseAt(
        _curve_base->vertices()[0], _curve_base->vertices()[1]);
    const int layup_side = layupSide();

    PGeoLineSegment *ls_base;
    bool owns_ls_base_vertices = false;
    if (!use_offset_as_base) {
      ls_base = new PGeoLineSegment(
          _curve_base->vertices()[0], _curve_base->vertices()[1]);
    }
    else {
      // Degenerate head case: the first staircase step stays on base vertex 0,
      // so the physical head thickness stack must be generated from the offset
      // edge and translated back by the total laminate thickness.
      PGeoLineSegment *ls_tmp = new PGeoLineSegment(
          _curve_offset->vertices()[0], _curve_offset->vertices()[1]);
      const int dir = -layup_side;
      ls_base = offsetLineSegment(ls_tmp, dir, _layup->getTotalThickness());
      owns_ls_base_vertices = true;
            if (bcfg.debug_level >= DebugLevel::join) PLOG(debug) << "degenerated case";
            if (bcfg.debug_level >= DebugLevel::join) PLOG(debug) << " ls_tmp: " + ls_tmp->printString();
            if (bcfg.debug_level >= DebugLevel::join) PLOG(debug) << " ls_base: " + ls_base->printString();
      delete ls_tmp;
    }

    const bool go_prev = traversesPrevOnHeadBound(layup_side);
    prev_bound_vertices = buildOpenBoundLayerVertices(
        _curve_base->vertices()[0], ls_base, go_prev,
        _curve_offset->vertices().front(), bcfg);
    if (owns_ls_base_vertices) {
      deleteDetachedLineSegment(ls_base);
    }
    else {
      delete ls_base;
    }
  }

  return prev_bound_vertices;
}




// Section 2: create all intermediate areas and update prev_bound_vertices
// and count.
void Segment::createIntermediateAreas(
    std::vector<PDCELVertex *> &prev_bound_vertices,
    int &count, const BuilderConfig &bcfg) {

    if (bcfg.debug_level >= DebugLevel::join) PLOG(debug) << "2. creating areas";

  // issue-20260521-skip-dropped-areas: in regions where Clipper2 produced
  // no genuine offset correspondence (h_i < |dist| → base vertex eaten by
  // inset), the staircase forward-fills every base index in the dropped
  // range to the same offset vertex. splitFace'ing through that shared
  // offset vertex generates a fan of slivers that gmsh cannot recover.
  // For OPEN segments we skip those areas outright — geometrically the
  // laminate is locally absent at the endpoints, the staircase already
  // shrinks the covered span, and the user has been warned via the M/N
  // warning + "skin dropped" message.
  // For CLOSED segments the dropped range lives in the middle of a loop;
  // skipping those areas would tear the shell open and break the DCEL
  // face closure. Keep the legacy forward-fill behavior — sliver areas
  // are sometimes the lesser evil there. Until a proper closed-loop
  // dropped-region story exists, the closed branch keeps the gmsh risk.
  // Gated behind useSkipDroppedAreas() (XML <general>/<skip_dropped_areas>,
  // default OFF): the layered_offset build path no longer needs this
  // workaround, so by default the full base range is kept and these areas
  // are built normally.
  const bool skip_dropped_areas =
      useSkipDroppedAreas() && !closed() && !_dropped_base_ranges_lo.empty();
  auto inDroppedRange = [&](int base_idx) {
    if (!skip_dropped_areas) return false;
    for (std::size_t r = 0; r < _dropped_base_ranges_lo.size(); ++r) {
      if (base_idx >= _dropped_base_ranges_lo[r]
          && base_idx <= _dropped_base_ranges_hi[r]) {
        return true;
      }
    }
    return false;
  };

  // Boundary-area skip: the area at pair k uses the base edge
  // base[pairs[k].base - 1] → base[pairs[k].base]. When EITHER endpoint
  // sits inside a dropped range, the edge crosses the cusp/thin region
  // and gmsh cannot mesh the resulting long thin sliver — this is the
  // same `Unable to recover the edge` failure mode that motivated Stage E
  // pre-trim (plan-20260522-stage-e-pretrim.md, Phase B). Skipping when
  // the previous base index is dropped removes the one extra area
  // immediately after the dropped run on each side.
  auto areaAtPairUsesDroppedEdge = [&](int base_idx) {
    if (!skip_dropped_areas) return false;
    if (inDroppedRange(base_idx)) return true;
    if (base_idx > 0 && inDroppedRange(base_idx - 1)) return true;
    return false;
  };
  for (auto k = 1; k < _base_offset_indices_pairs.size() - 1; k++) {
        if (bcfg.debug_level >= DebugLevel::join) PLOG(debug) << "area " + std::to_string(k);
    const int layup_side = layupSide();

    int vbi_tmp = _base_offset_indices_pairs[k].base;
    int voi_tmp = _base_offset_indices_pairs[k].offset;

    if (areaAtPairUsesDroppedEdge(vbi_tmp)) {
      if (bcfg.debug_level >= DebugLevel::join) {
        PLOG(debug) << "  skipping area at base[" << vbi_tmp
                    << "] (base edge endpoint in dropped range — "
                       "skin locally absent or boundary sliver)";
      }
      continue;
    }

    PDCELVertex *vb_tmp = _curve_base->vertices()[vbi_tmp];
    PDCELVertex *vo_tmp = _curve_offset->vertices()[voi_tmp];
        if (bcfg.debug_level >= DebugLevel::join) PLOG(debug) << "  base vertex: " + vb_tmp->printString();
        if (bcfg.debug_level >= DebugLevel::join) PLOG(debug) << 
        "  offset vertex: " + vo_tmp->printString();

    std::list<PDCELFace *> new_faces =
        bcfg.dcel->splitFace(_face, vb_tmp, vo_tmp);

    PArea *area = new PArea(this);
    count++;
    if (layup_side > 0) {
      area->setFace(new_faces.front());
      _face = new_faces.back();
    }
    else {
      area->setFace(new_faces.back());
      _face = new_faces.front();
    }

    PGeoLineSegment *ls_base = buildAreaBaseSegmentFromPair(
        _curve_base, _curve_offset.get(), _base_offset_indices_pairs, k);

    // Kept for debug prints; PArea owns the segment via its destructor.
    area->setLineSegmentBase(ls_base);

    // e1/e2 == "baseline" are now resolved per-face in
    // PArea::applyFrameFromBaseCurve (Phase B of
    // plan-20260514-decouple-local-frame-from-map.md). Only the "layup"
    // selector still pulls the through-thickness vector here.
    if (_mat_orient_e2 == "layup") {
      PGeoLineSegment *ls_layup = new PGeoLineSegment(vb_tmp, vo_tmp);
      area->setLocaly2(ls_layup->toVector());
      delete ls_layup;
    }

    bcfg.model->setFaceName(
        area->face(), _name + "_area_" + std::to_string(count));
    area->setPrevBoundVertices(prev_bound_vertices);

    for (auto v : splitBoundByLayup(vb_tmp, vo_tmp, bcfg)) {
      area->addNextBoundVertex(v);
    }

    _areas.emplace_back(area);
    prev_bound_vertices = area->nextBoundVertices();
  }

}




// Section 3: create and append the last (or only) area.
void Segment::buildLastArea(
    const std::vector<PDCELVertex *> &prev_bound_vertices,
    const std::vector<PDCELVertex *> &first_bound_vertices,
    int count, const BuilderConfig &bcfg) {

    if (bcfg.debug_level >= DebugLevel::join) PLOG(debug) << "3. creating the last area";

  PArea *area = new PArea(this);
  count++;
  area->setFace(_face);

  const std::size_t last_pair_index = _base_offset_indices_pairs.size() - 1;
  PGeoLineSegment *ls_base = buildAreaBaseSegmentFromPair(
      _curve_base, _curve_offset.get(), _base_offset_indices_pairs, last_pair_index);
  // Kept for debug prints; PArea owns the segment via its destructor.
  area->setLineSegmentBase(ls_base);

  // e1/e2 == "baseline" are now resolved per-face in
  // PArea::applyFrameFromBaseCurve (Phase B of
  // plan-20260514-decouple-local-frame-from-map.md). Only the "layup"
  // selector still pulls the through-thickness vector here.
  if (_mat_orient_e2 == "layup") {
    PGeoLineSegment *ls_layup = new PGeoLineSegment(
        _curve_base->vertices().back(),
        _curve_offset->vertices().back());
    area->setLocaly2(ls_layup->toVector());
    delete ls_layup;
  }

  bcfg.model->setFaceName(
      area->face(), _name + "_area_" + std::to_string(count));
  area->setPrevBoundVertices(prev_bound_vertices);

  if (closed()) {
    area->setNextBoundVertices(first_bound_vertices);
  }
  else {
    // Tail area's base edge runs base[N-2] -> base[N-1].
    const auto &base_v_tail = _curve_base->vertices();
    const bool use_offset_as_base = usesOffsetAsBaseAt(
        base_v_tail[base_v_tail.size() - 2],
        base_v_tail[base_v_tail.size() - 1]);
    const int layup_side = layupSide();

    PGeoLineSegment *ls_base_end;
    bool owns_ls_base_end_vertices = false;
    if (!use_offset_as_base) {
      ls_base_end = new PGeoLineSegment(
          _curve_base->vertices()[_curve_base->vertices().size() - 2],
          _curve_base->vertices()[_curve_base->vertices().size() - 1]);
    }
    else {
      // Degenerate tail case: the last staircase step stays on the previous
      // base vertex, so the closing bound is anchored on the offset edge and
      // shifted back to recover the effective base-side construction segment.
      // Index the offset vector by its own size — under the legacy open-path
      // resample step M == N held implicitly, but the Clipper2 backend's
      // nearest-pairing variant (issue-20260520) skips that resample and
      // allows M < N.
      const auto& off_v_tail = _curve_offset->vertices();
      PGeoLineSegment *ls_tmp = new PGeoLineSegment(
          off_v_tail[off_v_tail.size() - 2],
          off_v_tail[off_v_tail.size() - 1]);
      const int dir = layup_side;
      ls_base_end = offsetLineSegment(ls_tmp, dir, _layup->getTotalThickness());
      owns_ls_base_end_vertices = true;
            if (bcfg.debug_level >= DebugLevel::join) PLOG(debug) << "degenerated case";
            if (bcfg.debug_level >= DebugLevel::join) PLOG(debug) << " ls_tmp: " + ls_tmp->printString();
            if (bcfg.debug_level >= DebugLevel::join) PLOG(debug) << " ls_base: " + ls_base_end->printString();
      delete ls_tmp;
    }

    const bool go_prev = traversesPrevOnTailBound(layup_side);
    for (auto v : buildOpenBoundLayerVertices(
             _curve_base->vertices().back(), ls_base_end, go_prev,
             _curve_offset->vertices().back(), bcfg)) {
      area->addNextBoundVertex(v);
    }
    if (owns_ls_base_end_vertices) {
      deleteDetachedLineSegment(ls_base_end);
    }
    else {
      delete ls_base_end;
    }
  }

  _areas.emplace_back(area);
}

// Builds a layered-offset segment by tiling each layer into its own DCEL
// face(s). Returns true on success; returns false to REQUEST the legacy
// area/layer build (the caller's fallback path).
//
// Fallback contract — this is the central invariant of the function:
//   * `return false` is only safe BEFORE any splitFaceByPolyline call has
//     mutated the shared DCEL. Up to that point the geometry is untouched and
//     the legacy builder can take over cleanly.
//   * Once the DCEL has been mutated (`dcel_mutated == true`), a partial tiling
//     already exists; falling back would leave a corrupted mesh. From that
//     point any failure is a HARD abort via failLayeredAfterMutation(), never a
//     `return false`.
// Hence every `return false` below sits in the pre-mutation prologue or the
// per-layer loop's pre-split checks; everything after the first split aborts.
bool Segment::buildLayeredOffsetAreas(const BuilderConfig &bcfg) {
  // Missing inputs: nothing to build from. A closed segment cannot fall back
  // (no slit -> legacy crash), so fail fast; an open one lets legacy try.
  if (_layup == nullptr || _curve_base == nullptr || _curve_offset == nullptr
      || _face == nullptr) {
    if (closed()) {
      failClosedLayered(_name,
          "missing required inputs (layup/base/offset/face)");
    }
    return false;
  }
  prevabs::debug::SegmentTraceScope _trace_scope("buildLayeredOffsetAreas");

  std::vector<Layer> layers = _layup->getLayers();
  const int n_layers = static_cast<int>(layers.size());
  if (n_layers <= 0) {  // no layers
    if (closed()) failClosedLayered(_name, "layup has no layers");
    return false;  // open -> fallback
  }
  for (int k = 0; k < n_layers; ++k) {
    // A layer without a lamina has no material/thickness to offset or assign;
    // we cannot tile it. Closed fails fast; open hands off to the legacy builder.
    if (layers[k].getLamina() == nullptr) {
      if (closed()) {
        failClosedLayered(_name,
            "layer " + std::to_string(k + 1) + " has no lamina");
      }
      PLOG(warning) << "layered offset[" << _name << "]: layer "
                    << (k + 1) << " has no lamina; falling back";
      return false;
    }
  }

  const bool is_closed = closed();
  // Layup side is required to know which side of the base curve to offset onto;
  // 0 means it could not be determined -> fallback.
  const int side = requireValidLayupSide("buildLayeredOffsetAreas");
  if (side == 0) {
    if (is_closed) {
      failClosedLayered(_name, "could not determine layup side");
    }
    prevabs::debug::segmentTracePush("layered: invalid layup side -> FALLBACK");
    return false;
  }
  const int clipper_side = is_closed ? -side : side;
  prevabs::debug::segmentTracePush(
      "layered: enter (n_layers=" + std::to_string(n_layers)
      + ", side=" + std::to_string(side) + ")");

  LayeredCurve base_curve =
      makeExistingCurve(_curve_base->vertices(), is_closed);
  LayeredCurve shell_curve =
      makeExistingCurve(_curve_offset->vertices(), is_closed);
  const std::size_t n = base_curve.vertices.size();
  // No 1:1 base/shell vertex requirement: the per-layer band tiler
  // (splitLayerBandIntoCells) builds a staircase correspondence between each
  // layer's inner/outer curves, so the shell legitimately carries a different
  // vertex count than the base — a dense base offset by the (relatively large)
  // total thickness sheds vertices the same way the legacy offsetCurveBase
  // does. The only structural gate is that both curves are non-degenerate
  // (>= 2 vertices); an empty/degenerate offset still falls back, safely,
  // before any DCEL mutation.
  if (n < 2 || shell_curve.vertices.size() < 2) {
    if (is_closed) {
      failClosedLayered(_name,
          "degenerate base/shell curve (base=" + std::to_string(n)
          + ", shell=" + std::to_string(shell_curve.vertices.size()) + ")");
    }
    PLOG(warning) << "layered offset[" << _name
                  << "]: degenerate base/shell curve (base=" << n
                  << ", shell=" << shell_curve.vertices.size()
                  << "); falling back to legacy area/layer build";
    prevabs::debug::segmentTracePush(
        "layered: degenerate base/shell curve (base=" + std::to_string(n)
        + ", shell=" + std::to_string(shell_curve.vertices.size())
        + ") -> FALLBACK");
    return false;
  }

  // Single layer: no internal boundaries to split, so the existing face IS the
  // one and only layer face. Assign properties directly — this is a SUCCESS
  // path, not a fallback, and it never touches the DCEL topology.
  if (n_layers == 1) {
    Layer layer = layers.front();
    _layered_faces.push_back(_face);
    bcfg.model->setFaceName(_face, _name + "_layer_1_face_1");
    _face->setMaterial(layer.getLamina()->getMaterial());
    _face->setTheta3(layer.getAngle());
    _face->setLayerType(layer.getLayerType());
    // base and shell may carry different vertex counts; index the shared last
    // valid index of both for the representative through-thickness vector.
    const std::size_t layup_last =
        std::min(base_curve.vertices.size(), shell_curve.vertices.size()) - 1;
    const SVector3 layup_y2 =
        averageLayupVector(base_curve, shell_curve, 0, layup_last);
    applyLayeredFaceFrame(
        _face, _curve_base, is_closed,
        _mat_orient_e1, _mat_orient_e2, layup_y2, bcfg);
    _state = LifecycleState::AreasBuilt;
    prevabs::debug::segmentTracePush(
        "layered: n_layers==1 -> single face (SUCCESS)");
    PLOG(info) << "built layered offset segment " << _name
               << ": layers=1, faces=1";
    return true;
  }

  // Closed-only: pick the intermediate-layer offset direction from GEOMETRY,
  // not the winding-dependent `-side` flag. The layer rings must march from the
  // base ring toward the offset shell. Compare enclosed areas: if the shell ring
  // is smaller than the base, the layers SHRINK inward; if larger, they GROW
  // outward. `-side` alone does not encode this (it flips with baseline winding
  // and layup direction — e.g. a cw tube offsets the wrong way, landing the
  // layer rings outside the shell). Probe the actual Clipper2 result once with
  // the first layer thickness and flip the side if it moved the wrong way.
  int layer_clipper_side = clipper_side;
  const double area_base_ring = std::fabs(signedArea2D(base_curve.points));
  const double area_shell_ring = std::fabs(signedArea2D(shell_curve.points));
  const bool base_is_outer = area_base_ring >= area_shell_ring;
  if (is_closed) {
    const bool want_shrink = base_is_outer;  // shell inside base -> shrink
    const double probe_thk =
        layers[0].getLamina()->getThickness() * layers[0].getStack();
    LayeredCurve probe = makeLayerOffsetCurve(
        base_curve.points, is_closed, clipper_side, std::max(probe_thk, 1e-9));
    if (probe.points.size() >= 3) {
      const bool got_shrink =
          std::fabs(signedArea2D(probe.points)) < area_base_ring;
      if (got_shrink != want_shrink) layer_clipper_side = -clipper_side;
    }
    deleteUnregisteredLayeredCurve(probe);
    prevabs::debug::segmentTracePush(
        std::string("layered closed: base_is_outer=")
        + (base_is_outer ? "yes" : "no") + ", layer_clipper_side="
        + std::to_string(layer_clipper_side));
  }

  prevabs::debug::segmentTracePush(
      std::string("layered: ") + (is_closed ? "closed" : "open")
      + " multi-layer route-ii (n_layers=" + std::to_string(n_layers) + ")");
  // Per-layer flow (note-build-laminate-segment.md §2.1): generate each layer
  // curve by offsetting the previous one, orient it like the base, then in the
  // split loop trim/extend its ends to the segment bounds, embed it, and tile
  // the band via the inner/outer staircase map. Base and shell may carry
  // different vertex counts (no 1:1 requirement) — the staircase emits tri/quad
  // cells exactly like the legacy area build.

  std::vector<LayeredCurve> curves;
  curves.reserve(n_layers + 1);
  curves.push_back(base_curve);

  // Per-layer boundary curves curves[1..n_layers-1]; the last layer's outer
  // curve is the total-thickness shell (reused → zero seam). Two methods, see
  // local/plan-20260629-layer-offset-method-dir-seq.md:
  //   "dir" (default): each boundary is the offset of the BASE by the CUMULATIVE
  //     thickness — a true parallel offset of the smooth base, no compounding
  //     drift, consistent with the DIR pre-check in validatePerLayerOffsets.
  //   "seq" (route-ii, note-build-laminate-segment.md §2.1.1): each boundary is
  //     the offset of the PREVIOUS layer curve by this layer's thickness. The
  //     sequential offset compounds discretization across layers; open-run
  //     resampling re-regularises every new layer at its input vertices.
  // Ends are NOT aligned here — they are trimmed/extended to the segment bounds
  // in the split loop below (§2.1.2), once the per-layer inner curve is fixed.
  const bool use_dir = (layerOffsetMethod() != "seq");
  double cumu_thk = 0.0;
  for (int k = 0; k < n_layers - 1; ++k) {
    const double tk = (layers[k].getLamina() != nullptr)
                          ? layers[k].getLamina()->getThickness()
                                * layers[k].getStack()
                          : 0.0;
    cumu_thk += tk;
    // DIR offsets the base by the cumulative thickness; SEQ offsets the previous
    // layer curve (curves[k]) by this layer's thickness. curves[1] is identical
    // either way (cumu == first thickness), so only deeper layers differ. Open
    // curves are resampled along their base-vertex angle bisectors; closed
    // curves remain raw because resampleOpenRuns handles open runs only.
    LayeredCurve c = use_dir
        ? makeLayerOffsetCurve(base_curve.points, is_closed, layer_clipper_side,
                               cumu_thk)
        : makeLayerOffsetCurve(curves[k].points, is_closed, layer_clipper_side,
                               tk);
    // No 1:1 vertex requirement: splitLayerBandIntoCells builds a staircase
    // correspondence between consecutive layer curves, so a layer curve with
    // fewer vertices than its inner curve (Clipper2 merged some — normal for a
    // dense curve offset by a non-trivial distance) still tiles correctly. The
    // only failure is a fully degenerate offset (< 2 vertices), which cannot
    // bound a band; release every curve we own and fall back. Still
    // pre-mutation (no split yet), so safe.
    if (c.vertices.size() < 2) {
      // Release every curve we own before bailing (still pre-mutation: no split
      // has run yet, so the shared DCEL is pristine either way).
      deleteUnregisteredLayeredCurve(c);
      for (auto& owned : curves) {
        deleteUnregisteredLayeredCurve(owned);
      }
      if (is_closed) {
        failClosedLayered(_name,
            "layer " + std::to_string(k + 1) + " raw offset is degenerate ("
            + std::to_string(c.vertices.size()) + " verts)");
      }
      PLOG(warning) << "layered offset[" << _name << "]: layer "
                    << (k + 1) << " raw offset is degenerate ("
                    << c.vertices.size()
                    << " verts); falling back to legacy area/layer build";
      prevabs::debug::segmentTracePush(
          "layered: layer " + std::to_string(k + 1) + " offset verts="
          + std::to_string(c.vertices.size())
          + " (degenerate) -> FALLBACK");
      return false;
    }
    // Keep every layer curve oriented like the base. makeLayerOffsetCurve can
    // return the offset polygon walked in the opposite direction; left
    // reversed, the NEXT offset (clipper_side is direction-relative) would go
    // to the wrong geometric side, and the begin/end cap correspondence with
    // the base/shell would connect opposite ends.
    if (is_closed) {
      // A closed ring also comes back with an arbitrary seam vertex; match the
      // base winding AND rotate the seam onto the base seam so the closed
      // staircase pairs index-for-index. (The open front/back heuristic below
      // cannot disentangle a combined winding+seam mismatch.)
      alignClosedRingToReference(c, base_curve.points);
      // Drop Clipper2 grid-noise duplicate vertices (~1e-8 apart) so the closed
      // staircase cannot pair a zero-area connector onto them (issue-20260702).
      dedupClosedRing(c);
    } else {
      // Open: reverse when the endpoints pair better with the base run flipped.
      const double d_aligned =
          c.points.front().distance(base_curve.points.front())
          + c.points.back().distance(base_curve.points.back());
      const double d_flipped =
          c.points.front().distance(base_curve.points.back())
          + c.points.back().distance(base_curve.points.front());
      if (d_flipped < d_aligned) {
        std::reverse(c.vertices.begin(), c.vertices.end());
        std::reverse(c.points.begin(), c.points.end());
      }
    }
    curves.push_back(std::move(c));
  }
  curves.push_back(shell_curve);
  // The total-thickness shell also comes from Clipper2 with its own seam; align
  // it to the base too so the FINAL layer's staircase (curves[last] vs shell)
  // pairs correctly, same as the intermediate rings.
  if (is_closed) {
    alignClosedRingToReference(curves.back(), base_curve.points);
    dedupClosedRing(curves.back());
  }

  // Closed multi-layer: the shell is a true annulus. Tile it from the OUTER
  // boundary inward, carving each concentric boundary ring out of the remaining
  // annulus (splitFaceByClosedCurve) and tiling the resulting band into radial
  // cells (splitLayerBandIntoCells, whose first connector bridges the band's two
  // loops). No caps, no artificial straight bounds. Closed is fail-fast — a
  // carved DCEL cannot be handed to legacy.
  //
  // Which ring is outer is not fixed: `curves` runs base -> offset, but the
  // material may grow inward (base is the outer ring) OR outward (the offset
  // grew larger, so base is the inner hole — e.g. a ccw circle with a right
  // layup). Build an explicit OUTER->INNER ring order and the material layer
  // each carved band maps to, so the same peel-from-outside loop serves both
  // nestings. splitFaceByClosedCurve always reuses the outer region as
  // parts.front(), so parts.front() is the outermost remaining band regardless.
  if (is_closed) {
    std::vector<const LayeredCurve *> radial;  // outer -> inner, size n_layers+1
    std::vector<int> band_layer;               // carved band -> material layer
    radial.reserve(n_layers + 1);
    band_layer.reserve(n_layers);
    if (base_is_outer) {
      // curves already outer(base) -> inner(offset); band k is layer k.
      for (int i = 0; i <= n_layers; ++i) radial.push_back(&curves[i]);
      for (int i = 0; i < n_layers; ++i) band_layer.push_back(i);
    } else {
      // offset is outer; reverse so radial runs outer(offset) -> inner(base).
      // The outermost band then carries the LAST material layer.
      for (int i = n_layers; i >= 0; --i) radial.push_back(&curves[i]);
      for (int i = 0; i < n_layers; ++i) band_layer.push_back(n_layers - 1 - i);
    }

    int closed_face_count = 0;
    PDCELFace *remaining = _face;
    for (int k = 0; k < n_layers; ++k) {
      const int li = band_layer[k];
      const double tk_k = layers[li].getLamina()->getThickness()
                          * layers[li].getStack();
      // Band bounded by radial[k] (outer loop) and radial[k+1] (inner hole).
      const LayeredCurve &band_outer = *radial[k];
      const LayeredCurve &band_inner = *radial[k + 1];

      PDCELFace *layer_band;
      if (k < n_layers - 1) {
        // Carve the inner boundary ring; the reused outer region is the band.
        std::list<PDCELFace *> parts =
            bcfg.dcel->splitFaceByClosedCurve(remaining, band_inner.vertices);
        if (parts.size() != 2) {
          failLayeredAfterMutation(
              _name, "failed to carve closed layer boundary "
                         + std::to_string(k + 1));
        }
        layer_band = parts.front();  // between radial[k], radial[k+1]
        remaining = parts.back();    // the rest, carved next round
      } else {
        // Innermost band is the leftover annulus — no carve needed.
        layer_band = remaining;
      }

      std::vector<PDCELFace *> cells = splitLayerBandIntoCells(
          layer_band, band_outer, band_inner, is_closed, side, tk_k,
          bcfg.dcel, _name);
      if (cells.empty()) {
        failLayeredAfterMutation(
            _name, "failed to tile closed layer " + std::to_string(li + 1));
      }
      for (std::size_t i = 0; i < cells.size(); ++i) {
        const std::string face_name =
            _name + "_layer_" + std::to_string(li + 1)
                  + "_face_" + std::to_string(i + 1);
        if (!assignLayeredFaceProperties(
                cells[i], face_name, layers[li], band_outer, band_inner,
                _curve_base, is_closed, _mat_orient_e1, _mat_orient_e2, bcfg)) {
          failLayeredAfterMutation(
              _name, "failed to assign closed layer " + std::to_string(li + 1)
                         + " face properties");
        }
        ++closed_face_count;
        _layered_faces.push_back(cells[i]);
      }
      if (k == n_layers - 1) _face = cells.back();
    }

    for (auto &owned : curves) {
      deleteUnregisteredLayeredCurve(owned);
    }
    _state = LifecycleState::AreasBuilt;
    prevabs::debug::segmentTracePush(
        "layered: tiled all closed layers (SUCCESS, faces="
        + std::to_string(closed_face_count) + ")");
    PLOG(info) << "built closed layered offset segment " << _name
               << ": layers=" << n_layers << ", faces=" << closed_face_count;
    return true;
  }

  int face_count = 0;
  PDCELFace *remaining_face = _face;
  // Tracks whether any splitFaceByPolyline below has already mutated the
  // shared DCEL; once true a failure can no longer fall back to legacy safely.
  bool dcel_mutated = false;
  for (int k = 0; k < n_layers - 1; ++k) {
    // §2.1.2: trim/extend this layer boundary curve so its two ends land on the
    // segment bounds. The remaining face is bounded by curves[k] (inner, already
    // trimmed in the previous iteration; base_curve for k==0) and shell_curve
    // (outer), so those are the begin/end cap lines. This removes any miter
    // overshoot/reversal cusp at the bounds before the curve is embedded.
    const double tk_k = layers[k].getLamina()->getThickness()
                        * layers[k].getStack();
    if (!is_closed) {
      trimLayerCurveEndsToCaps(
          curves[k + 1], curves[k], shell_curve, tk_k, _name);
    }
    std::list<PDCELFace *> split_faces =
        bcfg.dcel->splitFaceByPolyline(
            remaining_face, curves[k + 1].vertices);
    if (split_faces.size() != 2) {
      // Once an earlier split mutated the shared DCEL a legacy fallback would
      // corrupt the mesh, so that case is a hard failure. Before any mutation
      // the split simply isn't viable on this geometry; that is a recoverable
      // routing decision (the legacy area build below yields a correct
      // result), so warn — matching the other layered fall-backs — rather
      // than error.
      if (dcel_mutated) {
        failLayeredAfterMutation(
            _name, "splitFaceByPolyline failed at layer boundary "
                       + std::to_string(k + 1));
      }
      PLOG(warning) << "layered offset[" << _name
                    << "]: cannot split layer boundary " << (k + 1)
                    << "; falling back to legacy area/layer build";
      prevabs::debug::segmentTracePush(
          "layered: splitFaceByPolyline FAILED @layer boundary "
          + std::to_string(k + 1) + " -> FALLBACK (clean: no mutation yet)");
      return false;
    }
    // splitFaceByPolyline succeeded above: the shared DCEL is now mutated.
    dcel_mutated = true;

    PDCELFace *f0 = split_faces.front();
    PDCELFace *f1 = split_faces.back();
    PDCELFace *layer_face = nullptr;
    PDCELFace *next_remaining = nullptr;

    // Discriminate the layer band from the outer remainder robustly: only the
    // band carries curves[k] (the remainder is bounded by curves[k+1] and the
    // shell). Probe an interior vertex of curves[k] — the centroid-distance
    // heuristic is unreliable for sharp/V shapes where both strips span the
    // full segment height and share a near-identical centroid.
    if (curves[k].points.size() > 2) {
      const SPoint2 probe = curves[k].points[curves[k].points.size() / 2];
      const double tol = GEO_MERGE_TOL;
      if (faceBoundaryHasPoint(f0, probe, tol)) {
        layer_face = f0; next_remaining = f1;
      } else if (faceBoundaryHasPoint(f1, probe, tol)) {
        layer_face = f1; next_remaining = f0;
      }
    }
    if (layer_face == nullptr) {
      const double d0 =
          faceCentroidDistanceToCurveForLayered(f0, curves[k], false);
      const double d1 =
          faceCentroidDistanceToCurveForLayered(f1, curves[k], false);
      layer_face = (d0 <= d1) ? f0 : f1;
      next_remaining = (layer_face == f0) ? f1 : f0;
    }

    std::vector<PDCELFace *> layer_cells = splitLayerBandIntoCells(
        layer_face, curves[k], curves[k + 1], is_closed, side, tk_k,
        bcfg.dcel, _name);
    if (layer_cells.empty()) {
      PLOG(error) << "layered offset[" << _name
                  << "]: failed to split layer " << (k + 1)
                  << " into cells";
      prevabs::debug::segmentTracePush(
          "layered: splitLayerBandIntoCells FAILED @layer " + std::to_string(k + 1)
          + " -> ABORT (DCEL already mutated, cannot fall back)");
      failLayeredAfterMutation(
          _name, "failed to split layer " + std::to_string(k + 1)
                     + " into cells");
    }
    for (std::size_t i = 0; i < layer_cells.size(); ++i) {
      const std::string face_name =
          _name + "_layer_" + std::to_string(k + 1)
                + "_face_" + std::to_string(i + 1);
      if (!assignLayeredFaceProperties(
              layer_cells[i], face_name, layers[k],
              curves[k], curves[k + 1], _curve_base, is_closed,
              _mat_orient_e1, _mat_orient_e2, bcfg)) {
        PLOG(error) << "layered offset[" << _name
                    << "]: failed to assign layer face properties for layer "
                    << (k + 1) << ", face " << (i + 1);
        failLayeredAfterMutation(
            _name, "failed to assign layer " + std::to_string(k + 1)
                       + " face properties");
      }
      ++face_count;
      _layered_faces.push_back(layer_cells[i]);
    }

    remaining_face = next_remaining;
    _face = remaining_face;

    PLOG(debug) << "layered offset[" << _name
                << "]: split boundary " << (k + 1)
                << ", layer_face=" << faceLabel(layer_cells.front(), bcfg.model)
                << ", remaining_face="
                << faceLabel(remaining_face, bcfg.model);
  }

  const int last = n_layers - 1;
  const double tk_last = layers[last].getLamina()->getThickness()
                         * layers[last].getStack();

  // Final layer: its outer boundary is the shell (segment offset), reused
  // verbatim — NOT re-offset. Resample the shell 1:1 against curves[last] so
  // the staircase cannot outer-stall (issue-20260628 §8.3). Open route-ii only:
  // a closed segment tiles a loop (no begin/end caps) and does not hit the
  // open-head shell stall, so it keeps the raw shell path unchanged.
  LayeredCurve final_outer;
  BaseOffsetMap final_map;
  const LayeredCurve* outer_for_final = &curves[last + 1];
  const BaseOffsetMap* final_map_ptr = nullptr;
  if (!is_closed) {
    final_outer = makeArcResampledOuterCurve(curves[last], curves[last + 1]);
    if (final_outer.vertices.size() == curves[last].vertices.size()
        && final_outer.vertices.size() >= 2) {
      final_map.reserve(final_outer.vertices.size());
      for (int i = 0; i < static_cast<int>(final_outer.vertices.size()); ++i) {
        final_map.push_back(BaseOffsetPair(i, i));
      }
      outer_for_final = &final_outer;
      final_map_ptr = &final_map;
    }
  }

  std::vector<PDCELFace *> final_cells = splitLayerBandIntoCells(
      remaining_face, curves[last], *outer_for_final, is_closed, side,
      tk_last, bcfg.dcel, _name, final_map_ptr);
  if (final_cells.empty()) {
    PLOG(error) << "layered offset[" << _name
                << "]: failed to split final layer into cells";
    prevabs::debug::segmentTracePush(
        "layered: splitLayerBandIntoCells FAILED @final layer "
        + std::to_string(n_layers) + " -> ABORT (DCEL already mutated, cannot fall back)");
    failLayeredAfterMutation(
        _name, "failed to split final layer into cells");
  }
  for (std::size_t i = 0; i < final_cells.size(); ++i) {
    const std::string face_name =
        _name + "_layer_" + std::to_string(n_layers)
              + "_face_" + std::to_string(i + 1);
    if (!assignLayeredFaceProperties(
            final_cells[i], face_name, layers[last],
            curves[last], *outer_for_final,
            _curve_base, is_closed, _mat_orient_e1, _mat_orient_e2, bcfg)) {
      PLOG(error) << "layered offset[" << _name
                  << "]: failed to assign final layer face properties";
      failLayeredAfterMutation(
          _name, "failed to assign final layer face properties");
    }
    ++face_count;
    _layered_faces.push_back(final_cells[i]);
  }
  _face = final_cells.back();

  for (auto& owned : curves) {
    deleteUnregisteredLayeredCurve(owned);
  }
  // The arc-resampled final outer owns its NEW interior vertices; any that were
  // not consumed by a split (none, in the success path) are unregistered and
  // freed here. The reused shell corner endpoints are registered and survive.
  deleteUnregisteredLayeredCurve(final_outer);
  _state = LifecycleState::AreasBuilt;
  prevabs::debug::segmentTracePush(
      "layered: tiled all layers (SUCCESS, faces=" + std::to_string(face_count)
      + ")");
  PLOG(info) << "built layered offset segment " << _name
             << ": layers=" << n_layers
             << ", faces=" << face_count;
  return true;
}


// Phase-2a: route-i per-layer offset validation (no DCEL effect). See header.
void Segment::validatePerLayerOffsets(const BuilderConfig &bcfg) {
  if (_layup == nullptr || _curve_base == nullptr) return;
  // Non-const copy: Layer/Lamina accessors (getLamina/getStack/getThickness)
  // are non-const member functions.
  std::vector<Layer> layers = _layup->getLayers();
  const int n = static_cast<int>(layers.size());
  if (n == 0) return;

  // Base in SPoint2 (drop trailing duplicate on a closed Baseline).
  std::vector<SPoint2> base;
  base.reserve(_curve_base->vertices().size());
  for (auto* v : _curve_base->vertices()) {
    base.emplace_back(v->point2()[0], v->point2()[1]);
  }
  const bool is_closed = closed();
  if (is_closed && base.size() >= 2
      && _curve_base->vertices().front() == _curve_base->vertices().back()) {
    base.pop_back();
  }
  if (base.size() < 2) return;

  const int side = layupSide();
  const double total = _layup->getTotalThickness();
  if (side == 0 || !(total > 0.0)) return;
  // Match offset()'s side convention: closed flips, open passes through.
  const int clipper_side = is_closed ? -side : side;

  const std::vector<SPoint2> shell =
      offsetCurveSeg(base, is_closed, clipper_side, total);
  if (shell.size() < 2) {
    PLOG(warning) << "per-layer[" << _name << "]: shell offset empty; skip";
    return;
  }

  std::vector<SPoint2> prev = base;
  std::vector<SPoint2> raw_last;
  double cum = 0.0, prev_mean = -1.0;
  bool nesting_ok = true, maps_ok = true;
  // Geometric-quality signals (the topological checks above are necessary
  // but NOT sufficient — they pass on thin segments that still fail to mesh,
  // cf. summary-20260520 "valid staircase != geometrically stable"). Track
  // the worst per-layer vertex ratio M_k/N and the total dropped base span:
  // these expose the thin-region collapse the exit checks miss.
  const int N = static_cast<int>(base.size());
  double min_m_ratio = 1.0;
  int total_dropped = 0;
  for (int k = 0; k < n; ++k) {
    const double tk = (layers[k].getLamina() != nullptr)
                          ? layers[k].getLamina()->getThickness()
                                * layers[k].getStack()
                          : 0.0;
    cum += tk;
    const std::vector<SPoint2> curve =
        offsetCurveSeg(base, is_closed, clipper_side, cum);
    if (curve.size() < 2) { maps_ok = false; break; }
    raw_last = curve;

    const auto plan = prevabs::geo::rebuildBaseOffsetMapFromGeometry(
        prev, curve, is_closed, side, std::max(tk, 1e-12),
        prevabs::geo::readPairingAlgoEnv());
    if (!plan.ok) maps_ok = false;

    min_m_ratio = std::min(min_m_ratio,
                           static_cast<double>(curve.size()) / N);
    for (std::size_t r = 0; r < plan.dropped_base_ranges_lo.size(); ++r) {
      total_dropped += plan.dropped_base_ranges_hi[r]
                       - plan.dropped_base_ranges_lo[r] + 1;
    }

    double dmax = 0.0, dsum = 0.0;
    for (const auto& p : curve) {
      const double d = footDistToPolylineSeg(p, base, is_closed);
      dmax = std::max(dmax, d);
      dsum += d;
    }
    const double dmean = dsum / static_cast<double>(curve.size());
    const bool within = (k == n - 1) || (dmax < total + GEO_NESTING_SLACK);
    if (!within || dmean < prev_mean - GEO_NESTING_SLACK) nesting_ok = false;
    prev_mean = dmean;
    prev = plan.ok ? plan.offset_points : curve;
  }

  // zero-gap: route-i cum_n curve must equal the shell (same offset call).
  double gap = std::numeric_limits<double>::infinity();
  if (raw_last.size() == shell.size()) {
    gap = 0.0;
    for (std::size_t i = 0; i < shell.size(); ++i) {
      gap = std::max(gap, std::hypot(raw_last[i].x() - shell[i].x(),
                                     raw_last[i].y() - shell[i].y()));
    }
  }
  const bool zero_gap = gap < GEO_NESTING_SLACK;
  const bool topo_pass = nesting_ok && zero_gap && maps_ok;
  // Geometric-quality flag: thin collapse (worst layer M_k/N below the
  // empirical mesh-able threshold ~0.7, cf. issue-20260521-mh104) or any
  // dropped base span. PASS-but-thin is the Phase-3 hard case.
  const bool geom_ok = (min_m_ratio >= 0.7) && (total_dropped == 0);
  PLOG(info) << "per-layer[" << _name << "]: "
             << (is_closed ? "closed" : "open") << " N=" << base.size()
             << " layers=" << n
             << " minM/N=" << min_m_ratio << " dropped=" << total_dropped
             << " nesting=" << (nesting_ok ? "Y" : "N")
             << " zero_gap=" << (zero_gap ? "Y" : "N")
             << " maps=" << (maps_ok ? "Y" : "N")
             << " geom=" << (geom_ok ? "OK" : "THIN")
             << " -> " << (topo_pass ? "PASS" : "FAIL")
             << (topo_pass && !geom_ok ? " (PASS-but-thin)" : "");
}


void Segment::buildAreas(const BuilderConfig &bcfg) {
  PLogContext segment_areas_context("segment areas: " + _name);
  PLogSection segment_areas_section(
      DebugLevel::join, "segment areas", _name);
  if (!requireExactState(LifecycleState::ShellBuilt, "buildAreas")) {
    logSkippingSegmentAreasAction(
        "buildAreas", _name, "segment is not in ShellBuilt state");
    return;
  }
  if (!validateStateInvariants("buildAreas")) {
    logSkippingSegmentAreasAction(
        "buildAreas", _name, "lifecycle invariants validation failed");
    return;
  }

    if (bcfg.debug_level >= DebugLevel::join) PLOG(debug) << "building areas of segment: " + _name;

  // Build-path trace: this is the area phase (appends to the per-segment file
  // started by offsetCurveBase). The scope is the root frame of this phase;
  // everything below nests under it. See debug/segmentBuildDump.hpp.
  prevabs::debug::segmentTraceBegin(_name);
  prevabs::debug::SegmentTraceScope _trace_scope(
      std::string("buildAreas (closed=") + (closed() ? "Y" : "N")
      + ", base_verts="
      + std::to_string(_curve_base ? _curve_base->vertices().size() : 0) + ")");

  // plan-20260522: re-derive the staircase from the current base/offset
  // geometry. The persisted `_base_offset_indices_pairs` is treated as a
  // geometric view, not as independent state — any prior join-time
  // mutation of the curves is reflected by recomputing here from the
  // present polylines, so no editor-side discrete splice is needed.
  // See rebuildBaseOffsetMapFromGeometry in include/offset_clipper2.hpp.
  if (_adaptive_variable_offset) {
    if (bcfg.debug_level >= DebugLevel::join) {
      PLOG(debug) << "buildAreas: keeping adaptive variable-offset "
                  << "BaseOffsetMap for segment '" << _name << "'";
    }
  } else if (_curve_base != nullptr && _curve_offset != nullptr) {
    std::vector<SPoint2> base_pts;
    base_pts.reserve(_curve_base->vertices().size());
    for (auto *v : _curve_base->vertices()) {
      base_pts.emplace_back(v->point2()[0], v->point2()[1]);
    }
    // Closed Baseline convention has front == back as the same pointer;
    // drop the trailing duplicate so the SPoint2 helper sees N distinct
    // vertices, matching the contract of planReverseMatchByNearest.
    if (closed() && base_pts.size() >= 2
        && _curve_base->vertices().front()
             == _curve_base->vertices().back()) {
      base_pts.pop_back();
    }
    std::vector<SPoint2> off_pts;
    off_pts.reserve(_curve_offset->vertices().size());
    for (auto *v : _curve_offset->vertices()) {
      off_pts.emplace_back(v->point2()[0], v->point2()[1]);
    }
    const int side = requireValidLayupSide("buildAreas/derive");
    const double dist =
        (_layup != nullptr) ? _layup->getTotalThickness() : 0.0;
    if (side != 0 && dist > 0.0 && base_pts.size() >= 2
        && off_pts.size() >= 2) {
      const auto plan = prevabs::geo::rebuildBaseOffsetMapFromGeometry(
          base_pts, off_pts, closed(), side, dist,
          prevabs::geo::readPairingAlgoEnv());
      if (plan.ok) {
        if (bcfg.debug_level >= DebugLevel::join) {
          PLOG(debug) << "buildAreas: derived staircase: "
                      << _base_offset_indices_pairs.size() << " pairs (was) -> "
                      << plan.id_pairs.size() << " pairs (derived); "
                      << "dropped ranges (was)="
                      << _dropped_base_ranges_lo.size()
                      << ", (derived)=" << plan.dropped_base_ranges_lo.size();
        }
        _base_offset_indices_pairs = plan.id_pairs;
        _dropped_base_ranges_lo    = plan.dropped_base_ranges_lo;
        _dropped_base_ranges_hi    = plan.dropped_base_ranges_hi;
      } else {
        PLOG(warning) << "buildAreas: rebuildBaseOffsetMapFromGeometry "
                         "failed for segment '" << _name
                      << "' — keeping persisted staircase";
      }
    }
  }

  // Phase-1 (plan-20260618-per-layer-offset-within-shell.md): emit the
  // base-offset-map debug dump here (moved from `offsetCurveBase`), now that
  // `_base_offset_indices_pairs` is the authoritative geometry-derived
  // staircase actually used for area construction. `--debug geo` only.
  if (bcfg.debug_level >= DebugLevel::geo
      && _curve_base != nullptr && _curve_offset != nullptr) {
    const std::string svg_path = config.file_directory + config.file_base_name
                               + "." + _name + ".base_offset_map.svg";
    dumpBaseOffsetMapSvg(svg_path, _name,
                         _curve_base->vertices(),
                         _curve_offset->vertices(),
                         _base_offset_indices_pairs,
                         &_offset_vertex_resampled,
                         &_offset_pre_resample_raw_points);
    const std::string json_path = config.file_directory + config.file_base_name
                                + "." + _name + ".base_offset_map.json";
    const double dist =
        (_layup != nullptr) ? _layup->getTotalThickness() : 0.0;
    dumpBaseOffsetMapJson(json_path, _name,
                          _curve_base->vertices(),
                          _curve_offset->vertices(),
                          _base_offset_indices_pairs,
                          closed(),
                          requireValidLayupSide("buildAreas/json"),
                          dist,
                          &_dropped_base_ranges_lo,
                          &_dropped_base_ranges_hi,
                          _used_adaptive_thickness ? &_adaptive_plan : nullptr);
  }

  // Phase-2a: validate route-i per-layer offsets in the production context
  // (flag-gated). Phase-2b then tries the direct layered-offset face build;
  // unsupported healthy-subset gaps fall back to the legacy path below.
  if (useLayeredOffset()) {
    prevabs::debug::segmentTracePush("useLayeredOffset=yes");
    validatePerLayerOffsets(bcfg);
    if (buildLayeredOffsetAreas(bcfg)) {
      validateStateInvariants("buildAreas/layered-offset");
      prevabs::debug::segmentTracePush("layered OK -> AreasBuilt");
      prevabs::debug::dumpSegmentBuild(*this, bcfg);
      segment_areas_section.setEndDetails(
          "areas=0, layers=" + std::to_string(layerCount()));
      return;
    }
    prevabs::debug::segmentTracePush("layered FELL BACK -> legacy");
  } else {
    prevabs::debug::segmentTracePush("useLayeredOffset=no -> legacy");
  }

  std::vector<PDCELVertex *> first_bound_vertices;
  std::vector<PDCELVertex *> prev_bound_vertices =
      buildBeginningBound(first_bound_vertices, bcfg);

  int count = 0;
  createIntermediateAreas(prev_bound_vertices, count, bcfg);

  buildLastArea(prev_bound_vertices, first_bound_vertices, count, bcfg);

  for (auto &each_area : _areas) {
    each_area->buildLayers(bcfg);
  }
  _state = LifecycleState::AreasBuilt;
  validateStateInvariants("buildAreas");
  prevabs::debug::segmentTracePush(
      "legacy: built areas=" + std::to_string(_areas.size())
      + " -> AreasBuilt");
  prevabs::debug::dumpSegmentBuild(*this, bcfg);
  segment_areas_section.setEndDetails(
      "areas=" + std::to_string(_areas.size())
      + ", layers=" + std::to_string(layerCount()));
}
