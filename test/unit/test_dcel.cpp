// test_dcel.cpp
// Unit tests for PDCEL — doubly-connected edge list.
//
// Compiled against the DCEL source files directly.  No Gmsh is required.
// Global variables normally defined in src/main.cpp are provided here.

#include "catch_amalgamated.hpp"
#include <cstdio>
#include <fstream>
#include <spdlog/sinks/base_sink.h>
#include <mutex>
#include <vector>

// ------------------------------------------------------------------
// Global variable definitions (normally in src/main.cpp).
// These must appear exactly once in the test executable.
// ------------------------------------------------------------------
#include "globalVariables.hpp"
#include "utilities.hpp"

bool         scientific_format = false;
PConfig      config;
RuntimeState runtime;

// Register a stderr logger so validate() warnings are visible during tests.
#include "plog.hpp"
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
namespace {
struct LoggerSetup {
  LoggerSetup() {
    if (!spdlog::get("prevabs")) {
      auto sink   = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
      auto logger = std::make_shared<spdlog::logger>("prevabs", sink);
      logger->set_level(spdlog::level::warn);
      spdlog::register_logger(logger);
    }
  }
} _logger_setup;
} // namespace

// ------------------------------------------------------------------
// DCEL headers
// ------------------------------------------------------------------
#include "PDCEL.hpp"
#include "PDCELFace.hpp"
#include "PDCELHalfEdge.hpp"
#include "PDCELHalfEdgeLoop.hpp"
#include "PDCELUtils.hpp"
#include "PDCELVertex.hpp"
#include "PBaseLine.hpp"
#include "PComponent.hpp"
#include "Material.hpp"
#include "PSegment.hpp"
#include "PModel.hpp"
#include "geo.hpp"

#include <algorithm>
#include <cmath>
#include <list>


// ==================================================================
// Helpers
// ==================================================================

// Walk the twin->next cycle starting from he and collect all visited
// half-edges.  Stops after limit steps to prevent infinite loops on
// broken DCEL state.
static std::vector<PDCELHalfEdge *> collectCycleAroundVertex(
    PDCELHalfEdge *start, int limit = 64)
{
  std::vector<PDCELHalfEdge *> result;
  PDCELHalfEdge *he = start;
  do {
    result.push_back(he);
    if (he->twin() == nullptr) break;
    he = he->twin()->next();
    if (he == nullptr) break;
  } while (he != start && (int)result.size() < limit);
  return result;
}

// Walk the next() chain starting from he and collect all visited
// half-edges (one face boundary).
static std::vector<PDCELHalfEdge *> collectFaceBoundary(
    PDCELHalfEdge *start, int limit = 64)
{
  std::vector<PDCELHalfEdge *> result;
  PDCELHalfEdge *he = start;
  do {
    result.push_back(he);
    he = he->next();
    if (he == nullptr) break;
  } while (he != start && (int)result.size() < limit);
  return result;
}

static PDCELFace *findFaceByName(PDCEL &dcel, PModel &model,
                                 const std::string &name) {
  for (PDCELFace *face : dcel.faces()) {
    if (model.faceData(face).name == name) {
      return face;
    }
  }
  return nullptr;
}

struct FillComponentFixture {
  PDCEL dcel;
  PModel model;
  Material material;
  LayerType fill_layertype;
  BuilderConfig bcfg{};

  FillComponentFixture()
      : material("fill_mat"),
        fill_layertype(1, &material, 0.0) {

    dcel.initialize();
    addRectangularSection();

    model.addMaterial(&material);
    model.addLayerType(&fill_layertype);

    bcfg.debug_level = DebugLevel::off;
    bcfg.tool = AnalysisTool::VABS;
    bcfg.tol = 1e-12;
    bcfg.geo_tol = 1e-9;
    bcfg.dcel = &dcel;
    bcfg.materials = &model;
    bcfg.model = &model;
  }

  void addRectangularSection() {
    PDCELVertex *v1 = new PDCELVertex(0.0, 0.0, 0.0);
    PDCELVertex *v2 = new PDCELVertex(0.0, 4.0, 0.0);
    PDCELVertex *v3 = new PDCELVertex(0.0, 4.0, 4.0);
    PDCELVertex *v4 = new PDCELVertex(0.0, 0.0, 4.0);
    std::list<PDCELVertex *> loop = {v1, v2, v3, v4, v1};

    PDCELFace *face = dcel.addFace(loop);
    REQUIRE(face != nullptr);
    REQUIRE(face->outer() != nullptr);

    PDCELHalfEdgeLoop *hel = dcel.addHalfEdgeLoop(face->outer());
    REQUIRE(hel != nullptr);
    hel->setFace(face);
  }

  PDCELVertex *findVertex(double y, double z) {
    for (PDCELVertex *vertex : dcel.vertices()) {
      if (vertex->y() == y && vertex->z() == z) {
        return vertex;
      }
    }
    return nullptr;
  }
};

struct LaminateComponentFixture {
  PDCEL dcel;
  PModel model;
  Material material;
  Lamina lamina;
  LayerType layertype;
  Layup layup;
  BuilderConfig bcfg{};

  LaminateComponentFixture()
      : material("lam_mat"),
        lamina("lam", &material, 1.0),
        layertype(1, &material, 0.0),
        layup("layup") {

    dcel.initialize();
    model.addMaterial(&material);
    model.addLayerType(&layertype);
    layup.addLayer(&lamina, 0.0, 1, &layertype);

    bcfg.debug_level = DebugLevel::off;
    bcfg.tool = AnalysisTool::VABS;
    bcfg.tol = 1e-12;
    bcfg.geo_tol = 1e-9;
    bcfg.dcel = &dcel;
    bcfg.materials = &model;
    bcfg.model = &model;
  }
};


TEST_CASE("buildFilling: open baseline without loop intersection returns early",
          "[dcel][component][fill][failure]") {
  FillComponentFixture fixture;

  PComponent component;
  component.setName("fill_no_intersection");
  component.setType(ComponentType::fill);
  component.setFillMaterial(&fixture.material);
  component.setFillLayertype(&fixture.fill_layertype);
  component.setFillFace(nullptr);

  PDCELVertex location(0.0, 2.0, 2.0);
  component.setFillLocation(&location);

  Baseline open("open_inside", "line");
  open.addPVertex(new PDCELVertex(0.0, 1.0, 1.0));
  open.addPVertex(new PDCELVertex(0.0, 3.0, 1.0));
  component.addFillBaselineGroup({&open});

  const std::size_t faces_before = fixture.dcel.faces().size();
  const std::size_t edges_before = fixture.dcel.halfedges().size();
  const std::size_t loops_before = fixture.dcel.halfedgeloops().size();

  component.build(fixture.bcfg);

  CHECK(component.fillface() == nullptr);
  CHECK(fixture.dcel.faces().size() == faces_before);
  CHECK(fixture.dcel.halfedges().size() == edges_before);
  CHECK(fixture.dcel.halfedgeloops().size() == loops_before);
}

TEST_CASE("buildFilling: outside fill location does not create a face",
          "[dcel][component][fill][failure]") {
  FillComponentFixture fixture;

  PComponent component;
  component.setName("fill_outside");
  component.setType(ComponentType::fill);
  component.setFillMaterial(&fixture.material);
  component.setFillLayertype(&fixture.fill_layertype);
  component.setFillFace(nullptr);

  PDCELVertex location(0.0, 5.0, 2.0);
  component.setFillLocation(&location);

  const std::size_t faces_before = fixture.dcel.faces().size();

  component.build(fixture.bcfg);

  CHECK(component.fillface() == nullptr);
  CHECK(fixture.dcel.faces().size() == faces_before);
}

