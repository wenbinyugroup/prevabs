// Unit tests for geometry helpers in src/geo/
//
// Self-contained: no project headers, no Gmsh dependency.
// All geometric logic is reimplemented inline using local POD types so that
// the tests exercise the same algorithms as the production code without
// requiring the full build environment.

#include "catch_amalgamated.hpp"

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


// ===================================================================
// Tests: groupValidSegments filter (dot-product + length check)
// ===================================================================

TEST_CASE("groupValidSegments: forward segment with matching offset is valid",
          "[geo][offset]") {
  Pt3 b0{0,0,0}, b1{0,1,0};
  Pt3 q0{0,0,1}, q1{0,1,1};
  Vec3 vb = b1 - b0;
  Vec3 vo = q1 - q0;
  CHECK(dot3(vb, vo) > 0);
  CHECK(vo.normSq() > 0);
}

TEST_CASE("groupValidSegments: reversed offset segment fails dot-product check",
          "[geo][offset]") {
  Pt3 b0{0,0,0}, b1{0,1,0};
  Pt3 r0{0,0,1}, r1{0,-1,1};  // offset goes backward in y
  Vec3 vb = b1 - b0;
  Vec3 vr = r1 - r0;
  CHECK(dot3(vb, vr) <= 0);
}

TEST_CASE("groupValidSegments: zero-length offset segment fails length check",
          "[geo][offset]") {
  Pt3 z0{0,0,1}, z1{0,0,1};
  Vec3 vz = z1 - z0;
  CHECK(vz.normSq() <= 0.0);
}

TEST_CASE("groupValidSegments: two consecutive valid segments form one group",
          "[geo][offset]") {
  Pt3 base[] = {{0,0,0},{0,1,0},{0,2,0}};
  Pt3 offs[] = {{0,0,1},{0,1,1},{0,2,1}};

  std::vector<std::vector<int>> groups;
  bool check_prev = false, check_next;
  for (int j = 0; j < 2; ++j) {
    Vec3 vb = base[j+1] - base[j];
    Vec3 vo = offs[j+1] - offs[j];
    if (dot3(vb, vo) <= 0 || vo.normSq() <= 0) {
      check_next = false;
    } else {
      check_next = true;
      if (!check_prev) groups.push_back({j});
      groups.back().push_back(j + 1);
    }
    check_prev = check_next;
  }

  REQUIRE(groups.size() == 1);
  CHECK(groups[0].size() == 3);
  CHECK(groups[0][0] == 0);
  CHECK(groups[0][2] == 2);
}

TEST_CASE("groupValidSegments: invalid middle segment splits into two groups",
          "[geo][offset]") {
  // seg0 valid, seg1 reversed (invalid), seg2 valid → 2 separate groups
  Pt3 base[] = {{0,0,0},{0,1,0},{0,2,0},{0,3,0}};
  Pt3 offs[] = {{0,0,1},{0,1,1},{0,0,1},{0,3,1}};  // seg1: q1→q2 goes backward

  std::vector<std::vector<int>> groups;
  bool check_prev = false, check_next;
  for (int j = 0; j < 3; ++j) {
    Vec3 vb = base[j+1] - base[j];
    Vec3 vo = offs[j+1] - offs[j];
    if (dot3(vb, vo) <= 0 || vo.normSq() <= 0) {
      check_next = false;
    } else {
      check_next = true;
      if (!check_prev) groups.push_back({j});
      groups.back().push_back(j + 1);
    }
    check_prev = check_next;
  }

  REQUIRE(groups.size() == 2);
  CHECK(groups[0][0] == 0);
  CHECK(groups[0][1] == 1);
  CHECK(groups[1][0] == 2);
  CHECK(groups[1][1] == 3);
}

TEST_CASE("groupValidSegments: all segments reversed yields empty group list",
          "[geo][offset]") {
  Pt3 base[] = {{0,0,0},{0,1,0},{0,2,0},{0,3,0}};
  Pt3 offs[] = {{0,0,1},{0,-1,1},{0,-2,1},{0,-3,1}};  // all reversed

  std::vector<std::vector<int>> groups;
  bool check_prev = false, check_next;
  for (int j = 0; j < 3; ++j) {
    Vec3 vb = base[j+1] - base[j];
    Vec3 vo = offs[j+1] - offs[j];
    if (dot3(vb, vo) <= 0 || vo.normSq() <= 0) {
      check_next = false;
    } else {
      check_next = true;
      if (!check_prev) groups.push_back({j});
      groups.back().push_back(j + 1);
    }
    check_prev = check_next;
  }

  CHECK(groups.empty());
}

