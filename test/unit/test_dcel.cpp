// test_dcel.cpp
// Unit tests for PDCEL — doubly-connected edge list.
//
// Compiled against the DCEL source files directly.  No Gmsh is required.
// Global variables normally defined in src/main.cpp are provided here.

#include "catch_amalgamated.hpp"

// ------------------------------------------------------------------
// Global variable definitions (normally in src/main.cpp).
// These must appear exactly once in the test executable.
// ------------------------------------------------------------------
#include "globalVariables.hpp"
#include "utilities.hpp"

bool         debug             = false;
int          i_indent          = 0;
bool         scientific_format = false;
PConfig      config;
RuntimeState runtime;
// g_msg is used only by fixGeometry / intersect / offset (not tested here).
// PLOG() silently drops messages when no "prevabs" spdlog logger is registered,
// so it is safe to leave g_msg nullptr for these tests.
Message*     g_msg             = nullptr;

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
#include "PDCELVertex.hpp"

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