TEST_CASE("buildFilling: open baseline is split onto the enclosing boundary",
          "[dcel][component][fill][split]") {
  FillComponentFixture fixture;

  PComponent component;
  component.setName("fill_split");
  component.setType(ComponentType::fill);
  component.setFillMaterial(&fixture.material);
  component.setFillLayertype(&fixture.fill_layertype);
  component.setFillFace(nullptr);

  PDCELVertex location(0.0, 2.0, 3.0);
  component.setFillLocation(&location);

  Baseline open("open_crossing", "line");
  open.addPVertex(new PDCELVertex(0.0, -1.0, 2.0));
  open.addPVertex(new PDCELVertex(0.0, 2.0, 2.0));
  open.addPVertex(new PDCELVertex(0.0, 5.0, 2.0));
  component.addFillBaselineGroup({&open});

  const std::size_t faces_before = fixture.dcel.faces().size();
  const std::size_t vertices_before = fixture.dcel.vertices().size();

  component.build(fixture.bcfg);

  REQUIRE(component.fillface() != nullptr);
  CHECK(fixture.model.faceData(component.fillface()).name == "fill_split_fill_face");
  CHECK(fixture.dcel.faces().size() == faces_before + 1);

  PDCELVertex *left_split = fixture.findVertex(0.0, 2.0);
  PDCELVertex *mid_vertex = fixture.findVertex(2.0, 2.0);
  PDCELVertex *right_split = fixture.findVertex(4.0, 2.0);
  REQUIRE(left_split != nullptr);
  REQUIRE(mid_vertex != nullptr);
  REQUIRE(right_split != nullptr);
  CHECK(fixture.dcel.vertices().size() == vertices_before + 3);
  CHECK(fixture.dcel.findHalfEdgeBetween(left_split, mid_vertex) != nullptr);
  CHECK(fixture.dcel.findHalfEdgeBetween(mid_vertex, right_split) != nullptr);
}

TEST_CASE("buildDetails: filling component is an explicit no-op",
          "[dcel][component][fill][details]") {
  FillComponentFixture fixture;

  PComponent component;
  component.setName("fill_details_skip");
  component.setType(ComponentType::fill);
  component.setFillMaterial(&fixture.material);
  component.setFillLayertype(&fixture.fill_layertype);
  component.setFillFace(nullptr);

  const std::size_t faces_before = fixture.dcel.faces().size();
  const std::size_t edges_before = fixture.dcel.halfedges().size();
  const std::size_t loops_before = fixture.dcel.halfedgeloops().size();

  component.buildDetails(fixture.bcfg);

  CHECK(component.fillface() == nullptr);
  CHECK(fixture.dcel.faces().size() == faces_before);
  CHECK(fixture.dcel.halfedges().size() == edges_before);
  CHECK(fixture.dcel.halfedgeloops().size() == loops_before);
}

TEST_CASE("buildLaminate: unresolved connected end aborts before shell build",
          "[dcel][component][laminate][failure]") {
  LaminateComponentFixture fixture;

  PDCELVertex *shared = new PDCELVertex(0.0, 0.0, 0.0);
  PDCELVertex *east = new PDCELVertex(0.0, 2.0, 0.0);
  PDCELVertex *west = new PDCELVertex(0.0, -2.0, 0.0);

  Baseline base_1("base_1", "line");
  base_1.addPVertex(shared);
  base_1.addPVertex(east);

  Baseline base_2("base_2", "line");
  base_2.addPVertex(shared);
  base_2.addPVertex(west);

  Segment seg_1("seg_1", &base_1, &fixture.layup, "left", 1);
  Segment seg_2("seg_2", &base_2, &fixture.layup, "left", 1);

  PComponent component;
  component.setName("laminate_parallel_heads");
  component.setType(ComponentType::laminate);
  component.setStyle(static_cast<JointStyle>(0));
  component.addSegment(&seg_1);
  component.addSegment(&seg_2);

  const std::size_t faces_before = fixture.dcel.faces().size();
  const std::size_t loops_before = fixture.dcel.halfedgeloops().size();

  component.build(fixture.bcfg);

  CHECK(seg_1.headVertexOffset() == nullptr);
  CHECK(seg_2.headVertexOffset() == nullptr);
  CHECK(seg_1.tailVertexOffset() != nullptr);
  CHECK(seg_2.tailVertexOffset() != nullptr);
  CHECK(seg_1.face() == nullptr);
  CHECK(seg_2.face() == nullptr);
  CHECK(fixture.dcel.faces().size() == faces_before);
  CHECK(fixture.dcel.halfedgeloops().size() == loops_before);
  CHECK(fixture.dcel.findHalfEdgeBetween(shared, east) == nullptr);
  CHECK(fixture.dcel.findHalfEdgeBetween(shared, west) == nullptr);
}

TEST_CASE("buildLaminate: cyclic component closes first and last segments",
          "[dcel][component][laminate][cycle]") {
  LaminateComponentFixture fixture;

  PDCELVertex *a_head = new PDCELVertex(0.0, 0.0, 0.0);
  PDCELVertex *b = new PDCELVertex(0.0, 2.0, 0.0);
  PDCELVertex *c = new PDCELVertex(0.0, 1.0, 2.0);
  PDCELVertex *a_tail = new PDCELVertex(0.0, 0.0, 0.0);

  Baseline base_1("base_1", "line");
  base_1.addPVertex(a_head);
  base_1.addPVertex(b);

  Baseline base_2("base_2", "line");
  base_2.addPVertex(b);
  base_2.addPVertex(c);

  Baseline base_3("base_3", "line");
  base_3.addPVertex(c);
  base_3.addPVertex(a_tail);

  Segment seg_1("seg_1", &base_1, &fixture.layup, "left", 1);
  Segment seg_2("seg_2", &base_2, &fixture.layup, "left", 1);
  Segment seg_3("seg_3", &base_3, &fixture.layup, "left", 1);

  PComponent component;
  component.setName("laminate_cycle");
  component.setType(ComponentType::laminate);
  component.setCycle(true);
  component.addSegment(&seg_1);
  component.addSegment(&seg_2);
  component.addSegment(&seg_3);

  const std::size_t faces_before = fixture.dcel.faces().size();

  component.build(fixture.bcfg);

  CHECK(seg_1.headVertexOffset() != nullptr);
  CHECK(seg_1.tailVertexOffset() != nullptr);
  CHECK(seg_2.headVertexOffset() != nullptr);
  CHECK(seg_2.tailVertexOffset() != nullptr);
  CHECK(seg_3.headVertexOffset() != nullptr);
  CHECK(seg_3.tailVertexOffset() != nullptr);
  CHECK(seg_1.face() != nullptr);
  CHECK(seg_2.face() != nullptr);
  CHECK(seg_3.face() != nullptr);
  CHECK(fixture.dcel.faces().size() > faces_before);
}


// ==================================================================
// 1. Vertex operations
// ==================================================================

TEST_CASE("addVertex: new vertex is inserted and returned", "[dcel][vertex]") {
  PDCEL dcel;
  PDCELVertex *v = new PDCELVertex(0, 1.0, 2.0);
  PDCELVertex *canon = dcel.addVertex(v);

  CHECK(canon == v);
  CHECK(dcel.vertices().size() == 1);
}

TEST_CASE("addVertex: coincident vertex returns existing, input not inserted",
          "[dcel][vertex]") {
  PDCEL dcel;
  PDCELVertex *v1 = new PDCELVertex(0, 1.0, 2.0);
  PDCELVertex *v1c = dcel.addVertex(v1);

  // Second vertex at the same position (within GEO_TOL)
  PDCELVertex *v2 = new PDCELVertex(0, 1.0, 2.0);
  PDCELVertex *v2c = dcel.addVertex(v2);

  // v2 was coincident: addVertex returns the existing v1, does not insert v2
  CHECK(v2c == v1c);
  CHECK(dcel.vertices().size() == 1);

  // v2 was not inserted; caller owns it and must delete it
  delete v2;
}

TEST_CASE("addVertex: distinct vertices are all inserted", "[dcel][vertex]") {
  PDCEL dcel;
  PDCELVertex *va = dcel.addVertex(new PDCELVertex(0, 0.0, 0.0));
  PDCELVertex *vb = dcel.addVertex(new PDCELVertex(0, 1.0, 0.0));
  PDCELVertex *vc = dcel.addVertex(new PDCELVertex(0, 0.0, 1.0));

  CHECK(dcel.vertices().size() == 3);
  CHECK(va != vb);
  CHECK(vb != vc);
  CHECK(va != vc);
}


// ==================================================================
// 2. Edge operations — twin wiring and source/target
// ==================================================================

