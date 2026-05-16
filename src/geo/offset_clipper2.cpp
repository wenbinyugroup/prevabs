#include "offset_clipper2.hpp"

#include "clipper2/clipper.h"

#include <cmath>
#include <limits>
#include <utility>

namespace prevabs {
namespace geo {

namespace {

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
    double                      dist) {

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

  // PreVABS side convention: +1 = outward (CCW interior), -1 = inward.
  // Clipper2 limitation: open paths can only be inflated with delta > 0
  // (EndType::Butt returns a surrounding polygon, no "side" semantic).
  // For open inputs we therefore ignore `side` and use +|dist|; callers
  // that need a one-sided offset of an open polyline should split the
  // Butt rectangle themselves (or use the single-segment fast path in
  // `offsetLineSegment` for the 2-vertex case).
  const double delta = base_is_closed
                         ? (side >= 0 ? +1.0 : -1.0) * dist
                         : dist;
  const Clipper2Lib::JoinType jt = Clipper2Lib::JoinType::Miter;
  const Clipper2Lib::EndType  et = base_is_closed
                                     ? Clipper2Lib::EndType::Polygon
                                     : Clipper2Lib::EndType::Butt;

  // precision = 8 is the Clipper2 1.4.0 hard ceiling — see
  // local/issue-20260515-clipper2-airfoil-a0.md §3.1.
  // miter_limit = 2.0 matches the legacy kOffsetMiterLimitFactor.
  constexpr int    kClipperPrecision = 8;
  constexpr double kMiterLimit       = 2.0;
  const Clipper2Lib::PathsD sol = Clipper2Lib::InflatePaths(
      subjs, delta, jt, et, kMiterLimit, kClipperPrecision);

  // Annotate each output vertex with its source segment (if any).
  const double source_radius = 1.5 * dist;
  result.reserve(sol.size());
  for (const auto& path : sol) {
    OffsetPolygon op;
    // Clipper2's InflatePaths *always* returns closed PathsD regardless
    // of EndType (Butt/Square/Round just cap the open ends; the result
    // is still a closed perimeter loop).
    op.is_closed = true;
    op.points.reserve(path.size());
    op.sources.reserve(path.size());
    for (const auto& p : path) {
      op.points.emplace_back(p.x, p.y);
      op.sources.push_back(
          attributeSource(p.x, p.y, input, base_is_closed, source_radius));
    }
    result.push_back(std::move(op));
  }
  return result;
}

}  // namespace geo
}  // namespace prevabs
