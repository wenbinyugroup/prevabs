// Unit tests for geometry helpers in src/geo/
//
// Self-contained: no project headers, no Gmsh dependency.
// Offset tests live in test_geo_offset.cpp.

#include "catch_amalgamated.hpp"
#include "test_common.hpp"


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