TEST_CASE("addEdge: creates two half-edges with correct twin relationship",
          "[dcel][edge]") {
  PDCEL dcel;
  PDCELVertex *v1 = dcel.addVertex(new PDCELVertex(0, 0.0, 0.0));
  PDCELVertex *v2 = dcel.addVertex(new PDCELVertex(0, 1.0, 0.0));

  PDCELHalfEdge *he = dcel.addEdge(v1, v2);

  REQUIRE(he != nullptr);
  REQUIRE(he->twin() != nullptr);

  // Exactly two half-edges added
  CHECK(dcel.halfedges().size() == 2);

  // Twin symmetry: he->twin()->twin() == he
  CHECK(he->twin()->twin() == he);
}

TEST_CASE("buildAreas: last-area frame queries base curve, not the offset",
          "[dcel][segment][areas]") {

  // Pre-Phase-B (plan-20260514-decouple-local-frame-from-map.md) the last
  // area's y1 was taken from `buildAreaBaseSegmentFromPair`, which at a
  // degenerate staircase step (base index repeating) pulled the segment
  // from the *offset* curve.  That gave a non-normalised vector along the
  // offset's local direction.  Phase B replaces that path: face frames are
  // now queried per-face from the base curve via CurveFrameLookup, so the
  // assertion here is the unit tangent of the base polyline at the face
  // centroid — irrespective of any degenerate offset pair.
  Material material("mat");
  Lamina lamina("lam", &material, 1.0);
  LayerType layertype(1, &material, 0.0);
  Layup layup("layup");
  layup.addLayer(&lamina, 0.0, 1, &layertype);

  Baseline base("base", "line");
  base.addPVertex(new PDCELVertex(0.0, 0.0, 0.0));
  base.addPVertex(new PDCELVertex(0.0, 1.0, 0.0));
  base.addPVertex(new PDCELVertex(0.0, 2.0, 0.0));

  Segment segment("seg", &base, &layup, "left", 1);
  segment.setMatOrient1("baseline");
  segment.offsetCurveBase();

  Baseline *offset = segment.curveOffset();
  for (PDCELVertex *vertex : offset->vertices()) {
    delete vertex;
  }
  offset->vertices().clear();
  offset->addPVertex(new PDCELVertex(0.0, 0.0, 1.0));
  offset->addPVertex(new PDCELVertex(0.0, 1.0, 1.0));
  offset->addPVertex(new PDCELVertex(0.0, 1.5, 1.0));
  offset->addPVertex(new PDCELVertex(0.0, 2.0, 1.0));

  BaseOffsetMap &pairs = segment.baseOffsetIndicesPairs();
  pairs.clear();
  pairs.push_back(BaseOffsetPair(0, 0));
  pairs.push_back(BaseOffsetPair(1, 1));
  pairs.push_back(BaseOffsetPair(2, 2));
  pairs.push_back(BaseOffsetPair(2, 3));

  PDCEL dcel;
  dcel.initialize();
  dcel.addEdge(base.vertices().front(), offset->vertices().front());
  dcel.addEdge(base.vertices().back(), offset->vertices().back());
  segment.setHeadVertexOffset(offset->vertices().front());
  segment.setTailVertexOffset(offset->vertices().back());

  PModel model;
  BuilderConfig bcfg{};
  bcfg.debug_level = DebugLevel::off;
  bcfg.tool = AnalysisTool::VABS;
  bcfg.tol = 1e-12;
  bcfg.geo_tol = 1e-9;
  bcfg.dcel = &dcel;
  bcfg.materials = &model;
  bcfg.model = &model;

  segment.build(bcfg);
  segment.buildAreas(bcfg);

  PDCELFace *last_layer_face = findFaceByName(
      dcel, model, "seg_area_3_layer_1");
  REQUIRE(last_layer_face != nullptr);

  // Base curve is collinear along +y, so the nearest-segment unit tangent
  // at any centroid is (0, 1, 0).
  const SVector3 y1 = last_layer_face->localy1();
  CHECK(y1[0] == Catch::Approx(0.0));
  CHECK(y1[1] == Catch::Approx(1.0));
  CHECK(y1[2] == Catch::Approx(0.0));
}

TEST_CASE("offsetCurveBase: repeated pre-build calls reuse the same offset curve",
          "[dcel][segment][offset]") {
  Material material("mat");
  Lamina lamina("lam", &material, 1.0);
  LayerType layertype(1, &material, 0.0);
  Layup layup("layup");
  layup.addLayer(&lamina, 0.0, 1, &layertype);

  Baseline base("base", "line");
  base.addPVertex(new PDCELVertex(0.0, 0.0, 0.0));
  base.addPVertex(new PDCELVertex(0.0, 2.0, 0.0));

  Segment segment("seg", &base, &layup, "left", 1);

  segment.offsetCurveBase();
  Baseline *first_offset = segment.curveOffset();
  REQUIRE(first_offset != nullptr);
  const std::size_t offset_vertex_count = first_offset->vertices().size();
  const std::size_t pair_count = segment.baseOffsetIndicesPairs().size();

  segment.offsetCurveBase();

  CHECK(segment.curveOffset() == first_offset);
  CHECK(segment.curveOffset()->vertices().size() == offset_vertex_count);
  CHECK(segment.baseOffsetIndicesPairs().size() == pair_count);
}

TEST_CASE("getIntersectionVertex: aliased curves shift the second insertion",
          "[dcel][geo][intersect]") {
  PDCELVertex *v0 = new PDCELVertex(0.0, 0.0, 0.0);
  PDCELVertex *v1 = new PDCELVertex(0.0, 2.0, 2.0);
  PDCELVertex *v2 = new PDCELVertex(0.0, 0.0, 2.0);
  PDCELVertex *v3 = new PDCELVertex(0.0, 2.0, 0.0);
  PDCELVertex *v4 = new PDCELVertex(0.0, 4.0, 0.0);

  std::vector<PDCELVertex *> curve = {v0, v1, v2, v3, v4};
  int i1 = 0;
  int i2 = 2;
  int is_new_1 = 0;
  int is_new_2 = 0;

  PDCELVertex *ip = getIntersectionVertex(
      curve, curve, i1, i2, 0.5, 0.5, is_new_1, is_new_2, 1e-9);

  REQUIRE(ip != nullptr);
  CHECK(is_new_1 == 1);
  CHECK(is_new_2 == 1);
  CHECK(i1 == 1);
  CHECK(i2 == 4);
  REQUIRE(curve.size() == 7);
  CHECK(curve[0] == v0);
  CHECK(curve[1] == ip);
  CHECK(curve[2] == v1);
  CHECK(curve[3] == v2);
  CHECK(curve[4] == ip);
  CHECK(curve[5] == v3);
  CHECK(curve[6] == v4);
  CHECK(std::count(curve.begin(), curve.end(), ip) == 2);

  delete ip;
  delete v0;
  delete v1;
  delete v2;
  delete v3;
  delete v4;
}

TEST_CASE("offset: acute cusp applies miter limit surrogate",
          "[dcel][geo][offset]") {
  PDCELVertex *v0 = new PDCELVertex(0.0, 0.0, 0.0);
  PDCELVertex *v1 = new PDCELVertex(0.0, 1.0, 0.0);
  PDCELVertex *v2 = new PDCELVertex(0.0, 0.1, 0.05);
  std::vector<PDCELVertex *> base = {v0, v1, v2};

  const int side = 1;
  const double dist = 0.1;
  PDCELVertex prev_start, prev_end, cur_start, cur_end;
  REQUIRE(offset(v0, v1, side, dist, &prev_start, &prev_end) == 1);
  REQUIRE(offset(v1, v2, side, dist, &cur_start, &cur_end) == 1);

  double u_prev = 0.0;
  double u_cur = 0.0;
  REQUIRE(calcLineIntersection2D(
      &prev_start, &prev_end, &cur_start, &cur_end,
      u_prev, u_cur, TOLERANCE));
  const SPoint3 raw_miter =
      getParametricPoint(prev_start.point(), prev_end.point(), u_prev);
  const double raw_miter_dist = raw_miter.distance(v1->point());
  REQUIRE(raw_miter_dist > 2.0 * dist);

  std::vector<PDCELVertex *> offset_vertices;
  BaseOffsetMap id_pairs;
  REQUIRE(offset(base, side, dist, offset_vertices, id_pairs) == 1);
  REQUIRE(offset_vertices.size() == 3);
  REQUIRE(validateBaseOffsetMap(id_pairs));

  const double limited_dist =
      offset_vertices[1]->point().distance(v1->point());
  CHECK(limited_dist <= 2.0 * dist + 1e-9);

  const SPoint3 bevel_mid =
      getParametricPoint(prev_end.point(), cur_start.point(), 0.5);
  CHECK(offset_vertices[1]->point().distance(bevel_mid) <= 1e-9);

  for (PDCELVertex *vertex : offset_vertices) {
    delete vertex;
  }
  delete v0;
  delete v1;
  delete v2;
}

