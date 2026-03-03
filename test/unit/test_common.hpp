#pragma once
// Shared POD types and geometry helpers for unit tests.
// Self-contained: no project headers, no Gmsh dependency.

#include <algorithm>
#include <cmath>
#include <vector>


// ===================================================================
// Minimal local geometry types
// ===================================================================

struct Pt3 {
  double x, y, z;
  Pt3(double x = 0, double y = 0, double z = 0) : x(x), y(y), z(z) {}
};

struct Vec3 {
  double x, y, z;
  Vec3(double x = 0, double y = 0, double z = 0) : x(x), y(y), z(z) {}
  double normSq() const { return x*x + y*y + z*z; }
};

static Vec3 operator-(const Pt3& a, const Pt3& b) {
  return Vec3(a.x - b.x, a.y - b.y, a.z - b.z);
}
static Pt3 operator+(const Pt3& p, const Vec3& v) {
  return Pt3(p.x + v.x, p.y + v.y, p.z + v.z);
}
static Vec3 operator*(double s, const Vec3& v) {
  return Vec3(s * v.x, s * v.y, s * v.z);
}


// ===================================================================
// Standalone math helpers (mirror the algorithms in offset.cpp / geo.cpp)
// ===================================================================

static double dot3(const Vec3& a, const Vec3& b) {
  return a.x*b.x + a.y*b.y + a.z*b.z;
}

static Vec3 cross3(const Vec3& a, const Vec3& b) {
  return Vec3(
    a.y*b.z - a.z*b.y,
    a.z*b.x - a.x*b.z,
    a.x*b.y - a.y*b.x
  );
}

// Sum of consecutive segment lengths along a polyline.
static double polylineLength(const std::vector<Pt3>& ps) {
  double len = 0.0;
  for (std::size_t i = 1; i < ps.size(); ++i) {
    Vec3 d = ps[i] - ps[i - 1];
    len += std::sqrt(d.normSq());
  }
  return len;
}

// Point at arc-length parameter u in [0, 1] along the polyline.
static Pt3 paramPointOnPolyline(const std::vector<Pt3>& ps, double u, double tol) {
  double total = polylineLength(ps);
  double target = u * total;
  for (std::size_t i = 0; i + 1 < ps.size(); ++i) {
    Vec3 d = ps[i + 1] - ps[i];
    double seg = std::sqrt(d.normSq());
    if (target <= seg + tol || i + 2 == ps.size()) {
      double t = (seg > tol) ? target / seg : 0.0;
      return ps[i] + t * d;
    }
    target -= seg;
  }
  return ps.back();
}

// 2-D line–line intersection using the y and z coordinates of Pt3.
// Returns true when not parallel; u1/u2 are the parametric locations on
// line-1 and line-2 respectively.
// Formula derived from parametric intersection with Cramer's rule.
static bool lineIntersect2D(
    const Pt3& p1, const Pt3& p2,
    const Pt3& p3, const Pt3& p4,
    double& u1, double& u2, double tol) {
  double dy1 = p2.y - p1.y, dz1 = p2.z - p1.z;
  double dy2 = p4.y - p3.y, dz2 = p4.z - p3.z;
  double cross = dy1 * dz2 - dz1 * dy2;
  if (std::fabs(cross) < tol) return false;
  double dy3 = p3.y - p1.y, dz3 = p3.z - p1.z;
  u1 = (dy3 * dz2 - dz3 * dy2) / cross;
  u2 = (dy3 * dz1 - dz3 * dy1) / cross;
  return true;
}

// Mirror of the isClose() check used in offset.cpp (absolute + relative tol).
static bool isClose(double x1, double y1, double x2, double y2,
                    double abs_tol, double rel_tol) {
  double d   = std::sqrt((x1-x2)*(x1-x2) + (y1-y2)*(y1-y2));
  double ref = std::max(std::sqrt(x1*x1 + y1*y1), std::sqrt(x2*x2 + y2*y2));
  return d <= abs_tol + rel_tol * ref;
}