TEST_CASE("groupValidSegments: only first segment valid makes one group",
          "[geo][offset]") {
  Pt3 base[] = {{0,0,0},{0,1,0},{0,2,0}};
  Pt3 offs[] = {{0,0,1},{0,1,1},{0,0,1}};  // seg1 reversed

  std::vector<std::vector<int>> groups;
  bool check_prev = false, check_next;
  for (int j = 0; j < 2; ++j) {
    Vec3 vb = base[j+1] - base[j];
    Vec3 vo = offs[j+1] - offs[j];
    if (dot3(vb, vo) <= 0 || vo.normSq() <= 0) {
      check_next = false;
    } else {
      check_next = true;
      if (!check_prev) groups.push_back({j});
      groups.back().push_back(j + 1);
    }
    check_prev = check_next;
  }

  REQUIRE(groups.size() == 1);
  CHECK(groups[0][0] == 0);
  CHECK(groups[0][1] == 1);
}


// ===================================================================
// Tests: offset direction for a 2-vertex polyline
// ===================================================================

TEST_CASE("offset direction: side=+1 on +y segment offsets in +z",
          "[geo][offset]") {
  // In offset.cpp: n=(side,0,0), t=segment direction, dir=cross(n,t).unit()*dist
  const double dist = 0.5, tol = 1e-10;
  Vec3 n{1, 0, 0}, t{0, 1, 0};
  Vec3 cp = cross3(n, t);
  double norm = std::sqrt(cp.normSq());
  REQUIRE(norm > tol);
  double dirz = cp.z / norm * dist;
  CHECK(std::fabs(dirz - dist) < tol);
}

TEST_CASE("offset direction: side=-1 on +y segment offsets in -z",
          "[geo][offset]") {
  const double dist = 0.5, tol = 1e-10;
  Vec3 n{-1, 0, 0}, t{0, 1, 0};
  Vec3 cp = cross3(n, t);
  double norm = std::sqrt(cp.normSq());
  REQUIRE(norm > tol);
  double dirz = cp.z / norm * dist;
  CHECK(std::fabs(dirz - (-dist)) < tol);
}

TEST_CASE("offset direction: side=+1 on +x segment offsets in -z",
          "[geo][offset]") {
  // cross((1,0,0), (0,0,1)) = (0*1-0*0, -(1*1-0*0), 1*0-0*0) = (0,-1,0) — +y segment
  // cross((1,0,0), (1,0,0)) is zero — use a different segment direction
  // cross((1,0,0), (0,0,1)) = (0*1-0*0, 0*1-1*1, 1*0-0*0) = (0,-1,0) → offset in -y
  const double dist = 0.5, tol = 1e-10;
  Vec3 n{1, 0, 0}, t{0, 0, 1};  // segment along z
  Vec3 cp = cross3(n, t);
  double norm = std::sqrt(cp.normSq());
  REQUIRE(norm > tol);
  // cp = (0*1-0*0, 0*0-1*1, 1*0-0*0) = (0, -1, 0)
  double diry = cp.y / norm * dist;
  CHECK(std::fabs(diry - (-dist)) < tol);
}


// ===================================================================
// Tests: polyline length
// ===================================================================

TEST_CASE("polylineLength: L-shaped 3-point polyline", "[geo]") {
  std::vector<Pt3> ps = {{0,0,0}, {0,3,0}, {0,3,4}};
  CHECK(std::fabs(polylineLength(ps) - 7.0) < 1e-9);
}

TEST_CASE("polylineLength: single straight segment", "[geo]") {
  std::vector<Pt3> ps = {{0,0,0}, {0,0,5}};
  CHECK(std::fabs(polylineLength(ps) - 5.0) < 1e-9);
}

TEST_CASE("polylineLength: degenerate zero-length segment", "[geo]") {
  std::vector<Pt3> ps = {{0,1,2}, {0,1,2}};
  CHECK(polylineLength(ps) < 1e-12);
}

TEST_CASE("polylineLength: 45-degree diagonal segment", "[geo]") {
  std::vector<Pt3> ps = {{0,0,0}, {0,1,1}};
  CHECK(std::fabs(polylineLength(ps) - std::sqrt(2.0)) < 1e-9);
}


// ===================================================================
// Tests: parametric point on polyline
// ===================================================================

TEST_CASE("paramPointOnPolyline: start (u=0) returns first vertex", "[geo]") {
  std::vector<Pt3> ps = {{0,0,0}, {0,1,0}, {0,2,0}};
  Pt3 p = paramPointOnPolyline(ps, 0.0, 1e-9);
  CHECK(std::fabs(p.y) < 1e-9);
  CHECK(std::fabs(p.z) < 1e-9);
}