TEST_CASE("Baseline::findMinimumInteriorAngle detects closed cusp",
          "[dcel][baseline][cusp]") {
  Baseline base("cusp", "line");
  PDCELVertex *v0 = new PDCELVertex(0.0, 0.0, 0.0);
  PDCELVertex *v1 = new PDCELVertex(0.0, 1.0, 0.0);
  PDCELVertex *v2 = new PDCELVertex(0.0, 0.1, 0.05);
  base.addPVertex(v0);
  base.addPVertex(v1);
  base.addPVertex(v2);
  base.addPVertex(v0);

  double min_angle_rad = 0.0;
  int vertex_index = -1;
  REQUIRE(base.findMinimumInteriorAngle(min_angle_rad, vertex_index));
  CHECK(vertex_index == 1);
  CHECK(min_angle_rad < 10.0 * PI / 180.0);

  delete v0;
  delete v1;
  delete v2;
}

TEST_CASE("offsetCurveBase: closed box baseline keeps a non-degenerate offset loop",
          "[dcel][segment][offset][closed]") {
  Material material("mat");
  Lamina lamina("lam", &material, 0.04);
  LayerType layertype(1, &material, 0.0);
  Layup layup("layup");
  layup.addLayer(&lamina, 0.0, 1, &layertype);
  layup.addLayer(&lamina, 90.0, 1, &layertype);

  Baseline base("box", "line");
  PDCELVertex *rt = new PDCELVertex(0.0, 0.5, 0.5);
  PDCELVertex *lt = new PDCELVertex(0.0, -0.5, 0.5);
  PDCELVertex *lb = new PDCELVertex(0.0, -0.5, -0.5);
  PDCELVertex *rb = new PDCELVertex(0.0, 0.5, -0.5);
  base.addPVertex(rt);
  base.addPVertex(lt);
  base.addPVertex(lb);
  base.addPVertex(rb);
  base.addPVertex(rt);

  Segment segment("seg", &base, &layup, "left", 1);
  segment.offsetCurveBase();

  Baseline *offset = segment.curveOffset();
  REQUIRE(offset != nullptr);
  REQUIRE(offset->vertices().size() == 5);
  CHECK(offset->vertices().front() == offset->vertices().back());

  const double tol = 1e-9;
  CHECK(offset->vertices()[0]->point().y() == Catch::Approx(0.42).margin(tol));
  CHECK(offset->vertices()[0]->point().z() == Catch::Approx(0.42).margin(tol));
  CHECK(offset->vertices()[1]->point().y() == Catch::Approx(-0.42).margin(tol));
  CHECK(offset->vertices()[1]->point().z() == Catch::Approx(0.42).margin(tol));
  CHECK(offset->vertices()[2]->point().y() == Catch::Approx(-0.42).margin(tol));
  CHECK(offset->vertices()[2]->point().z() == Catch::Approx(-0.42).margin(tol));
  CHECK(offset->vertices()[3]->point().y() == Catch::Approx(0.42).margin(tol));
  CHECK(offset->vertices()[3]->point().z() == Catch::Approx(-0.42).margin(tol));
}

TEST_CASE("buildAreas: left-side open segment builds head and tail layer faces",
          "[dcel][segment][areas][head][tail]") {

  Material material("mat");
  Lamina lamina("lam", &material, 1.0);
  LayerType layertype(1, &material, 0.0);
  Layup layup("layup");
  layup.addLayer(&lamina, 0.0, 1, &layertype);
  layup.addLayer(&lamina, 0.0, 1, &layertype);
  layup.addLayer(&lamina, 0.0, 1, &layertype);

  Baseline base("base", "line");
  base.addPVertex(new PDCELVertex(0.0, 0.0, 0.0));
  base.addPVertex(new PDCELVertex(0.0, 1.0, 0.0));
  base.addPVertex(new PDCELVertex(0.0, 2.0, 0.0));

  Segment segment("seg", &base, &layup, "left", 1);
  segment.offsetCurveBase();

  PDCEL dcel;
  dcel.initialize();
  dcel.addEdge(base.vertices().front(), segment.curveOffset()->vertices().front());
  dcel.addEdge(base.vertices().back(), segment.curveOffset()->vertices().back());
  segment.setHeadVertexOffset(segment.curveOffset()->vertices().front());
  segment.setTailVertexOffset(segment.curveOffset()->vertices().back());

  PModel model;
  BuilderConfig bcfg{};
  bcfg.debug_level = DebugLevel::off;
  bcfg.tool = AnalysisTool::VABS;
  bcfg.tol = 1e-12;
  bcfg.geo_tol = 1e-9;
  bcfg.dcel = &dcel;
  bcfg.materials = &model;
  bcfg.model = &model;

  segment.build(bcfg);
  segment.buildAreas(bcfg);

  std::string face_names_left;
  for (PDCELFace *face : dcel.faces()) {
    const std::string &name = model.faceData(face).name;
    if (!name.empty()) {
      if (!face_names_left.empty()) {
        face_names_left += ", ";
      }
      face_names_left += name;
    }
  }
  INFO("left face names: " << face_names_left);

  for (const char *name : {
           "seg_area_1_layer_1",
           "seg_area_2_layer_1"}) {
    REQUIRE(findFaceByName(dcel, model, name) != nullptr);
  }
}

TEST_CASE("buildAreas: right-side open segment builds head and tail layer faces",
          "[dcel][segment][areas][head][tail]") {

  Material material("mat");
  Lamina lamina("lam", &material, 1.0);
  LayerType layertype(1, &material, 0.0);
  Layup layup("layup");
  layup.addLayer(&lamina, 0.0, 1, &layertype);
  layup.addLayer(&lamina, 0.0, 1, &layertype);
  layup.addLayer(&lamina, 0.0, 1, &layertype);

  Baseline base("base", "line");
  base.addPVertex(new PDCELVertex(0.0, 0.0, 0.0));
  base.addPVertex(new PDCELVertex(0.0, 1.0, 0.0));
  base.addPVertex(new PDCELVertex(0.0, 2.0, 0.0));

  Segment segment("seg", &base, &layup, "right", 1);
  segment.offsetCurveBase();

  PDCEL dcel;
  dcel.initialize();
  dcel.addEdge(base.vertices().front(), segment.curveOffset()->vertices().front());
  dcel.addEdge(base.vertices().back(), segment.curveOffset()->vertices().back());
  segment.setHeadVertexOffset(segment.curveOffset()->vertices().front());
  segment.setTailVertexOffset(segment.curveOffset()->vertices().back());

  PModel model;
  BuilderConfig bcfg{};
  bcfg.debug_level = DebugLevel::off;
  bcfg.tool = AnalysisTool::VABS;
  bcfg.tol = 1e-12;
  bcfg.geo_tol = 1e-9;
  bcfg.dcel = &dcel;
  bcfg.materials = &model;
  bcfg.model = &model;

  segment.build(bcfg);
  segment.buildAreas(bcfg);

  std::string face_names_right;
  for (PDCELFace *face : dcel.faces()) {
    const std::string &name = model.faceData(face).name;
    if (!name.empty()) {
      if (!face_names_right.empty()) {
        face_names_right += ", ";
      }
      face_names_right += name;
    }
  }
  INFO("right face names: " << face_names_right);

  for (const char *name : {
           "seg_area_1_layer_1",
           "seg_area_2_layer_1"}) {
    REQUIRE(findFaceByName(dcel, model, name) != nullptr);
  }
}

