#define CATCH_CONFIG_MAIN
#include "catch_amalgamated.hpp"

#include "PGeoClasses.hpp"
#include "geo.hpp"

#include <cmath>
#include <fstream>
#include <sstream>
#include <vector>

static double dot3(const PGeoVector3& a, const PGeoVector3& b) {
  return a.c1() * b.c1() + a.c2() * b.c2() + a.c3() * b.c3();
}

static PGeoVector3 cross3(const PGeoVector3& n, const PGeoVector3& t) {
  return PGeoVector3(
    n.c2() * t.c3() - n.c3() * t.c2(),
    -(n.c1() * t.c3() - n.c3() * t.c1()),
    n.c1() * t.c2() - n.c2() * t.c1()
  );
}

TEST_CASE("groupValidSegments logic", "[geo][offset]") {
  PGeoPoint3 b0{0, 0, 0}, b1{0, 1, 0}, b2{0, 2, 0};
  PGeoPoint3 q0{0, 0, 1}, q1{0, 1, 1}, q2{0, 2, 1};

  PGeoVector3 vb0 = b1 - b0;
  PGeoVector3 vo0 = q1 - q0;
  PGeoVector3 vb1 = b2 - b1;
  PGeoVector3 vo1 = q2 - q1;

  bool seg0_valid = (dot3(vb0, vo0) > 0) && (vo0.normSq() > 0);
  bool seg1_valid = (dot3(vb1, vo1) > 0) && (vo1.normSq() > 0);

  CHECK(seg0_valid == true);
  CHECK(seg1_valid == true);

  PGeoPoint3 r0{0, 0, 1}, r1{0, -1, 1};
  PGeoVector3 vr = r1 - r0;
  bool rev_invalid = (dot3(vb0, vr) <= 0);
  CHECK(rev_invalid == true);

  PGeoPoint3 z0{0, 0, 1}, z1{0, 0, 1};
  PGeoVector3 vz = z1 - z0;
  bool zero_invalid = (vz.normSq() <= 0);
  CHECK(zero_invalid == true);
}

TEST_CASE("groupValidSegments grouping", "[geo][offset]") {
  PGeoPoint3 b0{0, 0, 0}, b1{0, 1, 0}, b2{0, 2, 0}, b3{0, 3, 0};
  PGeoPoint3 q0{0, 0, 1}, q1{0, 1, 1}, q2{0, 0, 1}, q3{0, 3, 1};

  std::vector<PGeoPoint3> base{b0, b1, b2, b3};
  std::vector<PGeoPoint3> vt{q0, q1, q2, q3};

  std::vector<std::vector<int>> groups;
  bool check_prev = false, check_next;
  for (int j = 0; j < 3; ++j) {
    PGeoVector3 vb = base[j + 1] - base[j];
    PGeoVector3 vo = vt[j + 1] - vt[j];
    if (dot3(vb, vo) <= 0 || vo.normSq() <= 0) {
      check_next = false;
    } else {
      check_next = true;
      if (!check_prev) {
        groups.push_back(std::vector<int>{j});
      }
      groups.back().push_back(j + 1);
    }
    check_prev = check_next;
  }

  CHECK(groups.size() == 2);
  CHECK(groups[0].size() == 2);
  CHECK(groups[1].size() == 2);
  CHECK(groups[0][0] == 0);
  CHECK(groups[0][1] == 1);
  CHECK(groups[1][0] == 2);
  CHECK(groups[1][1] == 3);
}

TEST_CASE("offset direction 2-vertex", "[geo][offset]") {
  const double dist = 0.5;
  const double tol = 1e-10;

  for (int side : {1, -1}) {
    PGeoVector3 n(side, 0, 0);
    PGeoVector3 t(0, 1, 0);
    PGeoVector3 cp = cross3(n, t);

    double cpnorm = std::sqrt(cp.normSq());
    REQUIRE(cpnorm > tol);

    double dirz = cp.c3() / cpnorm * dist;
    double expected_z = side * dist;
    double err = std::fabs(dirz - expected_z);

    CHECK(err < tol);
  }
}

TEST_CASE("calcPolylineLength", "[geo]") {
  std::vector<PGeoPoint3> ps;
  ps.push_back(PGeoPoint3{0, 0, 0});
  ps.push_back(PGeoPoint3{0, 3, 0});
  ps.push_back(PGeoPoint3{0, 3, 4});

  double len = calcPolylineLength(ps);
  CHECK(std::fabs(len - 7.0) < 1e-9);
}

TEST_CASE("findParamPointOnPolyline", "[geo]") {
  std::vector<PGeoPoint3> ps;
  ps.push_back(PGeoPoint3{0, 0, 0});
  ps.push_back(PGeoPoint3{0, 1, 0});
  ps.push_back(PGeoPoint3{0, 2, 0});

  double tol = 1e-9;

  PGeoPoint3 p0 = findParamPointOnPolyline(ps, 0.0, tol);
  CHECK(std::fabs(p0.c2()) < tol);
  CHECK(std::fabs(p0.c3()) < tol);

  PGeoPoint3 pmid = findParamPointOnPolyline(ps, 0.5, tol);
  CHECK(std::fabs(pmid.c2() - 1.0) < tol);
  CHECK(std::fabs(pmid.c3()) < tol);

  PGeoPoint3 p1 = findParamPointOnPolyline(ps, 1.0, tol);
  CHECK(std::fabs(p1.c2() - 2.0) < tol);
  CHECK(std::fabs(p1.c3()) < tol);
}