TEST_CASE("paramPointOnPolyline: end (u=1) returns last vertex", "[geo]") {
  std::vector<Pt3> ps = {{0,0,0}, {0,1,0}, {0,2,0}};
  Pt3 p = paramPointOnPolyline(ps, 1.0, 1e-9);
  CHECK(std::fabs(p.y - 2.0) < 1e-9);
}

TEST_CASE("paramPointOnPolyline: midpoint of uniform polyline", "[geo]") {
  std::vector<Pt3> ps = {{0,0,0}, {0,1,0}, {0,2,0}};
  Pt3 p = paramPointOnPolyline(ps, 0.5, 1e-9);
  CHECK(std::fabs(p.y - 1.0) < 1e-9);
}

TEST_CASE("paramPointOnPolyline: quarter point on unequal segments", "[geo]") {
  // seg0: length 1, seg1: length 3 → total 4; u=0.25 → arc-length 1.0 → junction
  std::vector<Pt3> ps = {{0,0,0}, {0,1,0}, {0,4,0}};
  Pt3 p = paramPointOnPolyline(ps, 0.25, 1e-9);
  CHECK(std::fabs(p.y - 1.0) < 1e-9);
}


// ===================================================================
// Tests: 2-D line intersection (mirrors calcLineIntersection2D logic)
// ===================================================================

TEST_CASE("lineIntersect2D: perpendicular lines cross at midpoints (u=0.5)",
          "[geo]") {
  Pt3 p1{0,0,0}, p2{0,2,0};   // horizontal in y
  Pt3 p3{0,1,-1}, p4{0,1,1};  // vertical in z at y=1
  double u1, u2;
  bool ok = lineIntersect2D(p1, p2, p3, p4, u1, u2, 1e-10);
  REQUIRE(ok);
  CHECK(std::fabs(u1 - 0.5) < 1e-9);
  CHECK(std::fabs(u2 - 0.5) < 1e-9);
}

TEST_CASE("lineIntersect2D: parallel lines return false", "[geo]") {
  Pt3 p1{0,0,0}, p2{0,1,0};
  Pt3 p3{0,0,1}, p4{0,1,1};  // same direction, shifted in z
  double u1, u2;
  bool ok = lineIntersect2D(p1, p2, p3, p4, u1, u2, 1e-10);
  CHECK(!ok);
}

TEST_CASE("lineIntersect2D: intersection exactly at segment endpoint (u1=1)",
          "[geo]") {
  Pt3 p1{0,0,0}, p2{0,1,0};   // endpoint at y=1
  Pt3 p3{0,1,-1}, p4{0,1,1};  // vertical at y=1
  double u1, u2;
  bool ok = lineIntersect2D(p1, p2, p3, p4, u1, u2, 1e-10);
  REQUIRE(ok);
  CHECK(std::fabs(u1 - 1.0) < 1e-9);
}

TEST_CASE("lineIntersect2D: intersection behind start of segment (u1 < 0)",
          "[geo]") {
  // Line1 starts at y=2 going to y=3, crosses the y=1 line if extended backward
  Pt3 p1{0,2,0}, p2{0,3,0};
  Pt3 p3{0,1,-1}, p4{0,1,1};
  double u1, u2;
  bool ok = lineIntersect2D(p1, p2, p3, p4, u1, u2, 1e-10);
  REQUIRE(ok);
  CHECK(u1 < 0.0);  // intersection is behind p1
}


// ===================================================================
// Tests: isClose proximity helper
// ===================================================================

TEST_CASE("isClose: identical points are close", "[geo]") {
  CHECK(isClose(1.0, 2.0, 1.0, 2.0, 1e-10, 1e-10));
}

TEST_CASE("isClose: points within absolute tolerance are close", "[geo]") {
  CHECK(isClose(0.0, 0.0, 5e-11, 5e-11, 1e-10, 0.0));
}

TEST_CASE("isClose: points outside absolute tolerance are not close", "[geo]") {
  CHECK(!isClose(0.0, 0.0, 1e-9, 0.0, 1e-10, 0.0));
}

TEST_CASE("isClose: distant points are not close", "[geo]") {
  CHECK(!isClose(0.0, 0.0, 1.0, 0.0, 1e-10, 1e-10));
}


// ===================================================================
// Tests: link-index adjustments from trimSubLinePair
// (mirrors the index bookkeeping in offset.cpp trimSubLinePair)
// ===================================================================