TEST_CASE("addEdge: source and target are consistent", "[dcel][edge]") {
  PDCEL dcel;
  PDCELVertex *v1 = dcel.addVertex(new PDCELVertex(0, 0.0, 0.0));
  PDCELVertex *v2 = dcel.addVertex(new PDCELVertex(0, 1.0, 0.0));

  PDCELHalfEdge *he = dcel.addEdge(v1, v2);
  REQUIRE(he != nullptr);

  // Forward half-edge: v1 -> v2
  CHECK(he->source() == v1);
  CHECK(he->target() == v2);

  // Reverse twin: v2 -> v1
  CHECK(he->twin()->source() == v2);
  CHECK(he->twin()->target() == v1);
}

TEST_CASE("addEdge: vertex incident edges are set", "[dcel][edge]") {
  PDCEL dcel;
  PDCELVertex *v1 = dcel.addVertex(new PDCELVertex(0, 0.0, 0.0));
  PDCELVertex *v2 = dcel.addVertex(new PDCELVertex(0, 1.0, 0.0));
  PDCELHalfEdge *he = dcel.addEdge(v1, v2);

  // Each vertex must have an incident edge assigned
  CHECK(v1->edge() != nullptr);
  CHECK(v2->edge() != nullptr);
  (void)he;
}


// ==================================================================
// 3. findHalfEdgeBetween / findHalfEdgeInFace
// ==================================================================

TEST_CASE("findHalfEdgeBetween(v1,v2): returns the half-edge from v1 to v2",
          "[dcel][edge]") {
  PDCEL dcel;
  PDCELVertex *v1 = dcel.addVertex(new PDCELVertex(0, 0.0, 0.0));
  PDCELVertex *v2 = dcel.addVertex(new PDCELVertex(0, 1.0, 0.0));
  PDCELHalfEdge *he = dcel.addEdge(v1, v2);

  PDCELHalfEdge *found_fwd = dcel.findHalfEdgeBetween(v1, v2);
  PDCELHalfEdge *found_rev = dcel.findHalfEdgeBetween(v2, v1);

  CHECK(found_fwd == he);
  CHECK(found_rev == he->twin());
}

TEST_CASE("findHalfEdgeBetween(v1,v2): returns nullptr when no edge exists",
          "[dcel][edge]") {
  PDCEL dcel;
  PDCELVertex *v1 = dcel.addVertex(new PDCELVertex(0, 0.0, 0.0));
  PDCELVertex *v2 = dcel.addVertex(new PDCELVertex(0, 1.0, 0.0));
  // No edge added

  CHECK(dcel.findHalfEdgeBetween(v1, v2) == nullptr);
  CHECK(dcel.findHalfEdgeBetween(v2, v1) == nullptr);
}

TEST_CASE("findHalfEdgeBetween(v1,v2): returns nullptr for isolated vertex",
          "[dcel][edge]") {
  // Fix: previously, calling findHalfEdgeBetween on a vertex with no incident
  // edge would only check degree, not guard the traversal loop.
  PDCEL dcel;
  PDCELVertex *v1 = dcel.addVertex(new PDCELVertex(0, 0.0, 0.0));
  PDCELVertex *v2 = dcel.addVertex(new PDCELVertex(0, 1.0, 0.0));
  // v1 has no incident edge
  CHECK(dcel.findHalfEdgeBetween(v1, v2) == nullptr);
}


// ==================================================================
// 4. removeEdge — vertex collapse
// ==================================================================

TEST_CASE("removeEdge: removes both half-edges", "[dcel][edge]") {
  PDCEL dcel;
  PDCELVertex *v1 = dcel.addVertex(new PDCELVertex(0, 0.0, 0.0));
  PDCELVertex *v2 = dcel.addVertex(new PDCELVertex(0, 1.0, 0.0));
  PDCELHalfEdge *he = dcel.addEdge(v1, v2);

  REQUIRE(dcel.halfedges().size() == 2);
  dcel.removeEdge(he);

  // Both he and he->twin() are deleted and removed from the list
  CHECK(dcel.halfedges().size() == 0);
}

TEST_CASE("removeEdge: target vertex is merged into source", "[dcel][edge]") {
  PDCEL dcel;
  PDCELVertex *v1 = dcel.addVertex(new PDCELVertex(0, 0.0, 0.0));
  PDCELVertex *v2 = dcel.addVertex(new PDCELVertex(0, 1.0, 0.0));
  PDCELHalfEdge *he = dcel.addEdge(v1, v2);

  REQUIRE(dcel.vertices().size() == 2);
  dcel.removeEdge(he);

  // v2 (target) is merged into v1 (source) and deleted
  CHECK(dcel.vertices().size() == 1);
  CHECK(dcel.vertices().front() == v1);
}


// ==================================================================
// 5. splitEdge
// ==================================================================

TEST_CASE("splitEdge: doubles half-edge count and adds midpoint vertex",
          "[dcel][splitEdge]") {
  PDCEL dcel;
  PDCELVertex *v1 = dcel.addVertex(new PDCELVertex(0, 0.0, 0.0));
  PDCELVertex *v2 = dcel.addVertex(new PDCELVertex(0, 2.0, 0.0));
  PDCELHalfEdge *he = dcel.addEdge(v1, v2);

  REQUIRE(dcel.halfedges().size() == 2);

  // v_mid owns a new heap allocation; splitEdge may replace it with an
  // existing coincident vertex (it won't here since (1,0) is new).
  PDCELVertex *v_mid = new PDCELVertex(0, 1.0, 0.0);
  v_mid = dcel.splitEdge(he, v_mid);
  // he is now a dangling pointer (deleted inside splitEdge); do not use it.

  CHECK(dcel.halfedges().size() == 4);
  CHECK(dcel.vertices().size() == 3);
}

TEST_CASE("splitEdge: sub-edges connect through the midpoint vertex",
          "[dcel][splitEdge]") {
  PDCEL dcel;
  PDCELVertex *v1 = dcel.addVertex(new PDCELVertex(0, 0.0, 0.0));
  PDCELVertex *v2 = dcel.addVertex(new PDCELVertex(0, 2.0, 0.0));
  PDCELHalfEdge *he = dcel.addEdge(v1, v2);

  PDCELVertex *v_mid = new PDCELVertex(0, 1.0, 0.0);
  v_mid = dcel.splitEdge(he, v_mid);

  // v_mid is now the canonical vertex for (0, 1.0, 0.0)
  PDCELHalfEdge *he1m = dcel.findHalfEdgeBetween(v1, v_mid);
  PDCELHalfEdge *hem2 = dcel.findHalfEdgeBetween(v_mid, v2);

  REQUIRE(he1m != nullptr);
  REQUIRE(hem2 != nullptr);

  CHECK(he1m->source() == v1);
  CHECK(he1m->target() == v_mid);
  CHECK(hem2->source() == v_mid);
  CHECK(hem2->target() == v2);
}

TEST_CASE("splitEdge: midpoint coincident with existing vertex uses canonical",
          "[dcel][splitEdge]") {
  PDCEL dcel;
  PDCELVertex *v1   = dcel.addVertex(new PDCELVertex(0, 0.0, 0.0));
  PDCELVertex *v2   = dcel.addVertex(new PDCELVertex(0, 2.0, 0.0));
  PDCELVertex *vexist = dcel.addVertex(new PDCELVertex(0, 1.0, 0.0));
  PDCELHalfEdge *he = dcel.addEdge(v1, v2);

  // Provide a NEW heap allocation at the same position as vexist.
  // splitEdge must substitute the canonical vexist and delete the new one.
  PDCELVertex *v_mid = new PDCELVertex(0, 1.0, 0.0);
  v_mid = dcel.splitEdge(he, v_mid);
  // v_mid now points to vexist (the canonical vertex)

  CHECK(v_mid == vexist);
  CHECK(dcel.vertices().size() == 3); // no new vertex created
}


// ==================================================================
// 6. validate() — structural invariants
// ==================================================================

TEST_CASE("validate: passes on freshly initialized DCEL", "[dcel][validate]") {
  PDCEL dcel;
  dcel.initialize();
  CHECK(dcel.validate());
}

