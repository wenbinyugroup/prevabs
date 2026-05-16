#include "geom_assertions.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace clipper2_airfoil {

double signedArea(const Clipper2Lib::PathD& path) {
  if (path.size() < 3) return 0.0;
  double a = 0.0;
  const std::size_t n = path.size();
  for (std::size_t i = 0; i < n; ++i) {
    const auto& p0 = path[i];
    const auto& p1 = path[(i + 1) % n];
    a += p0.x * p1.y - p1.x * p0.y;
  }
  return 0.5 * a;
}

double totalAbsArea(const Clipper2Lib::PathsD& paths) {
  double sum = 0.0;
  for (const auto& p : paths) sum += std::fabs(signedArea(p));
  return sum;
}

double largestAbsArea(const Clipper2Lib::PathsD& paths) {
  double m = 0.0;
  for (const auto& p : paths) m = std::max(m, std::fabs(signedArea(p)));
  return m;
}

namespace {

// Returns true if segments p0-p1 and p2-p3 cross, **exclusive** of any
// shared endpoint. We use a strict version of the standard
// signed-area orientation test.
bool segmentsCross(double p0x, double p0y, double p1x, double p1y,
                   double p2x, double p2y, double p3x, double p3y) {
  auto cross = [](double ax, double ay, double bx, double by,
                  double cx, double cy) {
    return (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
  };
  const double d1 = cross(p2x, p2y, p3x, p3y, p0x, p0y);
  const double d2 = cross(p2x, p2y, p3x, p3y, p1x, p1y);
  const double d3 = cross(p0x, p0y, p1x, p1y, p2x, p2y);
  const double d4 = cross(p0x, p0y, p1x, p1y, p3x, p3y);
  // Strict opposite signs on both pairs => proper crossing.
  if (((d1 > 0 && d2 < 0) || (d1 < 0 && d2 > 0))
   && ((d3 > 0 && d4 < 0) || (d3 < 0 && d4 > 0))) {
    return true;
  }
  return false;
}

}  // namespace

int countSelfIntersections(const Clipper2Lib::PathD& path) {
  const std::size_t n = path.size();
  if (n < 4) return 0;
  int count = 0;
  for (std::size_t i = 0; i < n; ++i) {
    const auto& a0 = path[i];
    const auto& a1 = path[(i + 1) % n];
    for (std::size_t j = i + 2; j < n; ++j) {
      // Skip the segment adjacent to i across the closing seam.
      if (i == 0 && j == n - 1) continue;
      const auto& b0 = path[j];
      const auto& b1 = path[(j + 1) % n];
      if (segmentsCross(a0.x, a0.y, a1.x, a1.y,
                        b0.x, b0.y, b1.x, b1.y)) {
        ++count;
      }
    }
  }
  return count;
}

bool isClosedByCoincidence(const Clipper2Lib::PathD& path, double tol) {
  if (path.size() < 2) return false;
  const auto& f = path.front();
  const auto& b = path.back();
  return std::fabs(f.x - b.x) <= tol && std::fabs(f.y - b.y) <= tol;
}

Bbox bboxOf(const Clipper2Lib::PathD& path) {
  Bbox bb{std::numeric_limits<double>::infinity(),
          std::numeric_limits<double>::infinity(),
          -std::numeric_limits<double>::infinity(),
          -std::numeric_limits<double>::infinity()};
  for (const auto& p : path) {
    bb.xmin = std::min(bb.xmin, p.x);
    bb.ymin = std::min(bb.ymin, p.y);
    bb.xmax = std::max(bb.xmax, p.x);
    bb.ymax = std::max(bb.ymax, p.y);
  }
  return bb;
}

Bbox bboxOf(const Clipper2Lib::PathsD& paths) {
  Bbox bb{std::numeric_limits<double>::infinity(),
          std::numeric_limits<double>::infinity(),
          -std::numeric_limits<double>::infinity(),
          -std::numeric_limits<double>::infinity()};
  for (const auto& path : paths) {
    Bbox sub = bboxOf(path);
    bb.xmin = std::min(bb.xmin, sub.xmin);
    bb.ymin = std::min(bb.ymin, sub.ymin);
    bb.xmax = std::max(bb.xmax, sub.xmax);
    bb.ymax = std::max(bb.ymax, sub.ymax);
  }
  return bb;
}

double minDistanceToContour(double x, double y,
                            const Clipper2Lib::PathD& contour) {
  const std::size_t n = contour.size();
  if (n < 2) return std::numeric_limits<double>::infinity();
  double best = std::numeric_limits<double>::infinity();
  for (std::size_t i = 0; i < n; ++i) {
    const auto& a = contour[i];
    const auto& b = contour[(i + 1) % n];
    const double dx = b.x - a.x;
    const double dy = b.y - a.y;
    const double len2 = dx * dx + dy * dy;
    double u = 0.0;
    if (len2 > 0.0) {
      u = ((x - a.x) * dx + (y - a.y) * dy) / len2;
      if (u < 0.0) u = 0.0;
      if (u > 1.0) u = 1.0;
    }
    const double fx = a.x + u * dx;
    const double fy = a.y + u * dy;
    const double rx = x - fx;
    const double ry = y - fy;
    const double d = std::sqrt(rx * rx + ry * ry);
    if (d < best) best = d;
  }
  return best;
}

}  // namespace clipper2_airfoil