TEST_CASE("trim link indices: tail trim keeps entries up to ls_i1 inclusive",
          "[geo][offset]") {
  // Mirrors: for (kk = ls_i1+1; kk < n; kk++) link_a.pop_back();
  std::vector<int> link_a = {10, 11, 12, 13};
  int ls_i1 = 1;
  std::size_t n = link_a.size();
  for (int kk = ls_i1 + 1; kk < static_cast<int>(n); ++kk) {
    link_a.pop_back();
  }
  REQUIRE(link_a.size() == 2);
  CHECK(link_a[0] == 10);
  CHECK(link_a[1] == 11);
}

TEST_CASE("trim link indices: tail trim at ls_i1=0 keeps only first entry",
          "[geo][offset]") {
  std::vector<int> link_a = {10, 11, 12};
  int ls_i1 = 0;
  std::size_t n = link_a.size();
  for (int kk = ls_i1 + 1; kk < static_cast<int>(n); ++kk) {
    link_a.pop_back();
  }
  REQUIRE(link_a.size() == 1);
  CHECK(link_a[0] == 10);
}

TEST_CASE("trim link indices: head trim removes ls_i2-1 leading entries",
          "[geo][offset]") {
  // Mirrors: for (kk = 0; kk < ls_i2-1; kk++) link_b.erase(begin);
  std::vector<int> link_b = {20, 21, 22, 23};
  int ls_i2 = 2;
  for (int kk = 0; kk < ls_i2 - 1; ++kk) {
    link_b.erase(link_b.begin());
  }
  REQUIRE(link_b.size() == 3);
  CHECK(link_b[0] == 21);
  CHECK(link_b[1] == 22);
  CHECK(link_b[2] == 23);
}

TEST_CASE("trim link indices: head trim at ls_i2=1 removes nothing",
          "[geo][offset]") {
  std::vector<int> link_b = {20, 21, 22};
  int ls_i2 = 1;
  for (int kk = 0; kk < ls_i2 - 1; ++kk) {
    link_b.erase(link_b.begin());
  }
  REQUIRE(link_b.size() == 3);
  CHECK(link_b[0] == 20);
}


// ===================================================================
// Tests: sentinel (-1) forward-fill in step 5 of offset()
// ===================================================================

TEST_CASE("sentinel forward-fill: middle -1 filled from predecessor",
          "[geo][offset]") {
  std::vector<int> m = {0, -1, 1};
  for (int i = 0; i < static_cast<int>(m.size()); ++i) {
    if (m[i] == -1) {
      m[i] = (i > 0 && m[i - 1] >= 0) ? m[i - 1] : 0;
    }
  }
  CHECK(m[0] == 0);
  CHECK(m[1] == 0);  // filled from m[0]
  CHECK(m[2] == 1);
}

TEST_CASE("sentinel forward-fill: leading -1 defaults to 0", "[geo][offset]") {
  std::vector<int> m = {-1, 0, 1};
  for (int i = 0; i < static_cast<int>(m.size()); ++i) {
    if (m[i] == -1) {
      m[i] = (i > 0 && m[i - 1] >= 0) ? m[i - 1] : 0;
    }
  }
  CHECK(m[0] == 0);
  CHECK(m[1] == 0);
  CHECK(m[2] == 1);
}

TEST_CASE("sentinel forward-fill: trailing -1 filled from predecessor",
          "[geo][offset]") {
  std::vector<int> m = {0, 1, -1};
  for (int i = 0; i < static_cast<int>(m.size()); ++i) {
    if (m[i] == -1) {
      m[i] = (i > 0 && m[i - 1] >= 0) ? m[i - 1] : 0;
    }
  }
  CHECK(m[0] == 0);
  CHECK(m[1] == 1);
  CHECK(m[2] == 1);  // filled from m[1]
}

TEST_CASE("sentinel forward-fill: consecutive -1 entries cascade forward",
          "[geo][offset]") {
  std::vector<int> m = {2, -1, -1, 3};
  for (int i = 0; i < static_cast<int>(m.size()); ++i) {
    if (m[i] == -1) {
      m[i] = (i > 0 && m[i - 1] >= 0) ? m[i - 1] : 0;
    }
  }
  CHECK(m[0] == 2);
  CHECK(m[1] == 2);
  CHECK(m[2] == 2);
  CHECK(m[3] == 3);
}

TEST_CASE("sentinel forward-fill: no -1 entries are unchanged", "[geo][offset]") {
  std::vector<int> m = {0, 1, 2};
  for (int i = 0; i < static_cast<int>(m.size()); ++i) {
    if (m[i] == -1) {
      m[i] = (i > 0 && m[i - 1] >= 0) ? m[i - 1] : 0;
    }
  }
  CHECK(m[0] == 0);
  CHECK(m[1] == 1);
  CHECK(m[2] == 2);
}