TEST_CASE("validate: passes after addFace with a triangular face",
          "[dcel][validate]") {
  PDCEL dcel;
  dcel.initialize();

  // The background (unbounded) face is the first face after initialize().
  // Pass nullptr so addFace inserts into the background face automatically.
  PDCELVertex *v1 = new PDCELVertex(0, 1.0, 1.0);
  PDCELVertex *v2 = new PDCELVertex(0, 3.0, 1.0);
  PDCELVertex *v3 = new PDCELVertex(0, 2.0, 3.0);

  std::list<PDCELVertex *> vloop = {v1, v2, v3, v1};
  PDCELFace *f = dcel.addFace(vloop);

  REQUIRE(f != nullptr);
  CHECK(dcel.faces().size() == 2); // background + triangle
  CHECK(dcel.validate());
}

TEST_CASE("validate: passes after addFace with a rectangular face",
          "[dcel][validate]") {
  PDCEL dcel;
  dcel.initialize();

  PDCELVertex *v1 = new PDCELVertex(0, 1.0, 1.0);
  PDCELVertex *v2 = new PDCELVertex(0, 3.0, 1.0);
  PDCELVertex *v3 = new PDCELVertex(0, 3.0, 3.0);
  PDCELVertex *v4 = new PDCELVertex(0, 1.0, 3.0);

  std::list<PDCELVertex *> vloop = {v1, v2, v3, v4, v1};
  PDCELFace *f = dcel.addFace(vloop);

  REQUIRE(f != nullptr);
  CHECK(dcel.validate());
}

TEST_CASE("validate: fails when an isolated vertex exists", "[dcel][validate]") {
  PDCEL dcel;
  // addVertex without addEdge leaves the vertex with no incident edge.
  dcel.addVertex(new PDCELVertex(0, 5.0, 5.0));
  CHECK(!dcel.validate());
}

TEST_CASE("validate: fails on self-loop half-edge (source == target)",
          "[dcel][validate]") {
  PDCEL dcel;
  dcel.initialize();

  PDCELVertex *v1 = dcel.addVertex(new PDCELVertex(0, 1.0, 0.0));
  PDCELVertex *v2 = dcel.addVertex(new PDCELVertex(0, 3.0, 0.0));
  PDCELVertex *v3 = dcel.addVertex(new PDCELVertex(0, 2.0, 3.0));
  std::list<PDCELVertex *> vloop = {v1, v2, v3, v1};
  dcel.addFace(vloop);

  REQUIRE(dcel.validate());  // sanity: starts valid

  // Corrupt: redirect the source of he(v1→v2) to v2 → source == target.
  PDCELHalfEdge *he = dcel.findHalfEdgeBetween(v1, v2);
  REQUIRE(he != nullptr);
  he->setSource(he->target());

  CHECK(!dcel.validate());
}

TEST_CASE("validate: fails when a multi-edge exists (v1→v2 duplicated)",
          "[dcel][validate]") {
  PDCEL dcel;
  // Create two edges between the same pair of vertices.
  // addEdge does not deduplicate; calling it twice creates a multi-edge.
  PDCELVertex *v1 = dcel.addVertex(new PDCELVertex(0, 0.0, 0.0));
  PDCELVertex *v2 = dcel.addVertex(new PDCELVertex(0, 1.0, 0.0));
  dcel.addEdge(v1, v2);
  dcel.addEdge(v1, v2);  // duplicate

  CHECK(!dcel.validate());
}

TEST_CASE("validate: fails when a half-edge has a null incident face (orphaned)",
          "[dcel][validate]") {
  PDCEL dcel;
  dcel.initialize();

  PDCELVertex *v1 = dcel.addVertex(new PDCELVertex(0, 1.0, 0.0));
  PDCELVertex *v2 = dcel.addVertex(new PDCELVertex(0, 3.0, 0.0));
  PDCELVertex *v3 = dcel.addVertex(new PDCELVertex(0, 2.0, 3.0));
  std::list<PDCELVertex *> vloop = {v1, v2, v3, v1};
  dcel.addFace(vloop);

  REQUIRE(dcel.validate());  // sanity: starts valid

  // Corrupt: clear the incident face of one boundary half-edge.
  PDCELHalfEdge *he = dcel.findHalfEdgeBetween(v1, v2);
  REQUIRE(he != nullptr);
  he->setIncidentFace(nullptr);

  CHECK(!dcel.validate());
}


// ==================================================================
// 7. Face operations
// ==================================================================

TEST_CASE("addFace(vloop): face count increases by one", "[dcel][face]") {
  PDCEL dcel;
  dcel.initialize();
  std::size_t before = dcel.faces().size(); // 1 background face

  PDCELVertex *v1 = new PDCELVertex(0, 1.0, 1.0);
  PDCELVertex *v2 = new PDCELVertex(0, 3.0, 1.0);
  PDCELVertex *v3 = new PDCELVertex(0, 2.0, 3.0);
  std::list<PDCELVertex *> vloop = {v1, v2, v3, v1};

  dcel.addFace(vloop);

  CHECK(dcel.faces().size() == before + 1);
}

TEST_CASE("splitFace: original face is deleted; two new faces created",
          "[dcel][face]") {
  // splitFace requires the face to have a PDCELHalfEdgeLoop on its outer
  // boundary.  addFace(vloop) creates the face and wires the half-edges but
  // does NOT create loops — addHalfEdgeLoop must be called explicitly.
  PDCEL dcel;
  dcel.initialize();

  // Build a square face
  PDCELVertex *v1 = new PDCELVertex(0, 0.0, 0.0);
  PDCELVertex *v2 = new PDCELVertex(0, 4.0, 0.0);
  PDCELVertex *v3 = new PDCELVertex(0, 4.0, 4.0);
  PDCELVertex *v4 = new PDCELVertex(0, 0.0, 4.0);

  std::list<PDCELVertex *> vloop = {v1, v2, v3, v4, v1};
  PDCELFace *f_square = dcel.addFace(vloop);
  REQUIRE(f_square != nullptr);
  REQUIRE(f_square->outer() != nullptr);

  // addFace(vloop) wires the next/prev chain but does not create a loop.
  // splitFace(f, v1, v2) requires f->outer()->loop() != nullptr.
  PDCELHalfEdgeLoop *hel = dcel.addHalfEdgeLoop(f_square->outer());
  REQUIRE(hel != nullptr);

  std::size_t before = dcel.faces().size(); // background + square = 2

  // Split two opposite edges of the square to obtain the split endpoints
  PDCELVertex *v_a = new PDCELVertex(0, 2.0, 0.0); // midpoint of v1->v2
  PDCELVertex *v_b = new PDCELVertex(0, 2.0, 4.0); // midpoint of v4->v3
  v_a = dcel.addVertex(v_a);
  v_b = dcel.addVertex(v_b);

  PDCELHalfEdge *he_v1v2 = dcel.findHalfEdgeBetween(v1, v2);
  REQUIRE(he_v1v2 != nullptr);
  v_a = dcel.splitEdge(he_v1v2, v_a);

  PDCELHalfEdge *he_v4v3 = dcel.findHalfEdgeBetween(v4, v3);
  REQUIRE(he_v4v3 != nullptr);
  v_b = dcel.splitEdge(he_v4v3, v_b);

  // Split the square along the v_a -- v_b chord
  std::list<PDCELFace *> new_faces = dcel.splitFace(f_square, v_a, v_b);
  // f_square is now a dangling pointer — do not use it.

  CHECK(new_faces.size() == 2);
  CHECK(dcel.faces().size() == before + 1); // one face replaced by two
  CHECK(dcel.validate());
}


// ==================================================================
// 8. addHalfEdgeLoop
// ==================================================================

