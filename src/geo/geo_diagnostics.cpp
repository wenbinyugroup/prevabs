#include "geo_diagnostics.hpp"

#include "PDCELVertex.hpp"
#include "PGeoClasses.hpp"
#include "globalConstants.hpp"
#include "overloadOperator.hpp"
#include "plog.hpp"
#include "pui.hpp"

#include "geo_types.hpp"

#include <cmath>
#include <sstream>
#include <string>

using namespace dcel;  // phase 0: DCEL types moved to namespace dcel

namespace prevabs {
namespace geo {

// Local half-thickness at base[i] for a closed/open polyline: project a ray
// from base[i] along the inward normal (determined by `side`, matching the
// single-vertex offset() cross-product convention), find the nearest
// non-adjacent base segment it hits, and return that distance. Returns +INF
// when the inward ray never re-enters the polygon (the offset has plenty of
// room locally).
//
// Pre: closed → N >= 4 vertices (distinct count N-1 >= 3); open → interior i.
double signedHalfThickness(
    const std::vector<PDCELVertex *> &base, int i, int side,
    bool base_is_closed) {
  // Distinct base vertex count:
  //   closed → drop trailing duplicate (front == back pointer).
  //   open   → all entries are distinct.
  const int n_distinct = base_is_closed
                           ? static_cast<int>(base.size()) - 1
                           : static_cast<int>(base.size());
  if (n_distinct < 3) return INF;

  // Endpoint vertices on an open base have only one neighbour, so a
  // half-thickness measurement is geometrically undefined (no opposite
  // side to project onto). Stage E callers should iterate interior
  // vertices only; this guard is defensive.
  if (!base_is_closed && (i <= 0 || i >= n_distinct - 1)) return INF;

  const int i_prev = base_is_closed
                       ? (i - 1 + n_distinct) % n_distinct
                       : i - 1;
  const int i_next = base_is_closed
                       ? (i + 1) % n_distinct
                       : i + 1;

  const SPoint2 p_i    = base[i]     ->point2();
  const SPoint2 p_prev = base[i_prev]->point2();
  const SPoint2 p_next = base[i_next]->point2();

  // Local tangent: average of incoming + outgoing edge tangents at
  // base[i]. Using (p_next - p_prev) gives this average direction in
  // one subtraction and is robust at near-cusp vertices.
  const double tx = p_next.x() - p_prev.x();
  const double ty = p_next.y() - p_prev.y();
  const double t_norm = std::sqrt(tx * tx + ty * ty);
  if (t_norm < TOLERANCE) return INF;
  const double tx_n = tx / t_norm;
  const double ty_n = ty / t_norm;

  // Inward normal in the yz plane (n × t with n = SVector3(side, 0, 0)).
  const double dir_x = -side * ty_n;
  const double dir_y =  side * tx_n;

  // Intersect ray (p_i + t * dir) with each non-adjacent segment.
  const int n_seg = base_is_closed ? n_distinct : n_distinct - 1;
  double min_t = INF;
  for (int j = 0; j < n_seg; ++j) {
    // Skip segments that touch base[i].
    if (j == i || j == i_prev) continue;

    const int j_next = base_is_closed ? (j + 1) % n_distinct : j + 1;
    const SPoint2 a = base[j]     ->point2();
    const SPoint2 b = base[j_next]->point2();

    const double ex = b.x() - a.x();
    const double ey = b.y() - a.y();

    // Solve  p_i + t * dir = a + s * (b - a).
    const double det = -dir_x * ey + dir_y * ex;
    if (std::fabs(det) < TOLERANCE) continue;  // parallel / collinear

    const double rx = a.x() - p_i.x();
    const double ry = a.y() - p_i.y();
    const double t = (-rx * ey + ry * ex) / det;
    const double s = (-dir_x * ry + dir_y * rx) / -det;
    if (t > TOLERANCE && s >= -TOLERANCE && s <= 1.0 + TOLERANCE) {
      if (t < min_t) min_t = t;
    }
  }
  return min_t;
}

void checkOffsetDistanceVsShortestEdge(
    const std::vector<PDCELVertex *> &base, double dist) {
  const std::size_t size = base.size();
  if (size < 2) return;

  double L_min     = base[0]->point().distance(base[1]->point());
  int    L_min_seg = 0;
  for (std::size_t i = 1; i + 1 < size; ++i) {
    const double L = base[i]->point().distance(base[i + 1]->point());
    if (L < L_min) {
      L_min     = L;
      L_min_seg = static_cast<int>(i);
    }
  }
  const double abs_dist = std::fabs(dist);
  if (L_min > 0.0 && abs_dist > 0.5 * L_min) {
    std::ostringstream oss;
    oss.precision(6);
    oss << "offset (multi-vertex): offset distance " << abs_dist
        << " is " << (abs_dist / L_min) << "x the shortest base segment "
        << "length (" << L_min
        << " at segment " << L_min_seg
        << "); offset construction may be numerically fragile — "
        << "consider <normalize>1</normalize>, a larger <scale>, "
        << "or a denser baseline";
    PLOG(warning) << oss.str();
  }
}

void warnLocalThinRegions(const std::vector<PDCELVertex *> &base,
                          int side, double dist, bool base_is_closed) {
  const std::size_t size = base.size();
  const int n_distinct = base_is_closed
                           ? static_cast<int>(size) - 1
                           : static_cast<int>(size);
  const double abs_dist   = std::fabs(dist);
  const char* const kind  = base_is_closed ? "closed" : "open";
  int n_below_dist   = 0;
  int n_below_2dist  = 0;
  int first_below    = -1;
  for (int i = 0; i < n_distinct; ++i) {
    const double h = signedHalfThickness(base, i, side, base_is_closed);
    if (h < abs_dist - TOLERANCE) {
      ++n_below_dist;
      if (first_below < 0) first_below = i;
    } else if (h < 2.0 * abs_dist - TOLERANCE) {
      ++n_below_2dist;
    }
  }
  if (n_below_dist > 0) {
    PLOG(warning)
        << "offset (multi-vertex, " << kind << "): " << n_below_dist
        << " base vertex/vertices have local half-thickness < |dist| = "
        << abs_dist << " (first at base[" << first_below << "] = "
        << base[first_below]->printString()
        << "); skin will be locally dropped at those locations";
  }
  if (n_below_2dist > 0) {
    PLOG(warning)
        << "offset (multi-vertex, " << kind << "): " << n_below_2dist
        << " base vertex/vertices have local half-thickness "
        << "in [|dist|, 2*|dist|) = [" << abs_dist << ", "
        << (2.0 * abs_dist)
        << "); offset will be valid but locally thin";
  }
}

void warnLowOffsetMNRatio(std::size_t base_size, std::size_t offset_size,
                          bool base_is_closed, double dist) {
  const std::size_t n_base_distinct =
      base_is_closed ? base_size - 1 : base_size;
  const std::size_t n_off_distinct =
      base_is_closed ? offset_size - 1 : offset_size;
  constexpr double kMNRatioWarn = 0.7;
  if (n_base_distinct > 0
      && static_cast<double>(n_off_distinct)
           < kMNRatioWarn * static_cast<double>(n_base_distinct)) {
    const double ratio =
        static_cast<double>(n_off_distinct)
        / static_cast<double>(n_base_distinct);
    std::ostringstream oss;
    oss.precision(2);
    oss << std::fixed
        << "offset (multi-vertex, "
        << (base_is_closed ? "closed" : "open")
        << "): only " << n_off_distinct
        << " offset verts produced for " << n_base_distinct
        << " base verts (M/N=" << ratio << "). The layup half-thickness "
        << std::fabs(dist)
        << " likely exceeds the local base curvature radius along part "
           "of the curve — Clipper2 has merged base corners during inset. "
           "Downstream mesh recovery typically fails below M/N=0.7. "
           "Consider reducing layup thickness, splitting the layup over a "
           "denser baseline, or excluding the thin region from the layup.";
    PUI_WARN << oss.str();
    PLOG(warning) << oss.str();
  }
}

}  // namespace geo
}  // namespace prevabs
