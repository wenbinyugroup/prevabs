// Unit tests for the offset pipeline in src/geo/offset.cpp.
//
// Self-contained: uses only test_common.hpp (local POD types) and jsonc_loader.hpp
// (JSONC data loader). No project headers, no Gmsh dependency.

#include "catch_amalgamated.hpp"
#include "joinHelpers.hpp"
#include "test_common.hpp"
#include "jsonc_loader.hpp"

#include <cmath>
#include <string>
#include <utility>
#include <vector>


// ===================================================================
// Offset helpers (mirror the algorithms in offset.cpp)
// ===================================================================

// Single-segment offset — mirrors the single-pair offset() in offset.cpp.
static std::pair<Pt3, Pt3> offsetSeg2D(
    const Pt3& v1, const Pt3& v2, int side, double dist) {
  Vec3 n{(double)side, 0.0, 0.0};
  Vec3 t = v2 - v1;
  Vec3 cp = cross3(n, t);
  double norm = std::sqrt(cp.normSq());
  if (norm < 1e-14) return {v1, v2};
  Vec3 dir{cp.x / norm * dist, cp.y / norm * dist, cp.z / norm * dist};
  return {v1 + dir, v2 + dir};
}

// Multi-vertex offset — mirrors Steps 1–2 of the offset() pipeline in offset.cpp:
//   Step 1: offset each segment and compute junction vertices via line intersection.
//   Step 2: filter out invalid (folded-back or degenerate) offset segments.
static std::vector<Pt3> multiVertexOffset(
    const std::vector<Pt3>& base, int side, double dist, double tol = 1e-9) {
  std::size_t n = base.size();
  if (n < 2) return {};
  if (n == 2) {
    auto p = offsetSeg2D(base[0], base[1], side, dist);
    return {p.first, p.second};
  }
  // Step 1: junction vertices
  std::vector<Pt3> junctions;
  junctions.reserve(n);
  Pt3 prev_s{}, prev_e{};
  for (int i = 0; i < (int)n - 1; ++i) {
    auto p = offsetSeg2D(base[i], base[i + 1], side, dist);
    Pt3 cs = p.first, ce = p.second;
    if (i == 0) {
      junctions.push_back(cs);
    } else {
      double u1, u2;
      bool ok = lineIntersect2D(prev_s, prev_e, cs, ce, u1, u2, tol);
      if (ok) {
        Vec3 d = prev_e - prev_s;
        junctions.push_back({0.0, prev_s.y + u1 * d.y, prev_s.z + u1 * d.z});
      } else {
        junctions.push_back(prev_e);
      }
    }
    if (i == (int)n - 2) junctions.push_back(ce);
    prev_s = cs;
    prev_e = ce;
  }
  // Step 2: validity filter
  std::vector<Pt3> result;
  bool prev_ok = false;
  for (int j = 0; j < (int)n - 1; ++j) {
    Vec3 vb = base[j + 1]      - base[j];
    Vec3 vo = junctions[j + 1] - junctions[j];
    bool cur_ok = (dot3(vb, vo) > 0) && (vo.normSq() >= tol * tol);
    if (cur_ok) {
      if (!prev_ok) result.push_back(junctions[j]);
      result.push_back(junctions[j + 1]);
    }
    prev_ok = cur_ok;
  }
  return result;
}


// ===================================================================
// Test data loader (reads test_offset.jsonc via loadJsonc)
// ===================================================================

struct OffsetTestCase {
  std::vector<Pt3> base;
  double dist = 0.0;
  int side = 1;
  std::vector<Pt3> expected;
};

static std::vector<OffsetTestCase> loadOffsetCases(const std::string& path) {
  nlohmann::json data = loadJsonc(path);
  std::vector<OffsetTestCase> cases;
  for (auto& entry : data) {
    OffsetTestCase tc;
    for (auto& pt : entry["input"]["base_curve"]) {
      tc.base.push_back({0.0, pt[0].get<double>(), pt[1].get<double>()});
    }
    tc.dist = entry["input"]["amount"].get<double>();
    tc.side = entry["input"]["side"].get<int>();
    for (auto& pt : entry["output"]["offset_curve"]) {
      tc.expected.push_back({0.0, pt[0].get<double>(), pt[1].get<double>()});
    }
    cases.push_back(tc);
  }
  return cases;
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

TEST_CASE("join index advance: u=0 resolves to the segment start vertex",
          "[geo][join]") {
  CHECK(advanceIntersectionVertexIndex(3, 0.0, 1e-9) == 4);
}

TEST_CASE("join index advance: interior u resolves to the inserted vertex",
          "[geo][join]") {
  CHECK(advanceIntersectionVertexIndex(3, 0.25, 1e-9) == 4);
}

TEST_CASE("join index advance: u=1 resolves to the segment end vertex",
          "[geo][join]") {
  CHECK(advanceIntersectionVertexIndex(3, 1.0, 1e-9) == 5);
}

TEST_CASE("join index advance: values within tolerance of 0 use the start vertex",
          "[geo][join]") {
  CHECK(advanceIntersectionVertexIndex(3, 5e-10, 1e-9) == 4);
}

TEST_CASE("join index advance: values within tolerance of 1 use the end vertex",
          "[geo][join]") {
  CHECK(advanceIntersectionVertexIndex(3, 1.0 - 5e-10, 1e-9) == 5);
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


// ===================================================================
// Tests: multi-vertex offset end-to-end (data from test_offset.jsonc)
// ===================================================================

TEST_CASE("multi-vertex offset: L-shaped curve (test_offset.jsonc Case 1)",
          "[geo][offset]") {
  const std::string path =
    std::string(TEST_DATA_DIR) + "/test_offset.jsonc";
  std::vector<OffsetTestCase> cases = loadOffsetCases(path);
  REQUIRE_FALSE(cases.empty());

  const OffsetTestCase& tc = cases[0];
  REQUIRE_FALSE(tc.base.empty());
  REQUIRE_FALSE(tc.expected.empty());

  std::vector<Pt3> result = multiVertexOffset(tc.base, tc.side, tc.dist);

  REQUIRE(result.size() == tc.expected.size());
  const double tol = 1e-6;
  for (std::size_t i = 0; i < result.size(); ++i) {
    INFO("vertex " << i);
    CHECK(std::fabs(result[i].y - tc.expected[i].y) < tol);
    CHECK(std::fabs(result[i].z - tc.expected[i].z) < tol);
  }
}