TEST_CASE("addHalfEdgeLoop: loop is created and incident edge set",
          "[dcel][loop]") {
  PDCEL dcel;
  // Build a triangle without a containing face (just topology)
  PDCELVertex *v1 = dcel.addVertex(new PDCELVertex(0, 0.0, 0.0));
  PDCELVertex *v2 = dcel.addVertex(new PDCELVertex(0, 2.0, 0.0));
  PDCELVertex *v3 = dcel.addVertex(new PDCELVertex(0, 1.0, 2.0));

  PDCELHalfEdge *he12 = dcel.addEdge(v1, v2);
  PDCELHalfEdge *he23 = dcel.addEdge(v2, v3);
  PDCELHalfEdge *he31 = dcel.addEdge(v3, v1);

  // Wire the three half-edges into a closed next/prev cycle manually.
  he12->setNext(he23); he23->setPrev(he12);
  he23->setNext(he31); he31->setPrev(he23);
  he31->setNext(he12); he12->setPrev(he31);

  PDCELHalfEdgeLoop *hel = dcel.addHalfEdgeLoop(he12);

  REQUIRE(hel != nullptr);
  CHECK(hel->incidentEdge() != nullptr);
  CHECK(dcel.halfedgeloops().size() == 1);

  // All three half-edges reference the same loop
  CHECK(he12->loop() == hel);
  CHECK(he23->loop() == hel);
  CHECK(he31->loop() == hel);
}

TEST_CASE("DCEL ids are assigned and exposed in printable labels",
          "[dcel][id]") {
  PDCEL dcel;

  PDCELVertex *v1 = dcel.addVertex(new PDCELVertex(0, 0.0, 0.0));
  PDCELVertex *v2 = dcel.addVertex(new PDCELVertex(0, 1.0, 0.0));
  PDCELHalfEdge *he12 = dcel.addEdge(v1, v2);

  REQUIRE(v1->id() > 0);
  REQUIRE(v2->id() > 0);
  REQUIRE(he12->id() > 0);
  REQUIRE(he12->twin()->id() > 0);

  CHECK(v1->label() == "v#" + std::to_string(v1->id()));
  CHECK(he12->label() == "he#" + std::to_string(he12->id()));
  CHECK(v1->printString().find(v1->label()) != std::string::npos);
  CHECK(he12->printString().find(he12->label()) != std::string::npos);
}

TEST_CASE("DCEL vertex and half-edge ids grow monotonically across splitEdge",
          "[dcel][id]") {
  PDCEL dcel;

  PDCELVertex *v1 = dcel.addVertex(new PDCELVertex(0, 0.0, 0.0));
  PDCELVertex *v2 = dcel.addVertex(new PDCELVertex(0, 2.0, 0.0));
  PDCELHalfEdge *he12 = dcel.addEdge(v1, v2);
  const unsigned int first_edge_id = he12->id();
  const unsigned int first_twin_id = he12->twin()->id();
  const unsigned int first_vertex_id = v2->id();

  PDCELVertex *vmid = new PDCELVertex(0, 1.0, 0.0);
  vmid = dcel.splitEdge(he12, vmid);
  REQUIRE(vmid != nullptr);
  CHECK(vmid->id() > first_vertex_id);

  unsigned int max_split_edge_id = 0;
  for (PDCELHalfEdge *he : dcel.halfedges()) {
    if (he->id() > max_split_edge_id) {
      max_split_edge_id = he->id();
    }
  }
  CHECK(max_split_edge_id > first_twin_id);
  CHECK(vmid->id() != v1->id());
  CHECK(vmid->id() != v2->id());

  PDCELHalfEdge *he_mid_v2 = dcel.findHalfEdgeBetween(vmid, v2);
  REQUIRE(he_mid_v2 != nullptr);

  PDCELVertex *vquarter = new PDCELVertex(0, 1.5, 0.0);
  vquarter = dcel.splitEdge(he_mid_v2, vquarter);
  REQUIRE(vquarter != nullptr);
  CHECK(vquarter->id() > vmid->id());

  unsigned int max_second_split_edge_id = 0;
  for (PDCELHalfEdge *he : dcel.halfedges()) {
    if (he->id() > max_second_split_edge_id) {
      max_second_split_edge_id = he->id();
    }
  }
  CHECK(max_second_split_edge_id > max_split_edge_id);
  CHECK(max_second_split_edge_id != first_edge_id);
  CHECK(max_second_split_edge_id != first_twin_id);
}

TEST_CASE("DCEL half-edge ids are not reused after removeEdge",
          "[dcel][id]") {
  PDCEL dcel;

  PDCELVertex *v1 = dcel.addVertex(new PDCELVertex(0, 0.0, 0.0));
  PDCELVertex *v2 = dcel.addVertex(new PDCELVertex(0, 2.0, 0.0));
  PDCELHalfEdge *he12 = dcel.addEdge(v1, v2);
  REQUIRE(he12 != nullptr);

  const unsigned int first_edge_id = he12->id();
  const unsigned int first_twin_id = he12->twin()->id();

  dcel.removeEdge(he12);
  CHECK(dcel.halfedges().empty());
  CHECK(dcel.vertices().size() == 1);

  PDCELVertex *v3 = dcel.addVertex(new PDCELVertex(0, 3.0, 0.0));
  PDCELHalfEdge *he_new = dcel.addEdge(v1, v3);
  REQUIRE(he_new != nullptr);
  CHECK(he_new->id() > first_twin_id);
  CHECK(he_new->twin()->id() > he_new->id());
  CHECK(he_new->id() != first_edge_id);
  CHECK(he_new->twin()->id() != first_twin_id);
}

TEST_CASE("PDCELHalfEdge::printString includes mirrored face name",
          "[dcel][id][face-name]") {
  PDCEL dcel;
  PModel model;

  PDCELVertex *v1 = dcel.addVertex(new PDCELVertex(0, 0.0, 0.0));
  PDCELVertex *v2 = dcel.addVertex(new PDCELVertex(0, 1.0, 0.0));
  PDCELVertex *v3 = dcel.addVertex(new PDCELVertex(0, 0.0, 1.0));

  PDCELHalfEdge *he12 = dcel.addEdge(v1, v2);
  PDCELHalfEdge *he23 = dcel.addEdge(v2, v3);
  PDCELHalfEdge *he31 = dcel.addEdge(v3, v1);

  he12->setNext(he23); he23->setPrev(he12);
  he23->setNext(he31); he31->setPrev(he23);
  he31->setNext(he12); he12->setPrev(he31);

  PDCELHalfEdgeLoop *loop = dcel.addHalfEdgeLoop(he12);
  REQUIRE(loop != nullptr);

  PDCELFace *face = dcel.addFace(loop);
  REQUIRE(face != nullptr);
  model.setFaceName(face, "tri_face");

  const std::string s = he12->printString();
  CHECK(s.find(face->label()) != std::string::npos);
  CHECK(s.find("tri_face") != std::string::npos);
}

TEST_CASE("PDCEL::dumpToFile includes background half-edges",
          "[dcel][dump]") {
  PDCEL dcel;
  dcel.initialize();

  REQUIRE_FALSE(dcel.halfedges().empty());
  PDCELHalfEdge *active = dcel.halfedges().front();
  REQUIRE(active != nullptr);
  REQUIRE(active->twin() != nullptr);

  const std::string dump_path =
      "C:\\tmp\\prevabs_test_dcel_dump_background.txt";
  dcel.dumpToFile(dump_path);

  std::ifstream ifs(dump_path.c_str());
  REQUIRE(ifs.is_open());

  const std::string dump_text((std::istreambuf_iterator<char>(ifs)),
                              std::istreambuf_iterator<char>());
  ifs.close();
  std::remove(dump_path.c_str());

  CHECK(dump_text.find("HALFEDGES (8):") != std::string::npos);
  CHECK(dump_text.find(active->label()) != std::string::npos);
  CHECK(dump_text.find(active->twin()->label()) != std::string::npos);
  CHECK(dump_text.find("scope=active") != std::string::npos);
  CHECK(dump_text.find("scope=background") != std::string::npos);
}

// ==================================================================
// Loop guard tests — walkLoopWithLimit and inline DCEL guards
// ==================================================================

// Helper: build a minimal 3-half-edge closed cycle with twins so that
// printString() is safe.  Returns the three forward half-edges.
static void makeClosedTriangle(
    PDCELVertex &v,
    PDCELHalfEdge &he0, PDCELHalfEdge &he1, PDCELHalfEdge &he2,
    PDCELHalfEdge &t0,  PDCELHalfEdge &t1,  PDCELHalfEdge &t2)
{
  // All sources and twin-sources point to the same dummy vertex — enough for
  // printString() to not dereference nullptr.
  he0.setSource(&v); t0.setSource(&v); he0.setTwin(&t0); t0.setTwin(&he0);
  he1.setSource(&v); t1.setSource(&v); he1.setTwin(&t1); t1.setTwin(&he1);
  he2.setSource(&v); t2.setSource(&v); he2.setTwin(&t2); t2.setTwin(&he2);
  // Closed cycle: he0 -> he1 -> he2 -> he0
  he0.setNext(&he1); he1.setNext(&he2); he2.setNext(&he0);
}

TEST_CASE("walkLoopWithLimit: valid 3-edge cycle visits all edges", "[dcel][error]") {
  PDCELVertex v;
  PDCELHalfEdge he0, he1, he2, t0, t1, t2;
  makeClosedTriangle(v, he0, he1, he2, t0, t1, t2);

  int count = 0;
  walkLoopWithLimit(&he0, [&count](PDCELHalfEdge *) { ++count; });
  REQUIRE(count == 3);
}

TEST_CASE("walkLoopWithLimit: broken cycle (revisits middle) throws", "[dcel][error]") {
  using Catch::Matchers::ContainsSubstring;

  PDCELVertex v;
  PDCELHalfEdge he0, he1, he2, t0, t1, t2;
  // Set up sources and twins so printString() is safe.
  he0.setSource(&v); t0.setSource(&v); he0.setTwin(&t0); t0.setTwin(&he0);
  he1.setSource(&v); t1.setSource(&v); he1.setTwin(&t1); t1.setTwin(&he1);
  he2.setSource(&v); t2.setSource(&v); he2.setTwin(&t2); t2.setTwin(&he2);
  // Broken cycle: he0 -> he1 -> he2 -> he1 (never returns to he0).
  he0.setNext(&he1); he1.setNext(&he2); he2.setNext(&he1);

  // Use a small limit so the test completes quickly.
  CHECK_THROWS_WITH(
      walkLoopWithLimit(&he0, [](PDCELHalfEdge *) {}, 10),
      ContainsSubstring("DCEL loop walk exceeded"));
}

TEST_CASE("PDCELFace::getOuterHalfEdgeWithSource throws on broken cycle",
          "[dcel][error]") {
  using Catch::Matchers::ContainsSubstring;

  PDCELVertex v, vtarget;
  PDCELHalfEdge he0, he1, he2, t0, t1, t2;
  he0.setSource(&v); t0.setSource(&v); he0.setTwin(&t0); t0.setTwin(&he0);
  he1.setSource(&v); t1.setSource(&v); he1.setTwin(&t1); t1.setTwin(&he1);
  he2.setSource(&v); t2.setSource(&v); he2.setTwin(&t2); t2.setTwin(&he2);
  // Broken cycle: he0 -> he1 -> he2 -> he1 (never returns to he0).
  he0.setNext(&he1); he1.setNext(&he2); he2.setNext(&he1);

  PDCELFace face(&he0);
  // vtarget is not in the chain, so the loop would run until the guard fires.
  CHECK_THROWS_WITH(
      face.getOuterHalfEdgeWithSource(&vtarget),
      ContainsSubstring("DCEL loop walk exceeded"));
}

// ==================================================================
// walkLoopWithLimit logging — one start debug log plus one warning when
// the walk exceeds kDCELWarnLoopSteps.
// ==================================================================

// Sink that counts debug/warn messages received.
struct CountingDebugSink : public spdlog::sinks::base_sink<std::mutex> {
  size_t debug_count = 0;
  size_t warning_count = 0;
  std::vector<std::string> messages;
protected:
  void sink_it_(const spdlog::details::log_msg &msg) override {
    if (msg.level == spdlog::level::debug) ++debug_count;
    if (msg.level == spdlog::level::warn) ++warning_count;
    messages.emplace_back(msg.payload.data(), msg.payload.size());
  }
  void flush_() override {}
};

struct CapturingInfoSink : public spdlog::sinks::base_sink<std::mutex> {
  std::vector<std::string> messages;
protected:
  void sink_it_(const spdlog::details::log_msg &msg) override {
    if (msg.level == spdlog::level::info) {
      messages.emplace_back(msg.payload.data(), msg.payload.size());
    }
  }
  void flush_() override {}
};

TEST_CASE("offsetCurveBase logs cusp-like closed baseline hint",
          "[dcel][segment][offset][cusp]") {
  auto sink = std::make_shared<CapturingInfoSink>();
  spdlog::drop("prevabs");
  {
    auto logger = std::make_shared<spdlog::logger>("prevabs", sink);
    logger->set_level(spdlog::level::info);
    spdlog::register_logger(logger);
  }

  Material material("mat");
  Lamina lamina("lam", &material, 0.1);
  LayerType layertype(1, &material, 0.0);
  Layup layup("layup");
  layup.addLayer(&lamina, 0.0, 1, &layertype);

  Baseline base("cusp", "line");
  PDCELVertex *v0 = new PDCELVertex(0.0, 0.0, 0.0);
  PDCELVertex *v1 = new PDCELVertex(0.0, 1.0, 0.0);
  PDCELVertex *v2 = new PDCELVertex(0.0, 0.1, 0.05);
  base.addPVertex(v0);
  base.addPVertex(v1);
  base.addPVertex(v2);
  base.addPVertex(v0);

  Segment segment("seg", &base, &layup, "left", 1);
  segment.offsetCurveBase();
  REQUIRE(segment.curveOffset() != nullptr);

  bool saw_cusp_log = false;
  for (const std::string &message : sink->messages) {
    if (message.find("cusp-like minimum interior angle") != std::string::npos
        && message.find("segment 'seg'") != std::string::npos) {
      saw_cusp_log = true;
      break;
    }
  }
  CHECK(saw_cusp_log);

  spdlog::drop("prevabs");
  {
    auto orig_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    auto logger = std::make_shared<spdlog::logger>("prevabs", orig_sink);
    logger->set_level(spdlog::level::warn);
    spdlog::register_logger(logger);
  }
}

TEST_CASE("walkLoopWithLimit: logs start once and warns once on long walks",
          "[dcel][error]") {
  const DebugLevel previous_debug_level = config.debug_level;
  config.debug_level = DebugLevel::geo;

  // Install a debug-level counting logger, replacing the default warn logger.
  auto sink = std::make_shared<CountingDebugSink>();
  spdlog::drop("prevabs");
  {
    auto logger = std::make_shared<spdlog::logger>("prevabs", sink);
    logger->set_level(spdlog::level::debug);
    spdlog::register_logger(logger);
  }

  // Build a 300-step closed cycle.
  const int N = 300;
  PDCELVertex v;
  std::vector<PDCELHalfEdge> chain(N), twins(N);
  for (int i = 0; i < N; ++i) {
    chain[i].setSource(&v);
    twins[i].setSource(&v);
    chain[i].setTwin(&twins[i]);
    twins[i].setTwin(&chain[i]);
    chain[i].setNext(&chain[(i + 1) % N]);
  }

  walkLoopWithLimit(&chain[0], [](PDCELHalfEdge *) {});

  // Restore the warn-level logger used by all other tests.
  spdlog::drop("prevabs");
  {
    auto orig_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    auto logger = std::make_shared<spdlog::logger>("prevabs", orig_sink);
    logger->set_level(spdlog::level::warn);
    spdlog::register_logger(logger);
  }
  config.debug_level = previous_debug_level;

  REQUIRE(sink->debug_count == 1);
  REQUIRE(sink->warning_count == 1);

  bool saw_start = false;
  bool saw_warning = false;
  for (const std::string &message : sink->messages) {
    if (message.find("walkLoopWithLimit: start") != std::string::npos &&
        message.find("step=0") != std::string::npos) {
      saw_start = true;
    }
    if (message.find("walkLoopWithLimit: unusually long loop walk") !=
            std::string::npos &&
        message.find("current=he#") != std::string::npos) {
      saw_warning = true;
    }
  }
  CHECK(saw_start);
  CHECK(saw_warning);
}
